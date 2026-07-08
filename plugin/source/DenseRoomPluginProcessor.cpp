#include "DenseRoomPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/DenseRoomSchema.h"

#include <array>
#include <span>

namespace
{
constexpr auto kPredelayId = "predelay";
constexpr auto kRevTimeId = "revTime";
constexpr auto kHighCutId = "highCut";
constexpr auto kSizeId = "size";
constexpr auto kPositionId = "position";
constexpr auto kPanId = "pan";
constexpr auto kEarlyMixId = "earlyMix";
constexpr auto kDiffusionId = "diffusion";
constexpr auto kMixId = "mix";

constexpr std::array<const char*, 3> kAllpassDelayIds = { "allpassDelay1", "allpassDelay2", "allpassDelay3" };
constexpr std::array<const char*, 6> kLineDelayIds = { "lineDelay1", "lineDelay2", "lineDelay3",
                                                        "lineDelay4", "lineDelay5", "lineDelay6" };
constexpr std::array<const char*, 6> kLinePanIds = { "linePan1", "linePan2", "linePan3",
                                                       "linePan4", "linePan5", "linePan6" };
constexpr std::array<const char*, 6> kLineLevelIds = { "lineLevel1", "lineLevel2", "lineLevel3",
                                                          "lineLevel4", "lineLevel5", "lineLevel6" };

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

EventideDenseRoomAudioProcessor::EventideDenseRoomAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideDenseRoomAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideDenseRoomAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kPredelayId, "Predelay", 0.0f, 0.5f, 0.02f, "s"));
    params.push_back(floatParam(kRevTimeId, "Rev Time", 0.1f, 10.0f, 2.0f, "s"));
    params.push_back(floatParam(kHighCutId, "High Cut", 0.0f, 1.0f, 0.3f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 0.7f));
    params.push_back(floatParam(kPositionId, "Position", 0.0f, 1.0f, 0.3f));
    params.push_back(floatParam(kPanId, "Pan", -1.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kEarlyMixId, "Early Mix", 0.0f, 1.0f, 0.3f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.6f));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));

    static constexpr std::array<float, 3> kDefaultAllpassDelays = { 337.0f, 563.0f, 809.0f };
    for (std::size_t i = 0; i < kAllpassDelayIds.size(); ++i)
    {
        params.push_back(floatParam(kAllpassDelayIds[i], juce::String("Allpass Delay ") + juce::String(i + 1),
                                     0.0f, 5000.0f, kDefaultAllpassDelays[i], "smp"));
    }

    static constexpr std::array<float, 6> kDefaultLineDelaysMs = { 27, 41, 59, 73, 89, 109 };
    static constexpr std::array<float, 6> kDefaultLinePans = { -0.8f, 0.8f, -0.4f, 0.4f, -0.15f, 0.15f };
    for (std::size_t i = 0; i < kLineDelayIds.size(); ++i)
    {
        params.push_back(floatParam(kLineDelayIds[i], juce::String("Delay ") + juce::String(i + 1), 1.0f,
                                     113.0f, kDefaultLineDelaysMs[i], "ms"));
        params.push_back(floatParam(kLinePanIds[i], juce::String("Pan ") + juce::String(i + 1), -1.0f, 1.0f,
                                     kDefaultLinePans[i]));
        params.push_back(
          floatParam(kLineLevelIds[i], juce::String("Level ") + juce::String(i + 1), -100.0f, 100.0f, 100.0f, "%"));
    }

    return { params.begin(), params.end() };
}

void EventideDenseRoomAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::DenseRoomAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideDenseRoomAudioProcessor::releaseResources() {}

bool EventideDenseRoomAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideDenseRoomAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setPredelaySeconds(paramValue(kPredelayId));
    engine_.setRevTimeSeconds(paramValue(kRevTimeId));
    engine_.setHighCut(paramValue(kHighCutId));
    engine_.setSize(paramValue(kSizeId));
    engine_.setPosition(paramValue(kPositionId));
    engine_.setPan(paramValue(kPanId));
    engine_.setEarlyMix(paramValue(kEarlyMixId));
    engine_.setDiffusion(paramValue(kDiffusionId));
    engine_.setMix(paramValue(kMixId));

    for (std::size_t i = 0; i < kAllpassDelayIds.size(); ++i)
    {
        engine_.setAllpassDelaySamples(static_cast<int>(i), paramValue(kAllpassDelayIds[i]));
    }
    for (std::size_t i = 0; i < kLineDelayIds.size(); ++i)
    {
        engine_.setLineDelayMs(static_cast<int>(i), paramValue(kLineDelayIds[i]));
        engine_.setLinePan(static_cast<int>(i), paramValue(kLinePanIds[i]));
        engine_.setLineLevel(static_cast<int>(i), paramValue(kLineLevelIds[i]));
    }

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideDenseRoomAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::denseRoomSchema());
}

void EventideDenseRoomAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideDenseRoomAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideDenseRoomAudioProcessor();
}
