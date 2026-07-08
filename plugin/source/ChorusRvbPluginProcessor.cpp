#include "ChorusRvbPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/ChorusRvbSchema.h"

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
constexpr auto kAttackId = "attack";
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

constexpr auto kChorusHighCutId = "chorusHighCut";
constexpr auto kChorusMasterDepthId = "chorusMasterDepth";
constexpr auto kChorusMasterRateId = "chorusMasterRate";

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

ChorusRvbAudioProcessor::ChorusRvbAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float ChorusRvbAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout ChorusRvbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInPanLeftId, "In Pan L", -1.0f, 1.0f, -1.0f));
    params.push_back(floatParam(kInPanRightId, "In Pan R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kChorusHighCutId, "Chorus High Cut", 1000.0f, 20000.0f, 10000.0f, "Hz", 0.4f));

    // -- Plate reverb core (fixed, in parallel) --
    params.push_back(floatParam(kDecayId, "Decay", 0.3f, 8.0f, 2.2f, "s", 0.5f));
    params.push_back(floatParam(kLowRatioId, "Low Ratio", 0.2f, 2.0f, 1.0f));
    params.push_back(floatParam(kCrossoverId, "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
    params.push_back(floatParam(kDampingId, "Damping", 0.0f, 1.0f, 0.3f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 0.6f));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kLinkId, 1 }, "Link", false));
    params.push_back(floatParam(kAttackId, "Attack", 0.0f, 1.0f, 0.0f));
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

    params.push_back(floatParam(kChorusMasterDepthId, "Chorus Master Depth", 0.0f, 200.0f, 100.0f, "%"));
    params.push_back(floatParam(kChorusMasterRateId, "Chorus Master Rate", 0.0f, 200.0f, 100.0f, "%"));

    static constexpr float kDefaultDelay[6] = { 0.02f, 0.035f, 0.05f, 0.025f, 0.04f, 0.055f };
    static constexpr float kDefaultPan[6] = { -0.7f, -0.4f, -0.15f, 0.15f, 0.4f, 0.7f };
    static constexpr float kDefaultDepth[6] = { 12.0f, 18.0f, 24.0f, 14.0f, 20.0f, 26.0f };
    static constexpr float kDefaultRate[6] = { 0.25f, 0.31f, 0.19f, 0.28f, 0.22f, 0.34f };
    for (int i = 0; i < 6; ++i)
    {
        params.push_back(floatParam(voiceParamId(i, "Delay").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.365f,
                                     kDefaultDelay[static_cast<std::size_t>(i)], "s", 0.5f));
        params.push_back(floatParam(voiceParamId(i, "Level").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.5f));
        params.push_back(floatParam(voiceParamId(i, "Pan").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                     kDefaultPan[static_cast<std::size_t>(i)]));
        params.push_back(floatParam(voiceParamId(i, "Feedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.15f));
        params.push_back(floatParam(voiceParamId(i, "Depth").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Depth", 0.0f, 500.0f,
                                     kDefaultDepth[static_cast<std::size_t>(i)], "ms"));
        params.push_back(floatParam(voiceParamId(i, "Rate").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Rate", 0.0f, 3.5f,
                                     kDefaultRate[static_cast<std::size_t>(i)], "Hz"));
    }

    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void ChorusRvbAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::ChorusRvbAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void ChorusRvbAudioProcessor::releaseResources() {}

bool ChorusRvbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChorusRvbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));
    engine_.setInPan(paramValue(kInPanLeftId), paramValue(kInPanRightId));
    engine_.setChorusHighCut(paramValue(kChorusHighCutId));

    engine_.setDecaySeconds(paramValue(kDecayId));
    engine_.setLowRatio(paramValue(kLowRatioId));
    engine_.setCrossoverFrequency(paramValue(kCrossoverId));
    engine_.setDamping(paramValue(kDampingId));
    engine_.setDiffusion(paramValue(kDiffusionId));
    engine_.setSize(paramValue(kSizeId));
    engine_.setLink(paramValue(kLinkId) >= 0.5f);
    engine_.setAttack(paramValue(kAttackId));
    engine_.setRvbOut(paramValue(kRvbOutId));
    engine_.setPreDelaySeconds(paramValue(kPreDelayId));
    engine_.setEarlyReflectionLevel(paramValue(kEarlyReflectionLevelLeftId),
                                     paramValue(kEarlyReflectionLevelRightId));
    engine_.setEarlyReflectionDelaySeconds(paramValue(kEarlyReflectionDelayLeftId),
                                            paramValue(kEarlyReflectionDelayRightId));
    engine_.setEkoDelaySeconds(paramValue(kEkoDelayLeftId), paramValue(kEkoDelayRightId));
    engine_.setEkoFeedback(paramValue(kEkoFeedbackLeftId), paramValue(kEkoFeedbackRightId));
    engine_.setSpin(paramValue(kSpinId));

    engine_.setChorusMaster(paramValue(kChorusMasterDepthId), paramValue(kChorusMasterRateId));

    for (int i = 0; i < 6; ++i)
    {
        engine_.setVoiceDelay(i, paramValue(voiceParamId(i, "Delay").toRawUTF8()));
        engine_.setVoiceLevel(i, paramValue(voiceParamId(i, "Level").toRawUTF8()));
        engine_.setVoicePan(i, paramValue(voiceParamId(i, "Pan").toRawUTF8()));
        engine_.setVoiceFeedback(i, paramValue(voiceParamId(i, "Feedback").toRawUTF8()));
        engine_.setVoiceChorus(i, paramValue(voiceParamId(i, "Depth").toRawUTF8()),
                                paramValue(voiceParamId(i, "Rate").toRawUTF8()));
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

juce::AudioProcessorEditor* ChorusRvbAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::chorusRvbSchema());
}

void ChorusRvbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void ChorusRvbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new ChorusRvbAudioProcessor();
}
