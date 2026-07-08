#include "VocoderPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/VocoderSchema.h"

#include <span>

namespace
{
constexpr auto kFormantSpeedId = "formantSpeed";
constexpr auto kEnvelopeSpeedId = "envelopeSpeed";
constexpr auto kFormantShiftId = "formantShift";
constexpr auto kDepthId = "depth";
constexpr auto kWidthId = "width";
constexpr auto kMixId = "mix";
constexpr auto kMaxResonanceId = "maxResonance";
constexpr auto kThresholdId = "threshold";
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";

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
}

EventideVocoderAudioProcessor::EventideVocoderAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideVocoderAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideVocoderAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kFormantSpeedId, "Formant Speed", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kEnvelopeSpeedId, "Envelope Speed", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kFormantShiftId, "Formant Shift", 0.0f, 100.0f, 0.0f));
    params.push_back(floatParam(kDepthId, "Depth", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kWidthId, "Width", 0.0f, 0.01f, 0.005f, "s"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kMaxResonanceId, "Max Resonance", 0.0f, 100.0f, 30.0f));
    params.push_back(floatParam(kThresholdId, "Threshold", 0.0f, 1.0f, 0.02f));
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

void EventideVocoderAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::VocoderAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideVocoderAudioProcessor::releaseResources() {}

bool EventideVocoderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideVocoderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setFormantSpeed(paramValue(kFormantSpeedId));
    engine_.setEnvelopeSpeed(paramValue(kEnvelopeSpeedId));
    engine_.setFormantShift(paramValue(kFormantShiftId));
    engine_.setDepth(paramValue(kDepthId));
    engine_.setWidthSeconds(paramValue(kWidthId));
    engine_.setMix(paramValue(kMixId));
    engine_.setMaxResonance(paramValue(kMaxResonanceId));
    engine_.setThreshold(paramValue(kThresholdId));
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideVocoderAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::vocoderSchema());
}

void EventideVocoderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideVocoderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideVocoderAudioProcessor();
}
