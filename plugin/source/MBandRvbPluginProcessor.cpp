#include "MBandRvbPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/MBandRvbSchema.h"

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
constexpr auto kShapeId = "shape";
constexpr auto kSpreadId = "spread";
constexpr auto kRvbOutId = "rvbOut";
constexpr auto kPreDelayId = "preDelay";
constexpr auto kEarlyReflectionLevelLeftId = "earlyReflectionLevelLeft";
constexpr auto kEarlyReflectionLevelRightId = "earlyReflectionLevelRight";
constexpr auto kEarlyReflectionDelayLeftId = "earlyReflectionDelayLeft";
constexpr auto kEarlyReflectionDelayRightId = "earlyReflectionDelayRight";
constexpr auto kEkoDelayLeftId = "ekoDelayLeft";
constexpr auto kEkoDelayRightId = "ekoDelayRight";
constexpr auto kEkoFeedbackLeftId = "ekoFeedbackLeft";
constexpr auto kEkoFeedbackRightId = "ekoFeedbackRight";
constexpr auto kSpinId = "spin";

constexpr auto kFxMixId = "fxMix";
constexpr auto kFxWidthId = "fxWidth";
constexpr auto kHiCutId = "hiCut";
constexpr auto kFxAdjustId = "fxAdjust";
constexpr auto kMixId = "mix";
constexpr auto kFreezeId = "freeze";

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

MBandRvbAudioProcessor::MBandRvbAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float MBandRvbAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout MBandRvbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInPanLeftId, "In Pan L", -1.0f, 1.0f, -1.0f));
    params.push_back(floatParam(kInPanRightId, "In Pan R", -1.0f, 1.0f, 1.0f));

    // -- Chamber reverb core (fixed, in parallel) --
    params.push_back(floatParam(kDecayId, "Decay", 0.3f, 8.0f, 2.5f, "s", 0.5f));
    params.push_back(floatParam(kLowRatioId, "Low Ratio", 0.2f, 2.0f, 1.0f));
    params.push_back(floatParam(kCrossoverId, "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
    params.push_back(floatParam(kDampingId, "Damping", 0.0f, 1.0f, 0.4f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 0.6f));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kLinkId, 1 }, "Link", false));
    params.push_back(floatParam(kShapeId, "Shape", 0.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kSpreadId, "Spread", 0.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kRvbOutId, "Rvb Out", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kPreDelayId, "Pre Delay", 0.0f, 0.93f, 0.0f, "s"));
    params.push_back(floatParam(kEarlyReflectionLevelLeftId, "Early Reflections L", 0.0f, 1.0f, 0.2f));
    params.push_back(floatParam(kEarlyReflectionLevelRightId, "Early Reflections R", 0.0f, 1.0f, 0.2f));
    params.push_back(
      floatParam(kEarlyReflectionDelayLeftId, "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
    params.push_back(
      floatParam(kEarlyReflectionDelayRightId, "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
    params.push_back(floatParam(kEkoDelayLeftId, "Eko Delay L", 0.0f, 1.2f, 0.0f, "s"));
    params.push_back(floatParam(kEkoDelayRightId, "Eko Delay R", 0.0f, 1.2f, 0.0f, "s"));
    params.push_back(floatParam(kEkoFeedbackLeftId, "Eko Feedback L", -1.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kEkoFeedbackRightId, "Eko Feedback R", -1.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kSpinId, "Spin", 0.0f, 1.0f, 0.5f));

    static constexpr float kDefaultDelay[6] = { 0.09f, 0.17f, 0.26f, 0.11f, 0.19f, 0.28f };
    static constexpr float kDefaultPan[6] = { -0.7f, -0.4f, -0.15f, 0.15f, 0.4f, 0.7f };
    static constexpr float kDefaultHiCut[6] = { 7000.0f, 5500.0f, 4000.0f, 7200.0f, 5700.0f, 4200.0f };
    static constexpr float kDefaultLoCut[6] = { 120.0f, 250.0f, 500.0f, 130.0f, 260.0f, 520.0f };
    for (int i = 0; i < 6; ++i)
    {
        params.push_back(floatParam(voiceParamId(i, "Delay").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Delay", 0.0f, 10.922f,
                                     kDefaultDelay[static_cast<std::size_t>(i)], "s", 0.4f));
        params.push_back(floatParam(voiceParamId(i, "Level").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.55f));
        params.push_back(floatParam(voiceParamId(i, "Pan").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                     kDefaultPan[static_cast<std::size_t>(i)]));
        params.push_back(floatParam(voiceParamId(i, "Feedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.25f));
        params.push_back(floatParam(voiceParamId(i, "HiCut").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Hi Cut", 200.0f, 20000.0f,
                                     kDefaultHiCut[static_cast<std::size_t>(i)], "Hz", 0.4f));
        params.push_back(floatParam(voiceParamId(i, "LoCut").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Lo Cut", 20.0f, 2000.0f,
                                     kDefaultLoCut[static_cast<std::size_t>(i)], "Hz", 0.4f));
    }

    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 45.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void MBandRvbAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::MBandRvbAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void MBandRvbAudioProcessor::releaseResources() {}

bool MBandRvbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MBandRvbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
    engine_.setShape(paramValue(kShapeId));
    engine_.setSpread(paramValue(kSpreadId));
    engine_.setRvbOut(paramValue(kRvbOutId));
    engine_.setPreDelaySeconds(paramValue(kPreDelayId));
    engine_.setEarlyReflectionLevel(paramValue(kEarlyReflectionLevelLeftId),
                                     paramValue(kEarlyReflectionLevelRightId));
    engine_.setEarlyReflectionDelaySeconds(paramValue(kEarlyReflectionDelayLeftId),
                                            paramValue(kEarlyReflectionDelayRightId));
    engine_.setEkoDelaySeconds(paramValue(kEkoDelayLeftId), paramValue(kEkoDelayRightId));
    engine_.setEkoFeedback(paramValue(kEkoFeedbackLeftId), paramValue(kEkoFeedbackRightId));
    engine_.setSpin(paramValue(kSpinId));

    for (int i = 0; i < 6; ++i)
    {
        engine_.setVoiceDelay(i, paramValue(voiceParamId(i, "Delay").toRawUTF8()));
        engine_.setVoiceLevel(i, paramValue(voiceParamId(i, "Level").toRawUTF8()));
        engine_.setVoicePan(i, paramValue(voiceParamId(i, "Pan").toRawUTF8()));
        engine_.setVoiceFeedback(i, paramValue(voiceParamId(i, "Feedback").toRawUTF8()));
        engine_.setVoiceHiCut(i, paramValue(voiceParamId(i, "HiCut").toRawUTF8()));
        engine_.setVoiceLoCut(i, paramValue(voiceParamId(i, "LoCut").toRawUTF8()));
    }

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

juce::AudioProcessorEditor* MBandRvbAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::mBandRvbSchema());
}

void MBandRvbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void MBandRvbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new MBandRvbAudioProcessor();
}
