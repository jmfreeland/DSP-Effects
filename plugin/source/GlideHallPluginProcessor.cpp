#include "GlideHallPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/GlideHallSchema.h"

#include <span>

namespace
{
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";
constexpr auto kInPanLeftId = "inPanLeft";
constexpr auto kInPanRightId = "inPanRight";
constexpr auto kVoiceDiffusionId = "voiceDiffusion";

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

constexpr auto kGlideLevelId = "glideLevel";
constexpr auto kGlideTapALevelLeftId = "glideTapALevelLeft";
constexpr auto kGlideTapADelayLeftId = "glideTapADelayLeft";
constexpr auto kGlideTapALevelRightId = "glideTapALevelRight";
constexpr auto kGlideTapADelayRightId = "glideTapADelayRight";
constexpr auto kGlideTapBLevelLeftId = "glideTapBLevelLeft";
constexpr auto kGlideTapBDelayLeftId = "glideTapBDelayLeft";
constexpr auto kGlideTapBLevelRightId = "glideTapBLevelRight";
constexpr auto kGlideTapBDelayRightId = "glideTapBDelayRight";
constexpr auto kGlideFeedbackLeftId = "glideFeedbackLeft";
constexpr auto kGlideFeedbackRightId = "glideFeedbackRight";
constexpr auto kGlideCrossFeedbackLeftId = "glideCrossFeedbackLeft";
constexpr auto kGlideCrossFeedbackRightId = "glideCrossFeedbackRight";

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

GlideHallAudioProcessor::GlideHallAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float GlideHallAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout GlideHallAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // -- Input conditioning --
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInPanLeftId, "In Pan L", -1.0f, 1.0f, -1.0f));
    params.push_back(floatParam(kInPanRightId, "In Pan R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kVoiceDiffusionId, "Voice Diffusion", 0.0f, 1.0f, 0.3f));

    // -- Concert Hall reverb core (fixed, in series) --
    params.push_back(floatParam(kDecayId, "Decay", 0.3f, 8.0f, 2.5f, "s", 0.5f));
    params.push_back(floatParam(kLowRatioId, "Low Ratio", 0.2f, 2.0f, 1.0f));
    params.push_back(floatParam(kCrossoverId, "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
    params.push_back(floatParam(kDampingId, "Damping", 0.0f, 1.0f, 0.4f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.6f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 0.6f));
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

    // -- Glide FX (row 3): a stereo pair of 2-tap gliding delays --
    params.push_back(floatParam(kGlideLevelId, "Gld Lvl", 0.0f, 1.0f, 0.8f));
    params.push_back(floatParam(kGlideTapALevelLeftId, "A Lvl L", -1.0f, 1.0f, 0.7f));
    params.push_back(floatParam(kGlideTapADelayLeftId, "A Dly L", 0.0f, 0.042f, 0.006f, "s"));
    params.push_back(floatParam(kGlideTapALevelRightId, "A Lvl R", -1.0f, 1.0f, 0.7f));
    params.push_back(floatParam(kGlideTapADelayRightId, "A Dly R", 0.0f, 0.042f, 0.007f, "s"));
    params.push_back(floatParam(kGlideTapBLevelLeftId, "B Lvl L", -1.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kGlideTapBDelayLeftId, "B Dly L", 0.0f, 0.042f, 0.018f, "s"));
    params.push_back(floatParam(kGlideTapBLevelRightId, "B Lvl R", -1.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kGlideTapBDelayRightId, "B Dly R", 0.0f, 0.042f, 0.020f, "s"));
    params.push_back(floatParam(kGlideFeedbackLeftId, "Fbk L", -1.0f, 1.0f, 0.2f));
    params.push_back(floatParam(kGlideFeedbackRightId, "Fbk R", -1.0f, 1.0f, 0.2f));
    params.push_back(floatParam(kGlideCrossFeedbackLeftId, "X-Fbk L", -1.0f, 1.0f, 0.1f));
    params.push_back(floatParam(kGlideCrossFeedbackRightId, "X-Fbk R", -1.0f, 1.0f, 0.1f));

    // -- Six voices (1-3 from the left glide bank, 4-6 from the right) --
    static constexpr float kDefaultDelay[6] = { 0.12f, 0.24f, 0.36f, 0.15f, 0.27f, 0.39f };
    static constexpr float kDefaultPan[6] = { -0.6f, -0.3f, -0.8f, 0.6f, 0.3f, 0.8f };
    for (int i = 0; i < 6; ++i)
    {
        params.push_back(floatParam(voiceParamId(i, "Delay").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Delay", 0.0f, 10.581f,
                                     kDefaultDelay[static_cast<std::size_t>(i)], "s", 0.4f));
        params.push_back(floatParam(voiceParamId(i, "Level").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.5f));
        params.push_back(floatParam(voiceParamId(i, "Pan").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                     kDefaultPan[static_cast<std::size_t>(i)]));
        params.push_back(floatParam(voiceParamId(i, "Feedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.15f));
        params.push_back(floatParam(voiceParamId(i, "CrossFeedback").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " X-Fbk", -1.0f, 1.0f, 0.03f));
    }

    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 0.6f));
    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void GlideHallAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::GlideHallAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void GlideHallAudioProcessor::releaseResources() {}

bool GlideHallAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GlideHallAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));
    engine_.setInPan(paramValue(kInPanLeftId), paramValue(kInPanRightId));
    engine_.setVoiceDiffusion(paramValue(kVoiceDiffusionId));

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

    engine_.setGlideLevel(paramValue(kGlideLevelId));
    engine_.setGlideTapALeft(paramValue(kGlideTapALevelLeftId), paramValue(kGlideTapADelayLeftId));
    engine_.setGlideTapARight(paramValue(kGlideTapALevelRightId), paramValue(kGlideTapADelayRightId));
    engine_.setGlideTapBLeft(paramValue(kGlideTapBLevelLeftId), paramValue(kGlideTapBDelayLeftId));
    engine_.setGlideTapBRight(paramValue(kGlideTapBLevelRightId), paramValue(kGlideTapBDelayRightId));
    engine_.setGlideFeedback(paramValue(kGlideFeedbackLeftId), paramValue(kGlideFeedbackRightId));
    engine_.setGlideCrossFeedback(paramValue(kGlideCrossFeedbackLeftId),
                                   paramValue(kGlideCrossFeedbackRightId));

    for (int i = 0; i < 6; ++i)
    {
        engine_.setVoiceDelay(i, paramValue(voiceParamId(i, "Delay").toRawUTF8()));
        engine_.setVoiceLevel(i, paramValue(voiceParamId(i, "Level").toRawUTF8()));
        engine_.setVoicePan(i, paramValue(voiceParamId(i, "Pan").toRawUTF8()));
        engine_.setVoiceFeedback(i, paramValue(voiceParamId(i, "Feedback").toRawUTF8()));
        engine_.setVoiceCrossFeedback(i, paramValue(voiceParamId(i, "CrossFeedback").toRawUTF8()));
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

juce::AudioProcessorEditor* GlideHallAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::glideHallSchema());
}

void GlideHallAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void GlideHallAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new GlideHallAudioProcessor();
}
