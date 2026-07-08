#include "StringModellerPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/StringModellerSchema.h"

#include <array>
#include <functional>
#include <span>

namespace
{
constexpr auto kPitchId = "pitch";
constexpr auto kDecayId = "decay";
constexpr auto kGateId = "gate";
constexpr auto kFreqId = "freq";
constexpr auto kQfacId = "qfac";
constexpr auto kBrightId = "bright";
constexpr auto kHighAmtId = "highAmt";
constexpr auto kBandAmtId = "bandAmt";
constexpr auto kLowAmtId = "lowAmt";
constexpr auto kInAmtId = "inAmt";
constexpr auto kChorusId = "chorus";
constexpr auto kChorusSpeedId = "chorusSpeed";
constexpr auto kChorusDepthId = "chorusDepth";
constexpr auto kMixId = "mix";
constexpr auto kTriggerId = "trigger";

constexpr std::array<const char*, 6> kNoteIds = { "note1", "note2", "note3", "note4", "note5", "note6" };

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

EventideStringModellerAudioProcessor::EventideStringModellerAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideStringModellerAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideStringModellerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kPitchId, "Pitch", -100.0f, 100.0f, 0.0f, "st"));
    params.push_back(floatParam(kDecayId, "Decay", 0.0f, 100.0f, 60.0f));
    params.push_back(floatParam(kGateId, "Gate", 1.0f, 100.0f, 30.0f));
    params.push_back(floatParam(kFreqId, "Freq", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kQfacId, "Qfac", 0.0f, 100.0f, 30.0f));
    params.push_back(floatParam(kBrightId, "Bright", 0.0f, 100.0f, 60.0f));
    params.push_back(floatParam(kHighAmtId, "High Amt", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kBandAmtId, "Band Amt", -100.0f, 100.0f, 60.0f, "%"));
    params.push_back(floatParam(kLowAmtId, "Low Amt", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kInAmtId, "In Amt", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kChorusId, "Chorus", 0.0f, 100.0f, 40.0f));
    params.push_back(floatParam(kChorusSpeedId, "Speed", 0.0f, 100.0f, 30.0f));
    params.push_back(floatParam(kChorusDepthId, "Depth", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 100.0f, 60.0f, "%"));

    static constexpr std::array<float, 6> kDefaultNoteHz = { 82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f };
    for (std::size_t i = 0; i < kNoteIds.size(); ++i)
    {
        auto n = juce::String(i + 1);
        params.push_back(floatParam(kNoteIds[i], "Note " + n, 16.0f, 8000.0f, kDefaultNoteHz[i], "Hz"));
    }

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kTriggerId, 1 }, "Pluck", false));

    return { params.begin(), params.end() };
}

void EventideStringModellerAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::StringModellerAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideStringModellerAudioProcessor::releaseResources() {}

bool EventideStringModellerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

namespace
{
// A momentary trigger checkbox: fires `action` once when the UI sets it
// true, then snaps itself back to false so the next click fires again.
void pollTrigger(juce::AudioProcessorValueTreeState& apvts, const char* id, const std::function<void()>& action)
{
    auto* param = apvts.getParameter(id);
    if (param->getValue() >= 0.5f)
    {
        action();
        param->setValueNotifyingHost(0.0f);
    }
}
}

void EventideStringModellerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setPitch(paramValue(kPitchId));
    engine_.setDecay(paramValue(kDecayId));
    engine_.setGateAmount(paramValue(kGateId));
    engine_.setFreq(paramValue(kFreqId));
    engine_.setQfac(paramValue(kQfacId));
    engine_.setBright(paramValue(kBrightId));
    engine_.setHighAmt(paramValue(kHighAmtId));
    engine_.setBandAmt(paramValue(kBandAmtId));
    engine_.setLowAmt(paramValue(kLowAmtId));
    engine_.setInAmt(paramValue(kInAmtId));
    engine_.setChorus(paramValue(kChorusId));
    engine_.setChorusSpeed(paramValue(kChorusSpeedId));
    engine_.setChorusDepth(paramValue(kChorusDepthId));
    engine_.setMix(paramValue(kMixId));

    for (std::size_t i = 0; i < kNoteIds.size(); ++i)
    {
        engine_.setNoteHz(static_cast<int>(i), paramValue(kNoteIds[i]));
    }

    pollTrigger(apvts, kTriggerId, [this] { engine_.trigger(); });

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideStringModellerAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::stringModellerSchema());
}

void EventideStringModellerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideStringModellerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideStringModellerAudioProcessor();
}
