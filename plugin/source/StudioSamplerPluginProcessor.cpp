#include "StudioSamplerPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/StudioSamplerSchema.h"

#include <array>
#include <functional>
#include <span>

namespace
{
using dsp::SamplerVoice;

constexpr std::array<const char*, 2> kPitchIds = { "pitch1", "pitch2" };
constexpr std::array<const char*, 2> kTimeIds = { "time1", "time2" };
constexpr std::array<const char*, 2> kAttackIds = { "attack1", "attack2" };
constexpr std::array<const char*, 2> kReleaseIds = { "release1", "release2" };
constexpr std::array<const char*, 2> kStartIds = { "start1", "start2" };
constexpr std::array<const char*, 2> kEndIds = { "end1", "end2" };
constexpr std::array<const char*, 2> kLoopIds = { "loop1", "loop2" };
constexpr std::array<const char*, 2> kShiftModeIds = { "shiftMode1", "shiftMode2" };
constexpr std::array<const char*, 2> kTriggerModeIds = { "triggerMode1", "triggerMode2" };
constexpr std::array<const char*, 2> kThresholdIds = { "threshold1", "threshold2" };
constexpr std::array<const char*, 2> kRecordIds = { "record1", "record2" };
constexpr std::array<const char*, 2> kStopIds = { "stop1", "stop2" };
constexpr std::array<const char*, 2> kPlayIds = { "play1", "play2" };
constexpr auto kMixId = "mix";

const juce::StringArray kShiftModeNames = { "Generic Sampler", "Constant Length" };
const juce::StringArray kTriggerModeNames = { "Off", "Audio" };

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

std::unique_ptr<juce::AudioParameterBool> triggerParam(const char* id, const juce::String& name)
{
    return std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id, 1 }, name, false);
}

SamplerVoice::ShiftMode shiftModeFromIndex(int index)
{
    return index == 1 ? SamplerVoice::ShiftMode::kConstantLength : SamplerVoice::ShiftMode::kGenericSampler;
}

SamplerVoice::TriggerMode triggerModeFromIndex(int index)
{
    return index == 1 ? SamplerVoice::TriggerMode::kAudio : SamplerVoice::TriggerMode::kOff;
}
}

EventideStudioSamplerAudioProcessor::EventideStudioSamplerAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideStudioSamplerAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideStudioSamplerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (std::size_t i = 0; i < kPitchIds.size(); ++i)
    {
        auto n = juce::String(i + 1);
        params.push_back(floatParam(kPitchIds[i], "Pitch " + n, -3600.0f, 3600.0f, 0.0f, "ct"));
        params.push_back(floatParam(kTimeIds[i], "Time " + n, 0.0f, 800.0f, 100.0f, "%"));
        params.push_back(floatParam(kAttackIds[i], "Attack " + n, 0.001f, 1.0f, 0.005f, "s"));
        params.push_back(floatParam(kReleaseIds[i], "Release " + n, 0.001f, 1.0f, 0.005f, "s"));
        params.push_back(floatParam(kStartIds[i], "Start " + n, 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(kEndIds[i], "End " + n, 0.0f, 1.0f, 1.0f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kLoopIds[i], 1 }, "Loop " + n, false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ kShiftModeIds[i], 1 }, "Shift Mode " + n, kShiftModeNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ kTriggerModeIds[i], 1 }, "Trigger Mode " + n, kTriggerModeNames, 0));
        params.push_back(floatParam(kThresholdIds[i], "Threshold " + n, 0.0f, 1.0f, 0.1f));
        params.push_back(triggerParam(kRecordIds[i], "Record " + n));
        params.push_back(triggerParam(kStopIds[i], "Stop " + n));
        params.push_back(triggerParam(kPlayIds[i], "Play " + n));
    }
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 100.0f, 100.0f, "%"));

    return { params.begin(), params.end() };
}

void EventideStudioSamplerAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::StudioSamplerAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideStudioSamplerAudioProcessor::releaseResources() {}

bool EventideStudioSamplerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void EventideStudioSamplerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    for (std::size_t i = 0; i < kPitchIds.size(); ++i)
    {
        auto channel = static_cast<int>(i);
        engine_.setPitchCents(channel, paramValue(kPitchIds[i]));
        engine_.setTimePercent(channel, paramValue(kTimeIds[i]));
        engine_.setAttackSeconds(channel, paramValue(kAttackIds[i]));
        engine_.setReleaseSeconds(channel, paramValue(kReleaseIds[i]));
        engine_.setStartFraction(channel, paramValue(kStartIds[i]));
        engine_.setEndFraction(channel, paramValue(kEndIds[i]));
        engine_.setLoop(channel, paramValue(kLoopIds[i]) >= 0.5f);
        engine_.setShiftMode(channel, shiftModeFromIndex(static_cast<int>(paramValue(kShiftModeIds[i]))));
        engine_.setTriggerMode(channel, triggerModeFromIndex(static_cast<int>(paramValue(kTriggerModeIds[i]))));
        engine_.setThreshold(channel, paramValue(kThresholdIds[i]));

        pollTrigger(apvts, kRecordIds[i], [this, channel] { engine_.record(channel); });
        pollTrigger(apvts, kStopIds[i], [this, channel] { engine_.stop(channel); });
        pollTrigger(apvts, kPlayIds[i], [this, channel] { engine_.play(channel); });
    }
    engine_.setMix(paramValue(kMixId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideStudioSamplerAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::studioSamplerSchema());
}

void EventideStudioSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideStudioSamplerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideStudioSamplerAudioProcessor();
}
