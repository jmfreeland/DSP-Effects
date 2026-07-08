#include "PhaserPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/PhaserSchema.h"

#include <functional>
#include <span>

namespace
{
using dsp::algorithms::Phaser;

constexpr auto kMixId = "mix";
constexpr auto kFeedbackId = "feedback";
constexpr auto kSweepRateId = "sweepRate";
constexpr auto kEnvelopeDecayRateId = "envelopeDecayRate";
constexpr auto kAdsrRateScalerId = "adsrRateScaler";
constexpr auto kSweepModeId = "sweepMode";
constexpr auto kSweepBottomId = "sweepBottom";
constexpr auto kSweepTopId = "sweepTop";
constexpr auto kAdsrAttackRateId = "adsrAttackRate";
constexpr auto kAdsrDecayRateId = "adsrDecayRate";
constexpr auto kAdsrSustainLevelId = "adsrSustainLevel";
constexpr auto kAdsrReleaseRateId = "adsrReleaseRate";
constexpr auto kAdsrAttackThresholdId = "adsrAttackThreshold";
constexpr auto kAdsrReleaseThresholdId = "adsrReleaseThreshold";
constexpr auto kEnvelopeChannelId = "envelopeChannel";
constexpr auto kEnvelopeDecayShapeId = "envelopeDecayShape";
constexpr auto kTriggerId = "trigger";

const juce::StringArray kSweepModeNames = { "LFO", "Envelope", "ADSR" };

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

Phaser::SweepMode sweepModeFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return Phaser::SweepMode::kEnvelope;
        case 2:
            return Phaser::SweepMode::kAdsr;
        case 0:
        default:
            return Phaser::SweepMode::kLfo;
    }
}
}

EventidePhaserAudioProcessor::EventidePhaserAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventidePhaserAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventidePhaserAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kMixId, "Mix", 0.0f, 100.0f, 50.0f, "%"));
    params.push_back(floatParam(kFeedbackId, "Feedback", -100.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kSweepRateId, "Sweep Rate", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kEnvelopeDecayRateId, "Envelope Decay Rate", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kAdsrRateScalerId, "ADSR Rate Scaler", 0.0f, 100.0f, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kSweepModeId, 1 }, "Sweep Mode", kSweepModeNames, 0));
    params.push_back(floatParam(kSweepBottomId, "Sweep Bottom", 0.0f, 100.0f, 20.0f));
    params.push_back(floatParam(kSweepTopId, "Sweep Top", 0.0f, 100.0f, 60.0f));

    params.push_back(floatParam(kAdsrAttackRateId, "ADSR Attack Rate", 0.0f, 100.0f, 60.0f));
    params.push_back(floatParam(kAdsrDecayRateId, "ADSR Decay Rate", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kAdsrSustainLevelId, "ADSR Sustain Level", 0.0f, 100.0f, 60.0f));
    params.push_back(floatParam(kAdsrReleaseRateId, "ADSR Release Rate", 0.0f, 100.0f, 40.0f));
    params.push_back(floatParam(kAdsrAttackThresholdId, "ADSR Attack Threshold", 0.0f, 100.0f, 30.0f));
    params.push_back(floatParam(kAdsrReleaseThresholdId, "ADSR Release Threshold", 0.0f, 100.0f, 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kEnvelopeChannelId, 1 },
                                                                  "Envelope Channel = Right (sidechain)", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kEnvelopeDecayShapeId, 1 },
                                                                  "Envelope Decay Exponential", true));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kTriggerId, 1 }, "ADSR Trigger", false));

    return { params.begin(), params.end() };
}

void EventidePhaserAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::PhaserAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventidePhaserAudioProcessor::releaseResources() {}

bool EventidePhaserAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void EventidePhaserAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setMix(paramValue(kMixId));
    engine_.setFeedback(paramValue(kFeedbackId));
    engine_.setSweepRate(paramValue(kSweepRateId));
    engine_.setEnvelopeDecayRate(paramValue(kEnvelopeDecayRateId));
    engine_.setAdsrRateScaler(paramValue(kAdsrRateScalerId));
    engine_.setSweepMode(sweepModeFromIndex(static_cast<int>(paramValue(kSweepModeId))));
    engine_.setSweepBottom(paramValue(kSweepBottomId));
    engine_.setSweepTop(paramValue(kSweepTopId));
    engine_.setAdsrAttackRate(paramValue(kAdsrAttackRateId));
    engine_.setAdsrDecayRate(paramValue(kAdsrDecayRateId));
    engine_.setAdsrSustainLevel(paramValue(kAdsrSustainLevelId));
    engine_.setAdsrReleaseRate(paramValue(kAdsrReleaseRateId));
    engine_.setAdsrAttackThreshold(paramValue(kAdsrAttackThresholdId));
    engine_.setAdsrReleaseThreshold(paramValue(kAdsrReleaseThresholdId));
    engine_.setEnvelopeChannel(paramValue(kEnvelopeChannelId) >= 0.5f);
    engine_.setEnvelopeDecayShapeExponential(paramValue(kEnvelopeDecayShapeId) >= 0.5f);

    pollTrigger(apvts, kTriggerId, [this] { engine_.trigger(); });

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventidePhaserAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::phaserSchema());
}

void EventidePhaserAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventidePhaserAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventidePhaserAudioProcessor();
}
