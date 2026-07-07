#include "DiatonicShiftPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/DiatonicShiftSchema.h"

#include <span>

namespace
{
constexpr auto kGrainId = "grain";
constexpr auto kDelayId = "delay";
constexpr auto kKeyId = "key";
constexpr auto kScaleId = "scale";
constexpr auto kLeftVoiceId = "leftVoice";
constexpr auto kRightVoiceId = "rightVoice";
constexpr auto kLeftFeedbackId = "leftFeedback";
constexpr auto kRightFeedbackId = "rightFeedback";
constexpr auto kLeftMixId = "leftMix";
constexpr auto kRightMixId = "rightMix";
constexpr auto kTuneId = "tune";
constexpr auto kLowNoteHzId = "lowNoteHz";
constexpr auto kHighNoteHzId = "highNoteHz";
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";

const juce::StringArray kKeyNames = { "C",  "C#", "D",  "D#", "E",  "F",
                                       "F#", "G",  "G#", "A",  "A#", "B" };
const juce::StringArray kScaleNames = { "Major", "Natural Minor", "Harmonic Minor", "Dorian",
                                         "Mixolydian", "Lydian" };
// Order matches dsp::HarmonicInterval exactly, so the AudioParameterChoice
// index can be cast straight to the enum.
const juce::StringArray kVoiceNames = { "Octave Down",       "7th Down",         "6th Down",
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

EventideDiatonicShiftAudioProcessor::EventideDiatonicShiftAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideDiatonicShiftAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideDiatonicShiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kGrainId, "Grain", 0.01f, 0.3f, 0.07f, "s"));
    params.push_back(floatParam(kDelayId, "Delay", 0.0f, 1.0f, 0.05f, "s"));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kKeyId, 1 }, "Key", kKeyNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kScaleId, 1 }, "Scale", kScaleNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kLeftVoiceId, 1 }, "Left Voice", kVoiceNames, 8));  // 3rd Up
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kRightVoiceId, 1 }, "Right Voice", kVoiceNames, 10)); // 5th Up
    params.push_back(floatParam(kLeftFeedbackId, "Left Feedback", 0.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kRightFeedbackId, "Right Feedback", 0.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kLeftMixId, "Left Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kRightMixId, "Right Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kTuneId, "Tune", -50.0f, 50.0f, 0.0f, "cents"));
    params.push_back(floatParam(kLowNoteHzId, "Low Note", 30.0f, 400.0f, 80.0f, "Hz"));
    params.push_back(floatParam(kHighNoteHzId, "High Note", 200.0f, 1500.0f, 800.0f, "Hz"));
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

void EventideDiatonicShiftAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::DiatonicShiftAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideDiatonicShiftAudioProcessor::releaseResources() {}

bool EventideDiatonicShiftAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideDiatonicShiftAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                         juce::MidiBuffer&)
{
    engine_.setGrainSeconds(paramValue(kGrainId));
    engine_.setDelaySeconds(paramValue(kDelayId));
    engine_.setKey(static_cast<int>(paramValue(kKeyId)));
    engine_.setScale(scaleFromIndex(static_cast<int>(paramValue(kScaleId))));
    engine_.setLeftVoice(static_cast<dsp::HarmonicInterval>(static_cast<int>(paramValue(kLeftVoiceId))));
    engine_.setRightVoice(
      static_cast<dsp::HarmonicInterval>(static_cast<int>(paramValue(kRightVoiceId))));
    engine_.setLeftFeedback(paramValue(kLeftFeedbackId));
    engine_.setRightFeedback(paramValue(kRightFeedbackId));
    engine_.setLeftMix(paramValue(kLeftMixId));
    engine_.setRightMix(paramValue(kRightMixId));
    engine_.setTuneCents(paramValue(kTuneId));
    engine_.setFrequencyRange(paramValue(kLowNoteHzId), paramValue(kHighNoteHzId));
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideDiatonicShiftAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::diatonicShiftSchema());
}

void EventideDiatonicShiftAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideDiatonicShiftAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideDiatonicShiftAudioProcessor();
}
