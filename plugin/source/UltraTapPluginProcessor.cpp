#include "UltraTapPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/UltraTapSchema.h"

#include <array>
#include <span>

namespace
{
constexpr auto kLengthId = "length";
constexpr auto kDiffusionId = "diffusion";
constexpr auto kWidthId = "width";
constexpr auto kFeedbackId = "feedback";
constexpr auto kFbTapId = "fbTap";
constexpr auto kMixId = "mix";
constexpr auto kStereoInputId = "stereoInput";
constexpr auto kSpacingShapeId = "spacingShape";
constexpr auto kWeightsShapeId = "weightsShape";
constexpr auto kPansShapeId = "pansShape";
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";

constexpr int kNumTaps = dsp::algorithms::UltraTap::kNumTaps;
constexpr int kNumAllpassStages = dsp::algorithms::UltraTap::kNumAllpassStages;

const juce::StringArray kShapeNames = { "Constant",          "Linear Increasing", "Linear Decreasing",
                                         "Exponential Increasing", "Exponential Decreasing", "Random" };
const juce::StringArray kPanShapeNames = { "Center",       "Left",          "Right",     "Sweep L to R",
                                            "Sweep R to L", "Spread from Center", "Merge to Center",
                                            "Alternating",  "Random" };

juce::String tapParamId(const char* suffix, int tap)
{
    return "tap" + juce::String(tap) + suffix;
}

juce::String allpassParamId(int stage)
{
    return "allpass" + juce::String(stage) + "Delay";
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

EventideUltraTapAudioProcessor::EventideUltraTapAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideUltraTapAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideUltraTapAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kLengthId, "Length", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kWidthId, "Width", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kFeedbackId, "Feedback", -1.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kFbTapId, "Fb Tap", 1.0f, 12.0f, 12.0f));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kStereoInputId, 1 },
                                                                  "Stereo Input", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ kSpacingShapeId, 1 },
                                                                    "Spacing Shape", kShapeNames, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ kWeightsShapeId, 1 },
                                                                    "Weights Shape", kShapeNames, 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ kPansShapeId, 1 },
                                                                    "Pans Shape", kPanShapeNames, 5));
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));

    static constexpr std::array<float, kNumAllpassStages> kDefaultAllpassMs = { 20.0f, 15.0f, 11.0f, 7.0f };
    for (int i = 0; i < kNumAllpassStages; ++i)
    {
        params.push_back(floatParam(allpassParamId(i), "Allpass " + juce::String(i + 1) + " Delay", 0.0f,
                                     800.0f, kDefaultAllpassMs[static_cast<std::size_t>(i)], "ms"));
    }

    for (int i = 0; i < kNumTaps; ++i)
    {
        params.push_back(floatParam(tapParamId("Delay", i), "Tap " + juce::String(i + 1) + " Delay", 0.0f,
                                     1450.0f / kNumTaps * 2.0f, 1400.0f / kNumTaps, "ms"));
        params.push_back(
          floatParam(tapParamId("Level", i), "Tap " + juce::String(i + 1) + " Level", 0.0f, 1.0f, 0.7f));
        params.push_back(
          floatParam(tapParamId("Pan", i), "Tap " + juce::String(i + 1) + " Pan", -1.0f, 1.0f, 0.0f));
    }

    return { params.begin(), params.end() };
}

void EventideUltraTapAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::UltraTapAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
    lastSpacingShape_ = lastWeightsShape_ = lastPansShape_ = -1;
}

void EventideUltraTapAudioProcessor::releaseResources() {}

bool EventideUltraTapAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideUltraTapAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setLength(paramValue(kLengthId));
    engine_.setDiffusion(paramValue(kDiffusionId));
    engine_.setWidth(paramValue(kWidthId));
    engine_.setFeedback(paramValue(kFeedbackId));
    engine_.setFbTap(static_cast<int>(paramValue(kFbTapId) + 0.5f));
    engine_.setMix(paramValue(kMixId));
    engine_.setStereoInput(paramValue(kStereoInputId) >= 0.5f);
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));

    for (int i = 0; i < kNumAllpassStages; ++i)
    {
        engine_.setAllpassDelayMs(i, paramValue(allpassParamId(i).toRawUTF8()));
    }
    for (int i = 0; i < kNumTaps; ++i)
    {
        engine_.setTapDelayMs(i, paramValue(tapParamId("Delay", i).toRawUTF8()));
        engine_.setTapLevel(i, paramValue(tapParamId("Level", i).toRawUTF8()));
        engine_.setTapPan(i, paramValue(tapParamId("Pan", i).toRawUTF8()));
    }

    // Quickset shapes are one-shot presets applied on change, not a
    // continuous transform - matches the manual's own "presets Tedium"
    // description (see UltraTap.h's applySpacingShape() doc comment).
    auto spacingShape = static_cast<int>(paramValue(kSpacingShapeId));
    if (spacingShape != lastSpacingShape_)
    {
        engine_.applySpacingShape(static_cast<dsp::algorithms::UltraTap::Shape>(spacingShape));
        lastSpacingShape_ = spacingShape;
    }
    auto weightsShape = static_cast<int>(paramValue(kWeightsShapeId));
    if (weightsShape != lastWeightsShape_)
    {
        engine_.applyWeightsShape(static_cast<dsp::algorithms::UltraTap::Shape>(weightsShape));
        lastWeightsShape_ = weightsShape;
    }
    auto pansShape = static_cast<int>(paramValue(kPansShapeId));
    if (pansShape != lastPansShape_)
    {
        engine_.applyPansShape(static_cast<dsp::algorithms::UltraTap::PanShape>(pansShape));
        lastPansShape_ = pansShape;
    }

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideUltraTapAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::ultraTapSchema());
}

void EventideUltraTapAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideUltraTapAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideUltraTapAudioProcessor();
}
