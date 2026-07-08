#include "MultiShiftPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/MultiShiftSchema.h"

#include <span>

namespace
{
using dsp::algorithms::MultiShift;

constexpr auto kLeftCentsId = "leftCents";
constexpr auto kRightCentsId = "rightCents";
constexpr auto kLeftPitchDelayId = "leftPitchDelay";
constexpr auto kRightPitchDelayId = "rightPitchDelay";
constexpr auto kLeftDelayId = "leftDelay";
constexpr auto kRightDelayId = "rightDelay";
constexpr auto kMixId = "mix";
constexpr auto kFeedbackScaleId = "feedbackScale";
constexpr auto kImageId = "image";
constexpr auto kLPitchLevelId = "lPitchLevel";
constexpr auto kRPitchLevelId = "rPitchLevel";
constexpr auto kLDelayLevelId = "lDelayLevel";
constexpr auto kRDelayLevelId = "rDelayLevel";
constexpr auto kLPitchPanId = "lPitchPan";
constexpr auto kRPitchPanId = "rPitchPan";
constexpr auto kLDelayPanId = "lDelayPan";
constexpr auto kRDelayPanId = "rDelayPan";
constexpr auto kLeftFb1AmountId = "leftFb1Amount";
constexpr auto kLeftFb1SourceId = "leftFb1Source";
constexpr auto kLeftFb2AmountId = "leftFb2Amount";
constexpr auto kLeftFb2SourceId = "leftFb2Source";
constexpr auto kRightFb1AmountId = "rightFb1Amount";
constexpr auto kRightFb1SourceId = "rightFb1Source";
constexpr auto kRightFb2AmountId = "rightFb2Amount";
constexpr auto kRightFb2SourceId = "rightFb2Source";
constexpr auto kLeftDirectionId = "leftDirection";
constexpr auto kRightDirectionId = "rightDirection";
constexpr auto kLeftXfadeSlowId = "leftXfadeSlow";
constexpr auto kRightXfadeSlowId = "rightXfadeSlow";
constexpr auto kLeftSpliceId = "leftSplice";
constexpr auto kRightSpliceId = "rightSplice";

// Order matches dsp::algorithms::MultiShift::Source exactly.
const juce::StringArray kSourceNames = { "L Pitch", "R Pitch", "L Delay", "R Delay" };

std::unique_ptr<juce::AudioParameterFloat> floatParam(const char* id, const juce::String& name,
                                                        float min, float max, float defaultValue,
                                                        const char* label = nullptr)
{
    juce::AudioParameterFloatAttributes attributes;
    if (label != nullptr)
    {
        attributes = attributes.withLabel(label);
    }
    auto interval = (max - min) / 10000.0f;
    return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ id, 1 }, name, juce::NormalisableRange<float>(min, max, interval), defaultValue,
      attributes);
}

std::unique_ptr<juce::AudioParameterChoice> sourceParam(const char* id, const juce::String& name,
                                                          MultiShift::Source defaultSource)
{
    return std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ id, 1 }, name, kSourceNames, static_cast<int>(defaultSource));
}
}

EventideMultiShiftAudioProcessor::EventideMultiShiftAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideMultiShiftAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideMultiShiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kLeftCentsId, "L Coarse/Fine", -3600.0f, 3600.0f, 0.0f, "cents"));
    params.push_back(floatParam(kRightCentsId, "R Coarse/Fine", -3600.0f, 3600.0f, 0.0f, "cents"));
    params.push_back(floatParam(kLeftPitchDelayId, "L Pitch Delay", 0.0f, 0.7f, 0.02f, "s"));
    params.push_back(floatParam(kRightPitchDelayId, "R Pitch Delay", 0.0f, 0.7f, 0.02f, "s"));
    params.push_back(floatParam(kLeftDelayId, "L Delay", 0.0f, 0.7f, 0.1f, "s"));
    params.push_back(floatParam(kRightDelayId, "R Delay", 0.0f, 0.7f, 0.1f, "s"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kFeedbackScaleId, "Feedback", 0.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kImageId, "Image", -1.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kLPitchLevelId, "L Pitch Level", -100.0f, 100.0f, 100.0f, "%"));
    params.push_back(floatParam(kRPitchLevelId, "R Pitch Level", -100.0f, 100.0f, 100.0f, "%"));
    params.push_back(floatParam(kLDelayLevelId, "L Delay Level", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kRDelayLevelId, "R Delay Level", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kLPitchPanId, "L Pitch Pan", -1.0f, 1.0f, -1.0f));
    params.push_back(floatParam(kRPitchPanId, "R Pitch Pan", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kLDelayPanId, "L Delay Pan", -1.0f, 1.0f, -1.0f));
    params.push_back(floatParam(kRDelayPanId, "R Delay Pan", -1.0f, 1.0f, 1.0f));

    params.push_back(floatParam(kLeftFb1AmountId, "L Feedback 1", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(sourceParam(kLeftFb1SourceId, "L Fb1 Source", MultiShift::Source::kLPitch));
    params.push_back(floatParam(kLeftFb2AmountId, "L Feedback 2", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(sourceParam(kLeftFb2SourceId, "L Fb2 Source", MultiShift::Source::kLDelay));
    params.push_back(floatParam(kRightFb1AmountId, "R Feedback 1", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(sourceParam(kRightFb1SourceId, "R Fb1 Source", MultiShift::Source::kRPitch));
    params.push_back(floatParam(kRightFb2AmountId, "R Feedback 2", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(sourceParam(kRightFb2SourceId, "R Fb2 Source", MultiShift::Source::kRDelay));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kLeftDirectionId, 1 }, "L Reverse", false));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kRightDirectionId, 1 }, "R Reverse", false));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kLeftXfadeSlowId, 1 }, "L Xfade Slow", false));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kRightXfadeSlowId, 1 }, "R Xfade Slow", false));
    params.push_back(floatParam(kLeftSpliceId, "L Splice", 0.001f, 0.7f, 0.15f, "s"));
    params.push_back(floatParam(kRightSpliceId, "R Splice", 0.001f, 0.7f, 0.15f, "s"));

    return { params.begin(), params.end() };
}

void EventideMultiShiftAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::MultiShiftAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideMultiShiftAudioProcessor::releaseResources() {}

bool EventideMultiShiftAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideMultiShiftAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setLeftCents(paramValue(kLeftCentsId));
    engine_.setRightCents(paramValue(kRightCentsId));
    engine_.setLeftPitchDelaySeconds(paramValue(kLeftPitchDelayId));
    engine_.setRightPitchDelaySeconds(paramValue(kRightPitchDelayId));
    engine_.setLeftDelaySeconds(paramValue(kLeftDelayId));
    engine_.setRightDelaySeconds(paramValue(kRightDelayId));
    engine_.setMix(paramValue(kMixId));
    engine_.setFeedbackScale(paramValue(kFeedbackScaleId));
    engine_.setImage(paramValue(kImageId));
    engine_.setLPitchLevel(paramValue(kLPitchLevelId));
    engine_.setRPitchLevel(paramValue(kRPitchLevelId));
    engine_.setLDelayLevel(paramValue(kLDelayLevelId));
    engine_.setRDelayLevel(paramValue(kRDelayLevelId));
    engine_.setLPitchPan(paramValue(kLPitchPanId));
    engine_.setRPitchPan(paramValue(kRPitchPanId));
    engine_.setLDelayPan(paramValue(kLDelayPanId));
    engine_.setRDelayPan(paramValue(kRDelayPanId));

    engine_.setLeftFeedback1(paramValue(kLeftFb1AmountId),
                              static_cast<MultiShift::Source>(static_cast<int>(paramValue(kLeftFb1SourceId))));
    engine_.setLeftFeedback2(paramValue(kLeftFb2AmountId),
                              static_cast<MultiShift::Source>(static_cast<int>(paramValue(kLeftFb2SourceId))));
    engine_.setRightFeedback1(
      paramValue(kRightFb1AmountId), static_cast<MultiShift::Source>(static_cast<int>(paramValue(kRightFb1SourceId))));
    engine_.setRightFeedback2(
      paramValue(kRightFb2AmountId), static_cast<MultiShift::Source>(static_cast<int>(paramValue(kRightFb2SourceId))));

    engine_.setLeftDirection(paramValue(kLeftDirectionId) >= 0.5f);
    engine_.setRightDirection(paramValue(kRightDirectionId) >= 0.5f);
    engine_.setLeftXfadeSlow(paramValue(kLeftXfadeSlowId) >= 0.5f);
    engine_.setRightXfadeSlow(paramValue(kRightXfadeSlowId) >= 0.5f);
    engine_.setLeftSpliceSeconds(paramValue(kLeftSpliceId));
    engine_.setRightSpliceSeconds(paramValue(kRightSpliceId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideMultiShiftAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::multiShiftSchema());
}

void EventideMultiShiftAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideMultiShiftAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideMultiShiftAudioProcessor();
}
