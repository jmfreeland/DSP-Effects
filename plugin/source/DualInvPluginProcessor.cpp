#include "DualInvPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/DualInvSchema.h"

#include <span>

namespace
{
constexpr auto kSendsId = "sends";
constexpr auto kReturnsId = "returns";
constexpr auto kRoutingId = "routing";
constexpr auto kRvbInLevelId = "rvbInLevel";
constexpr auto kFxInLevelId = "fxInLevel";
constexpr auto kRvbMixId = "rvbMix";
constexpr auto kFxMixId = "fxMix";

constexpr auto kDurationId = "duration";
constexpr auto kLowSlopeId = "lowSlope";
constexpr auto kMidSlopeId = "midSlope";
constexpr auto kCrossoverId = "crossover";
constexpr auto kDampingId = "damping";
constexpr auto kDiffusionId = "diffusion";
constexpr auto kSizeId = "size";
constexpr auto kShapeId = "shape";
constexpr auto kRvbInId = "rvbIn";
constexpr auto kRvbOutId = "rvbOut";
constexpr auto kPreDelayId = "preDelay";
constexpr auto kEarlyReflectionLevelLeftId = "earlyReflectionLevelLeft";
constexpr auto kEarlyReflectionLevelRightId = "earlyReflectionLevelRight";
constexpr auto kEarlyReflectionDelayLeftId = "earlyReflectionDelayLeft";
constexpr auto kEarlyReflectionDelayRightId = "earlyReflectionDelayRight";
constexpr auto kSpinId = "spin";

constexpr auto kSpliceId = "splice";
constexpr auto kFxWidthId = "fxWidth";
constexpr auto kHiCutId = "hiCut";
constexpr auto kFxAdjustId = "fxAdjust";
constexpr auto kMixId = "mix";

const juce::StringArray kSendsNames { "Stereo", "L=Rvb, R=FX", "Mono", "L=FX, R=Rvb" };
const juce::StringArray kReturnsNames { "Stereo", "Rvb=L, FX=R", "Mono", "FX=L, Rvb=R" };
const juce::StringArray kRoutingNames { "Parallel", "Rvb into FX", "FX into Rvb" };

// One voice's five parameter IDs, e.g. "voice0Delay".
juce::String voiceParamId(int index, const char* suffix)
{
    return "voice" + juce::String(index) + suffix;
}

std::unique_ptr<juce::AudioParameterFloat> floatParam(const char* id, const juce::String& name,
                                                        float min, float max, float defaultValue,
                                                        const char* label = nullptr,
                                                        float skew = 1.0f)
{
    juce::AudioParameterFloatAttributes attributes;
    if (label != nullptr)
    {
        attributes = attributes.withLabel(label);
    }
    auto interval = (max - min) / 10000.0f;
    return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ id, 1 }, name, juce::NormalisableRange<float>(min, max, interval, skew),
      defaultValue, attributes);
}
}

DualInvAudioProcessor::DualInvAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float DualInvAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout DualInvAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // -- Submixer --
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kSendsId, 1 }, "Sends", kSendsNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kReturnsId, 1 }, "Returns", kReturnsNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kRoutingId, 1 }, "Routing", kRoutingNames, 0));
    params.push_back(floatParam(kRvbInLevelId, "Rvb In Lvl", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kFxInLevelId, "FX In Lvl", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kRvbMixId, "Rvb Mix", 0.0f, 1.0f, 0.8f));
    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 1.0f));

    // -- Inverse reverb block --
    params.push_back(floatParam(kDurationId, "Duration", 0.05f, 10.0f, 2.5f, "s", 0.5f));
    params.push_back(floatParam(kLowSlopeId, "Low Slope", -1.0f, 1.0f, -0.3f));
    params.push_back(floatParam(kMidSlopeId, "Mid Slope", -1.0f, 1.0f, -0.3f));
    params.push_back(floatParam(kCrossoverId, "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
    params.push_back(floatParam(kDampingId, "Damping", 0.0f, 1.0f, 0.4f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kShapeId, "Shape", 0.0f, 1.0f, 0.3f));
    params.push_back(floatParam(kRvbInId, "Rvb In", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kRvbOutId, "Rvb Out", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kPreDelayId, "Pre Delay", 0.0f, 0.93f, 0.0f, "s"));
    params.push_back(floatParam(kEarlyReflectionLevelLeftId, "Early Reflections L", 0.0f, 1.0f, 0.2f));
    params.push_back(floatParam(kEarlyReflectionLevelRightId, "Early Reflections R", 0.0f, 1.0f, 0.2f));
    params.push_back(
      floatParam(kEarlyReflectionDelayLeftId, "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
    params.push_back(
      floatParam(kEarlyReflectionDelayRightId, "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
    params.push_back(floatParam(kSpinId, "Spin", 0.0f, 1.0f, 0.5f));

    // -- Dual Shifter (2 voices) --
    static constexpr float kDefaultDelay[2] = { 0.02f, 0.03f };
    static constexpr float kDefaultCents[2] = { 12.0f, -12.0f };
    static constexpr float kDefaultPan[2] = { -0.7f, 0.7f };
    for (int i = 0; i < 2; ++i)
    {
        params.push_back(floatParam(voiceParamId(i, "Delay").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.25f,
                                     kDefaultDelay[static_cast<std::size_t>(i)], "s", 0.5f));
        params.push_back(floatParam(voiceParamId(i, "Cents").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pitch", -3600.0f, 3600.0f,
                                     kDefaultCents[static_cast<std::size_t>(i)], "cents"));
        params.push_back(floatParam(voiceParamId(i, "Level").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Level", 0.0f, 1.0f, 0.7f));
        params.push_back(floatParam(voiceParamId(i, "Pan").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                     kDefaultPan[static_cast<std::size_t>(i)]));
        params.push_back(floatParam(voiceParamId(i, "Feedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(voiceParamId(i, "CrossFeedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " X-Fbk", -1.0f, 1.0f, 0.0f));
    }

    params.push_back(floatParam(kSpliceId, "Splice", 0.001f, 0.05f, 0.004f, "s"));
    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 45.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

void DualInvAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::DualInvAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void DualInvAudioProcessor::releaseResources() {}

bool DualInvAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DualInvAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setSends(static_cast<dsp::Submixer::Sends>(static_cast<int>(paramValue(kSendsId))));
    engine_.setReturns(static_cast<dsp::Submixer::Returns>(static_cast<int>(paramValue(kReturnsId))));
    engine_.setRouting(
      static_cast<dsp::graphs::DualInvAlgorithm::Routing>(static_cast<int>(paramValue(kRoutingId))));
    engine_.setRvbInLevel(paramValue(kRvbInLevelId));
    engine_.setFxInLevel(paramValue(kFxInLevelId));
    engine_.setRvbMix(paramValue(kRvbMixId));
    engine_.setFxMix(paramValue(kFxMixId));

    engine_.setDuration(paramValue(kDurationId));
    engine_.setLowSlope(paramValue(kLowSlopeId));
    engine_.setMidSlope(paramValue(kMidSlopeId));
    engine_.setCrossoverFrequency(paramValue(kCrossoverId));
    engine_.setDamping(paramValue(kDampingId));
    engine_.setDiffusion(paramValue(kDiffusionId));
    engine_.setSize(paramValue(kSizeId));
    engine_.setShape(paramValue(kShapeId));
    engine_.setRvbIn(paramValue(kRvbInId));
    engine_.setRvbOut(paramValue(kRvbOutId));
    engine_.setPreDelaySeconds(paramValue(kPreDelayId));
    engine_.setEarlyReflectionLevel(paramValue(kEarlyReflectionLevelLeftId),
                                     paramValue(kEarlyReflectionLevelRightId));
    engine_.setEarlyReflectionDelaySeconds(paramValue(kEarlyReflectionDelayLeftId),
                                            paramValue(kEarlyReflectionDelayRightId));
    engine_.setSpin(paramValue(kSpinId));

    for (int i = 0; i < 2; ++i)
    {
        engine_.setVoice(i, paramValue(voiceParamId(i, "Delay").toRawUTF8()),
                          paramValue(voiceParamId(i, "Cents").toRawUTF8()),
                          paramValue(voiceParamId(i, "Level").toRawUTF8()),
                          paramValue(voiceParamId(i, "Pan").toRawUTF8()));
        engine_.setVoiceFeedback(i, paramValue(voiceParamId(i, "Feedback").toRawUTF8()),
                                  paramValue(voiceParamId(i, "CrossFeedback").toRawUTF8()));
    }
    engine_.setSpliceSeconds(paramValue(kSpliceId));

    engine_.setFxWidth(paramValue(kFxWidthId));
    engine_.setHiCut(paramValue(kHiCutId));
    engine_.setFxAdjustDb(paramValue(kFxAdjustId));
    engine_.setMix(paramValue(kMixId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* DualInvAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::dualInvSchema());
}

void DualInvAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void DualInvAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

// This creates new instances of the plugin.
// NOLINTNEXTLINE(readability-identifier-naming) - JUCE-mandated symbol name.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DualInvAudioProcessor();
}
