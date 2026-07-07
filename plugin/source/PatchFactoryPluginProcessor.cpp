#include "PatchFactoryPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/PatchFactorySchema.h"

#include <span>

namespace
{
using dsp::algorithms::PatchFactory;

constexpr auto kCutoff1Id = "cutoff1";
constexpr auto kCutoff2Id = "cutoff2";
constexpr auto kQ1Id = "q1";
constexpr auto kQ2Id = "q2";
constexpr auto kDelay1Id = "delay1";
constexpr auto kDelay2Id = "delay2";
constexpr auto kScale1Id = "scale1";
constexpr auto kScale2Id = "scale2";
constexpr auto kShiftCentsId = "shiftCents";
constexpr auto kPitchDelayId = "pitchDelay";
constexpr auto kLeftMixId = "leftMix";
constexpr auto kRightMixId = "rightMix";

constexpr auto kFilter1InId = "filter1In";
constexpr auto kFilter2InId = "filter2In";
constexpr auto kDelay1InId = "delay1In";
constexpr auto kDelay2InId = "delay2In";
constexpr auto kScale1InId = "scale1In";
constexpr auto kScale2InId = "scale2In";
constexpr auto kSum1aInId = "sum1aIn";
constexpr auto kSum1bInId = "sum1bIn";
constexpr auto kSum2aInId = "sum2aIn";
constexpr auto kSum2bInId = "sum2bIn";
constexpr auto kShiftInId = "shiftIn";
constexpr auto kLOutputId = "lOutput";
constexpr auto kROutputId = "rOutput";

// Order matches dsp::algorithms::PatchFactory::Source exactly, so the
// AudioParameterChoice index can be cast straight to the enum.
const juce::StringArray kSourceNames = { "Left Input", "Sum 1",       "Sum 2",       "Delay 1",
                                          "Delay 2",    "Scaler 1",    "Scaler 2",    "Lowpass 1",
                                          "Bandpass 1", "Highpass 1",  "Lowpass 2",   "Bandpass 2",
                                          "Highpass 2", "Pitch Shift", "Noise Gen",   "Null Input" };

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
                                                          PatchFactory::Source defaultSource)
{
    return std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ id, 1 }, name, kSourceNames, static_cast<int>(defaultSource));
}
}

EventidePatchFactoryAudioProcessor::EventidePatchFactoryAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventidePatchFactoryAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventidePatchFactoryAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kCutoff1Id, "Cutoff 1", 0.0f, 7000.0f, 1000.0f, "Hz"));
    params.push_back(floatParam(kCutoff2Id, "Cutoff 2", 0.0f, 7000.0f, 1000.0f, "Hz"));
    params.push_back(floatParam(kQ1Id, "Q 1", 0.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kQ2Id, "Q 2", 0.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kDelay1Id, "Delay 1", 0.0f, 0.5f, 0.15f, "s"));
    params.push_back(floatParam(kDelay2Id, "Delay 2", 0.0f, 0.5f, 0.25f, "s"));
    params.push_back(floatParam(kScale1Id, "Scale 1", -100.0f, 100.0f, 30.0f, "%"));
    params.push_back(floatParam(kScale2Id, "Scale 2", -100.0f, 100.0f, 70.0f, "%"));
    params.push_back(floatParam(kShiftCentsId, "Shift", -4800.0f, 1200.0f, 0.0f, "cents"));
    params.push_back(floatParam(kPitchDelayId, "P Delay", 0.01f, 0.3f, 0.02f, "s"));
    params.push_back(floatParam(kLeftMixId, "Left Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kRightMixId, "Right Mix", 0.0f, 1.0f, 0.5f));

    params.push_back(sourceParam(kFilter1InId, "Filt 1 In", PatchFactory::Source::kDelay1));
    params.push_back(sourceParam(kFilter2InId, "Filt 2 In", PatchFactory::Source::kPitchShift));
    params.push_back(sourceParam(kDelay1InId, "Delay 1 In", PatchFactory::Source::kSum2));
    params.push_back(sourceParam(kDelay2InId, "Delay 2 In", PatchFactory::Source::kLeftInput));
    params.push_back(sourceParam(kScale1InId, "Scale 1 In", PatchFactory::Source::kNoiseGen));
    params.push_back(sourceParam(kScale2InId, "Scale 2 In", PatchFactory::Source::kLeftInput));
    params.push_back(sourceParam(kSum1aInId, "Sum 1a In", PatchFactory::Source::kScaler1));
    params.push_back(sourceParam(kSum1bInId, "Sum 1b In", PatchFactory::Source::kNullInput));
    params.push_back(sourceParam(kSum2aInId, "Sum 2a In", PatchFactory::Source::kScaler2));
    params.push_back(sourceParam(kSum2bInId, "Sum 2b In", PatchFactory::Source::kNullInput));
    params.push_back(sourceParam(kShiftInId, "Shift In", PatchFactory::Source::kLeftInput));
    params.push_back(sourceParam(kLOutputId, "L Output", PatchFactory::Source::kLowpass1));
    params.push_back(sourceParam(kROutputId, "R Output", PatchFactory::Source::kLowpass2));

    return { params.begin(), params.end() };
}

void EventidePatchFactoryAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::PatchFactoryAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventidePatchFactoryAudioProcessor::releaseResources() {}

bool EventidePatchFactoryAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventidePatchFactoryAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setCutoff1(paramValue(kCutoff1Id));
    engine_.setCutoff2(paramValue(kCutoff2Id));
    engine_.setQ1(paramValue(kQ1Id));
    engine_.setQ2(paramValue(kQ2Id));
    engine_.setDelay1Seconds(paramValue(kDelay1Id));
    engine_.setDelay2Seconds(paramValue(kDelay2Id));
    engine_.setScale1(paramValue(kScale1Id));
    engine_.setScale2(paramValue(kScale2Id));
    engine_.setShiftCents(paramValue(kShiftCentsId));
    engine_.setPitchDelaySeconds(paramValue(kPitchDelayId));
    engine_.setLeftMix(paramValue(kLeftMixId));
    engine_.setRightMix(paramValue(kRightMixId));

    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kFilter1In,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kFilter1InId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kFilter2In,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kFilter2InId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kDelay1In,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kDelay1InId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kDelay2In,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kDelay2InId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kScale1In,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kScale1InId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kScale2In,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kScale2InId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kSum1aIn,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kSum1aInId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kSum1bIn,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kSum1bInId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kSum2aIn,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kSum2aInId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kSum2bIn,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kSum2bInId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kShiftIn,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kShiftInId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kLOutput,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kLOutputId))));
    engine_.setPatch(dsp::algorithms::PatchFactory::Destination::kROutput,
                      static_cast<dsp::algorithms::PatchFactory::Source>(static_cast<int>(paramValue(kROutputId))));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventidePatchFactoryAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::patchFactorySchema());
}

void EventidePatchFactoryAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventidePatchFactoryAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventidePatchFactoryAudioProcessor();
}
