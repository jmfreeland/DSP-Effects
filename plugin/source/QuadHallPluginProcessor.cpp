#include "QuadHallPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/QuadHallSchema.h"

#include <span>

namespace
{
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";
constexpr auto kInPanLeftId = "inPanLeft";
constexpr auto kInPanRightId = "inPanRight";

constexpr auto kDecayId = "decay";
constexpr auto kLowRatioId = "lowRatio";
constexpr auto kCrossoverId = "crossover";
constexpr auto kDampingId = "damping";
constexpr auto kDiffusionId = "diffusion";
constexpr auto kSizeId = "size";
constexpr auto kLinkId = "link";
constexpr auto kDefinitionId = "definition";
constexpr auto kDepthId = "depth";
constexpr auto kRvbInId = "rvbIn";
constexpr auto kRvbOutId = "rvbOut";
constexpr auto kPreDelayId = "preDelay";
constexpr auto kEarlyReflectionLevelLeftId = "earlyReflectionLevelLeft";
constexpr auto kEarlyReflectionLevelRightId = "earlyReflectionLevelRight";
constexpr auto kEarlyReflectionDelayLeftId = "earlyReflectionDelayLeft";
constexpr auto kEarlyReflectionDelayRightId = "earlyReflectionDelayRight";
constexpr auto kSpinId = "spin";
constexpr auto kChorusId = "chorus";

constexpr auto kSpliceId = "splice";
constexpr auto kFxMixId = "fxMix";
constexpr auto kFxWidthId = "fxWidth";
constexpr auto kHiCutId = "hiCut";
constexpr auto kFxAdjustId = "fxAdjust";
constexpr auto kMixId = "mix";
constexpr auto kFreezeId = "freeze";

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

QuadHallAudioProcessor::QuadHallAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float QuadHallAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout QuadHallAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // -- Input conditioning --
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInPanLeftId, "In Pan L", -1.0f, 1.0f, -1.0f));
    params.push_back(floatParam(kInPanRightId, "In Pan R", -1.0f, 1.0f, 1.0f));

    // -- Concert Hall reverb core (fixed, in series) --
    params.push_back(floatParam(kDecayId, "Decay", 0.3f, 8.0f, 2.5f, "s", 0.5f));
    params.push_back(floatParam(kLowRatioId, "Low Ratio", 0.2f, 2.0f, 1.0f));
    params.push_back(floatParam(kCrossoverId, "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
    params.push_back(floatParam(kDampingId, "Damping", 0.0f, 1.0f, 0.4f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.6f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 0.5f));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kLinkId, 1 }, "Link", false));
    params.push_back(floatParam(kDefinitionId, "Definition", 0.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kDepthId, "Depth", 0.0f, 1.0f, 0.5f));
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
    params.push_back(floatParam(kChorusId, "Chorus", 0.0f, 1.0f, 0.3f));

    // -- Four voices (1-2 = Left shifts, 3-4 = Right shifts) --
    static constexpr float kDefaultDelay[4] = { 0.02f, 0.03f, 0.02f, 0.03f };
    static constexpr float kDefaultCents[4] = { 12.0f, -12.0f, 15.0f, -15.0f };
    static constexpr float kDefaultPan[4] = { -0.6f, -0.3f, 0.3f, 0.6f };
    for (int i = 0; i < 4; ++i)
    {
        params.push_back(floatParam(voiceParamId(i, "Delay").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.25f,
                                     kDefaultDelay[static_cast<std::size_t>(i)], "s", 0.5f));
        params.push_back(floatParam(voiceParamId(i, "Cents").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pitch", -3600.0f, 3600.0f,
                                     kDefaultCents[static_cast<std::size_t>(i)], "cents"));
        params.push_back(floatParam(voiceParamId(i, "Level").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Level", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(voiceParamId(i, "Pan").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                     kDefaultPan[static_cast<std::size_t>(i)]));
        params.push_back(floatParam(voiceParamId(i, "Feedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(voiceParamId(i, "CrossFeedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " X-Fbk", -1.0f, 1.0f, 0.0f));
    }

    params.push_back(floatParam(kSpliceId, "Splice", 0.001f, 0.05f, 0.004f, "s"));
    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 0.35f));
    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 45.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void QuadHallAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::QuadHallAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void QuadHallAudioProcessor::releaseResources() {}

bool QuadHallAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void QuadHallAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));
    engine_.setInPan(paramValue(kInPanLeftId), paramValue(kInPanRightId));

    engine_.setDecaySeconds(paramValue(kDecayId));
    engine_.setLowRatio(paramValue(kLowRatioId));
    engine_.setCrossoverFrequency(paramValue(kCrossoverId));
    engine_.setDamping(paramValue(kDampingId));
    engine_.setDiffusion(paramValue(kDiffusionId));
    engine_.setSize(paramValue(kSizeId));
    engine_.setLink(paramValue(kLinkId) >= 0.5f);
    engine_.setDefinition(paramValue(kDefinitionId));
    engine_.setDepth(paramValue(kDepthId));
    engine_.setRvbIn(paramValue(kRvbInId));
    engine_.setRvbOut(paramValue(kRvbOutId));
    engine_.setPreDelaySeconds(paramValue(kPreDelayId));
    engine_.setEarlyReflectionLevel(paramValue(kEarlyReflectionLevelLeftId),
                                     paramValue(kEarlyReflectionLevelRightId));
    engine_.setEarlyReflectionDelaySeconds(paramValue(kEarlyReflectionDelayLeftId),
                                            paramValue(kEarlyReflectionDelayRightId));
    engine_.setSpin(paramValue(kSpinId));
    engine_.setChorus(paramValue(kChorusId));

    for (int i = 0; i < 4; ++i)
    {
        engine_.setVoice(i, paramValue(voiceParamId(i, "Delay").toRawUTF8()),
                          paramValue(voiceParamId(i, "Cents").toRawUTF8()),
                          paramValue(voiceParamId(i, "Level").toRawUTF8()),
                          paramValue(voiceParamId(i, "Pan").toRawUTF8()));
        engine_.setVoiceFeedback(i, paramValue(voiceParamId(i, "Feedback").toRawUTF8()),
                                  paramValue(voiceParamId(i, "CrossFeedback").toRawUTF8()));
    }
    engine_.setSpliceSeconds(paramValue(kSpliceId));

    engine_.setFxMix(paramValue(kFxMixId));
    engine_.setFxWidth(paramValue(kFxWidthId));
    engine_.setHiCut(paramValue(kHiCutId));
    engine_.setFxAdjustDb(paramValue(kFxAdjustId));
    engine_.setMix(paramValue(kMixId));
    engine_.setFrozen(paramValue(kFreezeId) >= 0.5f);

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* QuadHallAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::quadHallSchema());
}

void QuadHallAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void QuadHallAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new QuadHallAudioProcessor();
}
