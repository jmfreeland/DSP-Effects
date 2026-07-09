#include "Res2PlatePluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/Res2PlateSchema.h"

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

constexpr auto kKeyId = "key";
constexpr auto kScaleId = "scale";
constexpr auto kTuneId = "tune";
constexpr auto kLowNoteHzId = "lowNoteHz";
constexpr auto kHighNoteHzId = "highNoteHz";

constexpr auto kVoiceDiffusionId = "voiceDiffusion";
constexpr auto kFxMixId = "fxMix";
constexpr auto kFxWidthId = "fxWidth";
constexpr auto kHiCutId = "hiCut";
constexpr auto kFxAdjustId = "fxAdjust";
constexpr auto kMixId = "mix";
constexpr auto kFreezeId = "freeze";

const juce::StringArray kKeyNames = { "C",  "C#", "D",  "D#", "E",  "F",
                                       "F#", "G",  "G#", "A",  "A#", "B" };
const juce::StringArray kScaleNames = { "Major", "Natural Minor", "Harmonic Minor", "Dorian",
                                         "Mixolydian", "Lydian" };
// Order matches dsp::HarmonicInterval exactly, so the AudioParameterChoice
// index can be cast straight to the enum - see DiatonicShiftPluginProcessor,
// the original source of this table.
const juce::StringArray kIntervalNames = { "Octave Down",       "7th Down",         "6th Down",
                                            "5th Down",          "4th Down",         "3rd Down",
                                            "2nd Down",          "2nd Up",           "3rd Up",
                                            "4th Up",            "5th Up",           "6th Up",
                                            "7th Up",             "Octave Up",
                                            "Low Tonic Pedal",    "High Tonic Pedal",
                                            "Low Dominant Pedal", "High Dominant Pedal" };

dsp::Scale scaleFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return dsp::Scale::kNaturalMinor;
        case 2:
            return dsp::Scale::kHarmonicMinor;
        case 3:
            return dsp::Scale::kDorian;
        case 4:
            return dsp::Scale::kMixolydian;
        case 5:
            return dsp::Scale::kLydian;
        case 0:
        default:
            return dsp::Scale::kMajor;
    }
}

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

Res2PlateAudioProcessor::Res2PlateAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float Res2PlateAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout Res2PlateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInPanLeftId, "In Pan L", -1.0f, 1.0f, -1.0f));
    params.push_back(floatParam(kInPanRightId, "In Pan R", -1.0f, 1.0f, 1.0f));

    // -- Plate reverb core (fixed, in series) --
    params.push_back(floatParam(kDecayId, "Decay", 0.3f, 8.0f, 2.0f, "s", 0.5f));
    params.push_back(floatParam(kLowRatioId, "Low Ratio", 0.2f, 2.0f, 1.0f));
    params.push_back(floatParam(kCrossoverId, "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
    params.push_back(floatParam(kDampingId, "Damping", 0.0f, 1.0f, 0.4f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.6f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 0.5f));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kLinkId, 1 }, "Link", false));
    params.push_back(floatParam(kAttackId, "Attack", 0.0f, 1.0f, 1.0f));
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

    // -- Diatonic pitch tracking --
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kKeyId, 1 }, "Key", kKeyNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kScaleId, 1 }, "Scale", kScaleNames, 0));
    params.push_back(floatParam(kTuneId, "Tune", -50.0f, 50.0f, 0.0f, "cents"));
    params.push_back(floatParam(kLowNoteHzId, "Low Note", 20.0f, 800.0f, 80.0f, "Hz", 0.4f));
    params.push_back(floatParam(kHighNoteHzId, "High Note", 200.0f, 4000.0f, 800.0f, "Hz", 0.4f));

    static constexpr int kDefaultIntervalIndex[6] = { 8, 10, 13, 7, 11, 12 };
    static constexpr float kDefaultPan[6] = { -0.7f, -0.35f, -0.85f, 0.7f, 0.35f, 0.85f };
    for (int i = 0; i < 6; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ voiceParamId(i, "Interval"), 1 }, "Voice " + juce::String(i + 1) + " Interval",
          kIntervalNames, kDefaultIntervalIndex[i]));
        params.push_back(floatParam(voiceParamId(i, "Level").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.5f));
        params.push_back(floatParam(voiceParamId(i, "Pan").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                     kDefaultPan[static_cast<std::size_t>(i)]));
        params.push_back(floatParam(voiceParamId(i, "Duration").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Duration", 0.05f, 8.0f, 2.5f,
                                     "s", 0.4f));
        params.push_back(floatParam(voiceParamId(i, "HiCut").toRawUTF8(),
                                     "Voice " + juce::String(i + 1) + " Hi Cut", 200.0f, 20000.0f,
                                     4000.0f, "Hz", 0.4f));
    }

    params.push_back(floatParam(kVoiceDiffusionId, "Voice Diffusion", 0.0f, 1.0f, 0.2f));
    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void Res2PlateAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::Res2PlateAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void Res2PlateAudioProcessor::releaseResources() {}

bool Res2PlateAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Res2PlateAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    engine_.setKey(static_cast<int>(paramValue(kKeyId)));
    engine_.setScale(scaleFromIndex(static_cast<int>(paramValue(kScaleId))));
    engine_.setTuneCents(paramValue(kTuneId));
    engine_.setFrequencyRange(paramValue(kLowNoteHzId), paramValue(kHighNoteHzId));

    for (int i = 0; i < 6; ++i)
    {
        engine_.setVoiceInterval(
          i, static_cast<dsp::HarmonicInterval>(static_cast<int>(paramValue(voiceParamId(i, "Interval").toRawUTF8()))));
        engine_.setVoiceLevel(i, paramValue(voiceParamId(i, "Level").toRawUTF8()));
        engine_.setVoicePan(i, paramValue(voiceParamId(i, "Pan").toRawUTF8()));
        engine_.setVoiceDuration(i, paramValue(voiceParamId(i, "Duration").toRawUTF8()));
        engine_.setVoiceHiCut(i, paramValue(voiceParamId(i, "HiCut").toRawUTF8()));
    }

    engine_.setVoiceDiffusion(paramValue(kVoiceDiffusionId));
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

juce::AudioProcessorEditor* Res2PlateAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::res2PlateSchema());
}

void Res2PlateAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void Res2PlateAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new Res2PlateAudioProcessor();
}
