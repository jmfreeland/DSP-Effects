#include "InversePluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/ReverbCoreSchemas.h"

#include <span>

namespace
{
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";
constexpr auto kInPanLeftId = "inPanLeft";
constexpr auto kInPanRightId = "inPanRight";
constexpr auto kCrossoverId = "crossover";
constexpr auto kDampingId = "damping";
constexpr auto kDiffusionId = "diffusion";
constexpr auto kSizeId = "size";
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
constexpr auto kDurationId = "duration";
constexpr auto kLowSlopeId = "lowSlope";
constexpr auto kMidSlopeId = "midSlope";
constexpr auto kShapeId = "shape";
constexpr auto kVoiceDiffusionId = "voiceDiffusion";
constexpr auto kVoiceGlideResponseId = "voiceGlideResponse";
constexpr auto kVoiceGlideRangeId = "voiceGlideRange";
constexpr auto kClearId = "clear";
constexpr auto kPostDelayLeftId = "postDelayLeft";
constexpr auto kPostDelayRightId = "postDelayRight";
constexpr auto kPostDelayGlideResponseId = "postDelayGlideResponse";
constexpr auto kPostDelayGlideRangeId = "postDelayGlideRange";
constexpr auto kPostDelayMixId = "postDelayMix";
constexpr auto kRvbWidthId = "rvbWidth";
constexpr auto kFxMixId = "fxMix";
constexpr auto kFxWidthId = "fxWidth";
constexpr auto kHiCutId = "hiCut";
constexpr auto kFxAdjustId = "fxAdjust";
constexpr auto kMixId = "mix";
constexpr auto kFreezeId = "freeze";

// One voice's four parameter IDs, e.g. "voice0Delay".
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

LexiconInverseAudioProcessor::LexiconInverseAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float LexiconInverseAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
LexiconInverseAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // -- Input conditioning --
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInPanLeftId, "In Pan L", -1.0f, 1.0f, -1.0f));
    params.push_back(floatParam(kInPanRightId, "In Pan R", -1.0f, 1.0f, 1.0f));

    // -- Reverb core --
    // Note: no Decay/Low Ratio/Link here, unlike the other three cores -
    // Duration + Low Slope + Mid Slope below replace them for Inverse.
    params.push_back(floatParam(kCrossoverId, "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
    params.push_back(floatParam(kDampingId, "Damping", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.6f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kDefinitionId, "Definition", 0.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kDepthId, "Depth", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kRvbInId, "Rvb In", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kRvbOutId, "Rvb Out", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kPreDelayId, "Pre Delay", 0.0f, 0.93f, 0.0f, "s"));
    params.push_back(
      floatParam(kEarlyReflectionLevelLeftId, "Early Reflections L", 0.0f, 1.0f, 0.2f));
    params.push_back(
      floatParam(kEarlyReflectionLevelRightId, "Early Reflections R", 0.0f, 1.0f, 0.2f));
    params.push_back(
      floatParam(kEarlyReflectionDelayLeftId, "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
    params.push_back(
      floatParam(kEarlyReflectionDelayRightId, "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
    params.push_back(floatParam(kSpinId, "Spin", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kChorusId, "Chorus", 0.0f, 1.0f, 0.3f));

    // -- Inverse-specific: Duration + independent Low Slope/Mid Slope --
    params.push_back(floatParam(kDurationId, "Duration", 0.05f, 8.0f, 1.0f, "s", 0.5f));
    params.push_back(floatParam(kLowSlopeId, "Low Slope", -1.0f, 1.0f, -0.3f));
    params.push_back(floatParam(kMidSlopeId, "Mid Slope", -1.0f, 1.0f, -0.3f));
    // Spread is fixed internally for Inverse per the manual - only Shape
    // is exposed here (see dsp/algorithms/Inverse.h).
    params.push_back(floatParam(kShapeId, "Shape", 0.0f, 1.0f, 0.3f));

    // -- Voice Diffusion + four delay Voices, matching the PCM81's 4-Voice Reverb Shell --
    params.push_back(floatParam(kVoiceDiffusionId, "Voice Diffusion", 0.0f, 1.0f, 0.0f));
    static constexpr float kDefaultDelay[4] = { 0.09f, 0.13f, 0.0f, 0.0f };
    static constexpr float kDefaultFeedback[4] = { 0.15f, 0.10f, 0.0f, 0.0f };
    static constexpr float kDefaultLevel[4] = { 0.25f, 0.18f, 0.0f, 0.0f };
    static constexpr float kDefaultPan[4] = { -0.3f, 0.3f, 0.0f, 0.0f };
    for (int i = 0; i < 4; ++i)
    {
        params.push_back(floatParam(voiceParamId(i, "Delay").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.365f,
                                     kDefaultDelay[i], "s"));
        params.push_back(floatParam(voiceParamId(i, "Feedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Feedback", -1.0f, 1.0f,
                                     kDefaultFeedback[i]));
        params.push_back(floatParam(voiceParamId(i, "Level").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f,
                                     kDefaultLevel[i]));
        params.push_back(floatParam(voiceParamId(i, "Pan").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                     kDefaultPan[i]));
    }
    params.push_back(floatParam(kVoiceGlideResponseId, "Voice Glide Response", 0.0f, 100.0f, 50.0f));
    params.push_back(
      floatParam(kVoiceGlideRangeId, "Voice Glide Range", 0.0f, 1.365f, 0.0f, "s"));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kClearId, 1 }, "Clear", false));

    // -- Post-delay, width, and output chain --
    params.push_back(floatParam(kPostDelayLeftId, "Post Delay L", 0.0f, 1.365f, 0.25f, "s"));
    params.push_back(floatParam(kPostDelayRightId, "Post Delay R", 0.0f, 1.365f, 0.25f, "s"));
    params.push_back(
      floatParam(kPostDelayGlideResponseId, "Post Delay Glide Response", 0.0f, 100.0f, 50.0f));
    params.push_back(
      floatParam(kPostDelayGlideRangeId, "Post Delay Glide Range", 0.0f, 1.365f, 0.0f, "s"));
    params.push_back(floatParam(kPostDelayMixId, "Post Delay Mix", 0.0f, 1.0f, 0.15f));
    params.push_back(floatParam(kRvbWidthId, "Rvb Width", -360.0f, 360.0f, 0.0f, "deg"));
    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 0.75f));
    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.4f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void LexiconInverseAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::InverseAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void LexiconInverseAudioProcessor::releaseResources() {}

bool LexiconInverseAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LexiconInverseAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));
    engine_.setInPan(paramValue(kInPanLeftId), paramValue(kInPanRightId));

    engine_.setCrossoverFrequency(paramValue(kCrossoverId));
    engine_.setDamping(paramValue(kDampingId));
    engine_.setDiffusion(paramValue(kDiffusionId));
    engine_.setSize(paramValue(kSizeId));
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
    engine_.setDuration(paramValue(kDurationId));
    engine_.setLowSlope(paramValue(kLowSlopeId));
    engine_.setMidSlope(paramValue(kMidSlopeId));
    engine_.setShape(paramValue(kShapeId));
    engine_.setVoiceDiffusion(paramValue(kVoiceDiffusionId));

    engine_.setVoiceGlide(paramValue(kVoiceGlideResponseId), paramValue(kVoiceGlideRangeId));
    for (int i = 0; i < 4; ++i)
    {
        engine_.setVoice(i, paramValue(voiceParamId(i, "Delay").toRawUTF8()),
                          paramValue(voiceParamId(i, "Feedback").toRawUTF8()),
                          paramValue(voiceParamId(i, "Level").toRawUTF8()),
                          paramValue(voiceParamId(i, "Pan").toRawUTF8()));
    }
    engine_.setClear(paramValue(kClearId) >= 0.5f);

    engine_.setPostDelayGlide(paramValue(kPostDelayGlideResponseId),
                               paramValue(kPostDelayGlideRangeId));
    engine_.setPostDelaySeconds(paramValue(kPostDelayLeftId), paramValue(kPostDelayRightId));
    engine_.setPostDelayMix(paramValue(kPostDelayMixId));
    engine_.setRvbWidth(paramValue(kRvbWidthId));
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

juce::AudioProcessorEditor* LexiconInverseAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::inverseSchema());
}

void LexiconInverseAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void LexiconInverseAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new LexiconInverseAudioProcessor();
}
