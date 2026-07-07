#include "SweptCombsPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/SweptCombsSchema.h"

#include <array>
#include <span>

namespace
{
constexpr auto kMasterDelayId = "masterDelay";
constexpr auto kMasterRateId = "masterRate";
constexpr auto kMasterDepthId = "masterDepth";
constexpr auto kMasterFeedbackId = "masterFeedback";
constexpr auto kWidthId = "width";
constexpr auto kMixId = "mix";
constexpr auto kStereoInputId = "stereoInput";
constexpr auto kRepeatId = "repeat";

// Per-line ("Tedium") parameter IDs, built at static-init time so all six
// lines share one naming scheme (line0Delay, line1Delay, ...).
constexpr int kNumLines = dsp::algorithms::SweptCombs::kNumLines;

juce::String lineParamId(const char* suffix, int line)
{
    return "line" + juce::String(line) + suffix;
}

std::unique_ptr<juce::AudioParameterFloat> floatParam(const juce::String& id, const juce::String& name,
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

EventideSweptCombsAudioProcessor::EventideSweptCombsAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideSweptCombsAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideSweptCombsAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kMasterDelayId, "Master Delay", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kMasterRateId, "Master Rate", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kMasterDepthId, "Master Depth", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kMasterFeedbackId, "Master Feedback", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kWidthId, "Width", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kStereoInputId, 1 }, "Stereo Input", true));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kRepeatId, 1 }, "Repeat", false));

    static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 41, 67, 93, 127, 163, 211 };
    static constexpr std::array<float, kNumLines> kDefaultRates = { 20, 35, 50, 65, 80, 95 };
    static constexpr std::array<float, kNumLines> kDefaultPans = { -1.0f, -0.6f, -0.2f, 0.2f, 0.6f, 1.0f };
    for (int i = 0; i < kNumLines; ++i)
    {
        auto idx = static_cast<std::size_t>(i);
        params.push_back(
          floatParam(lineParamId("Delay", i), "Line " + juce::String(i + 1) + " Delay", 0.0f, 250.0f,
                     kDefaultDelaysMs[idx], "ms"));
        params.push_back(floatParam(lineParamId("Rate", i), "Line " + juce::String(i + 1) + " Rate", 0.0f,
                                     100.0f, kDefaultRates[idx]));
        params.push_back(
          floatParam(lineParamId("Depth", i), "Line " + juce::String(i + 1) + " Depth", 0.0f, 100.0f, 30.0f));
        params.push_back(floatParam(lineParamId("Feedback", i), "Line " + juce::String(i + 1) + " Feedback",
                                     -1.0f, 1.0f, 0.2f));
        params.push_back(floatParam(lineParamId("Pan", i), "Line " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                     kDefaultPans[idx]));
        params.push_back(
          floatParam(lineParamId("Level", i), "Line " + juce::String(i + 1) + " Level", 0.0f, 1.0f, 0.8f));
    }

    return { params.begin(), params.end() };
}

void EventideSweptCombsAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::SweptCombsAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideSweptCombsAudioProcessor::releaseResources() {}

bool EventideSweptCombsAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideSweptCombsAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setMasterDelay(paramValue(kMasterDelayId));
    engine_.setMasterRate(paramValue(kMasterRateId));
    engine_.setMasterDepth(paramValue(kMasterDepthId));
    engine_.setMasterFeedback(paramValue(kMasterFeedbackId));
    engine_.setWidth(paramValue(kWidthId));
    engine_.setMix(paramValue(kMixId));
    engine_.setStereoInput(paramValue(kStereoInputId) >= 0.5f);
    engine_.setRepeat(paramValue(kRepeatId) >= 0.5f);

    for (int i = 0; i < kNumLines; ++i)
    {
        engine_.setLineDelayMs(i, paramValue(lineParamId("Delay", i).toRawUTF8()));
        engine_.setLineRate(i, paramValue(lineParamId("Rate", i).toRawUTF8()));
        engine_.setLineDepth(i, paramValue(lineParamId("Depth", i).toRawUTF8()));
        engine_.setLineFeedback(i, paramValue(lineParamId("Feedback", i).toRawUTF8()));
        engine_.setLinePan(i, paramValue(lineParamId("Pan", i).toRawUTF8()));
        engine_.setLineLevel(i, paramValue(lineParamId("Level", i).toRawUTF8()));
    }

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideSweptCombsAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::sweptCombsSchema());
}

void EventideSweptCombsAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideSweptCombsAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideSweptCombsAudioProcessor();
}
