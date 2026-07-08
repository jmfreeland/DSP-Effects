#include "BandDelayPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/BandDelaySchema.h"

#include <array>
#include <span>

namespace
{
constexpr auto kGlobalDelayId = "globalDelay";
constexpr auto kGlobalFrequencyId = "globalFrequency";
constexpr auto kGlobalQId = "globalQ";
constexpr auto kGlobalPanId = "globalPan";
constexpr auto kFeedbackDelayId = "feedbackDelay";
constexpr auto kFeedbackId = "feedback";
constexpr auto kMixId = "mix";

constexpr std::array<const char*, 8> kBaseHzIds = { "baseHz1", "baseHz2", "baseHz3", "baseHz4",
                                                     "baseHz5", "baseHz6", "baseHz7", "baseHz8" };
constexpr std::array<const char*, 8> kCentsIds = { "cents1", "cents2", "cents3", "cents4",
                                                    "cents5", "cents6", "cents7", "cents8" };
constexpr std::array<const char*, 8> kQIds = { "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8" };
constexpr std::array<const char*, 8> kDelayIds = { "delay1", "delay2", "delay3", "delay4",
                                                    "delay5", "delay6", "delay7", "delay8" };
constexpr std::array<const char*, 8> kLevelIds = { "level1", "level2", "level3", "level4",
                                                    "level5", "level6", "level7", "level8" };
constexpr std::array<const char*, 8> kPanIds = { "pan1", "pan2", "pan3", "pan4", "pan5", "pan6", "pan7", "pan8" };

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

EventideBandDelayAudioProcessor::EventideBandDelayAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideBandDelayAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideBandDelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kGlobalDelayId, "Global Delay", 0.0f, 100.0f, 100.0f, "%"));
    params.push_back(floatParam(kGlobalFrequencyId, "Global Frequency", -128.0f, 128.0f, 0.0f, "st"));
    params.push_back(floatParam(kGlobalQId, "Global Q", 0.0f, 100.0f, 100.0f, "%"));
    params.push_back(floatParam(kGlobalPanId, "Global Pan", -1.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kFeedbackDelayId, "Feedback Delay", 0.0f, 1.485f, 0.3f, "s"));
    params.push_back(floatParam(kFeedbackId, "Feedback", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));

    static constexpr std::array<float, 8> kDefaultBaseHz = { 110.0f, 220.0f, 330.0f, 440.0f,
                                                               660.0f, 880.0f, 1320.0f, 2200.0f };
    static constexpr std::array<float, 8> kDefaultDelaysMs = { 83, 149, 227, 311, 401, 487, 571, 661 };
    static constexpr std::array<float, 8> kDefaultPans = { -1.0f, 1.0f, -0.6f, 0.6f, -0.3f, 0.3f, -0.1f, 0.1f };
    for (std::size_t i = 0; i < kBaseHzIds.size(); ++i)
    {
        auto n = juce::String(i + 1);
        params.push_back(floatParam(kBaseHzIds[i], "Base Hz " + n, 20.0f, 10000.0f, kDefaultBaseHz[i], "Hz"));
        params.push_back(floatParam(kCentsIds[i], "Cents " + n, 0.0f, 12800.0f, 0.0f, "ct"));
        params.push_back(floatParam(kQIds[i], "Q " + n, 0.0f, 999.0f, 999.0f));
        params.push_back(floatParam(kDelayIds[i], "Delay " + n, 0.0f, 1496.0f, kDefaultDelaysMs[i], "ms"));
        params.push_back(
          floatParam(kLevelIds[i], "Level " + n, -100.0f, 100.0f, (i % 2 == 0) ? 100.0f : -100.0f, "%"));
        params.push_back(floatParam(kPanIds[i], "Pan " + n, -1.0f, 1.0f, kDefaultPans[i]));
    }

    return { params.begin(), params.end() };
}

void EventideBandDelayAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::BandDelayAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideBandDelayAudioProcessor::releaseResources() {}

bool EventideBandDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideBandDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setGlobalDelay(paramValue(kGlobalDelayId));
    engine_.setGlobalFrequency(paramValue(kGlobalFrequencyId));
    engine_.setGlobalQ(paramValue(kGlobalQId));
    engine_.setGlobalPan(paramValue(kGlobalPanId));
    engine_.setFeedbackDelaySeconds(paramValue(kFeedbackDelayId));
    engine_.setFeedback(paramValue(kFeedbackId));
    engine_.setMix(paramValue(kMixId));

    for (std::size_t i = 0; i < kBaseHzIds.size(); ++i)
    {
        auto band = static_cast<int>(i);
        engine_.setFilterBaseHz(band, paramValue(kBaseHzIds[i]));
        engine_.setFilterFrequencyCents(band, paramValue(kCentsIds[i]));
        engine_.setFilterQ(band, paramValue(kQIds[i]));
        engine_.setFilterDelayMs(band, paramValue(kDelayIds[i]));
        engine_.setFilterLevel(band, paramValue(kLevelIds[i]));
        engine_.setFilterPan(band, paramValue(kPanIds[i]));
    }

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideBandDelayAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::bandDelaySchema());
}

void EventideBandDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideBandDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideBandDelayAudioProcessor();
}
