#include "StutterPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/StutterSchema.h"

#include <functional>
#include <span>

namespace
{
using dsp::algorithms::Stutter;

constexpr auto kLength1Id = "length1";
constexpr auto kLength2Id = "length2";
constexpr auto kCount1Id = "count1";
constexpr auto kCount2Id = "count2";
constexpr auto kLeftCentsId = "leftCents";
constexpr auto kRightCentsId = "rightCents";
constexpr auto kLeftDelayId = "leftDelay";
constexpr auto kRightDelayId = "rightDelay";
constexpr auto kLeftFeedbackId = "leftFeedback";
constexpr auto kRightFeedbackId = "rightFeedback";
constexpr auto kUp1RateId = "up1Rate";
constexpr auto kUp1MaxId = "up1Max";
constexpr auto kDn1RateId = "dn1Rate";
constexpr auto kDn1MinId = "dn1Min";
constexpr auto kUp2RateId = "up2Rate";
constexpr auto kUp2MaxId = "up2Max";
constexpr auto kDn2RateId = "dn2Rate";
constexpr auto kDn2MinId = "dn2Min";
constexpr auto kRand1MaxId = "rand1Max";
constexpr auto kRand2MaxId = "rand2Max";
constexpr auto kSweepTarget1Id = "sweepTarget1";
constexpr auto kSweepTarget2Id = "sweepTarget2";
constexpr auto kLeftMixId = "leftMix";
constexpr auto kRightMixId = "rightMix";
constexpr auto kAutoId = "autoOn";
constexpr auto kSpeedId = "speed";
constexpr auto kProgramId = "program";

constexpr auto kTriggerStutter1Id = "triggerStutter1";
constexpr auto kTriggerStutter2Id = "triggerStutter2";
constexpr auto kTriggerSweepUp1Id = "triggerSweepUp1";
constexpr auto kTriggerSweepDown1Id = "triggerSweepDown1";
constexpr auto kTriggerRandomPitch1Id = "triggerRandomPitch1";
constexpr auto kTriggerSweepUp2Id = "triggerSweepUp2";
constexpr auto kTriggerSweepDown2Id = "triggerSweepDown2";
constexpr auto kTriggerRandomPitch2Id = "triggerRandomPitch2";

const juce::StringArray kSweepTargetNames = { "None", "Left", "Right", "Both" };
const juce::StringArray kProgramNames = { "Total Random", "Random Sweep", "Random Pitch", "Just Stutter" };

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

Stutter::SweepTarget sweepTargetFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return Stutter::SweepTarget::kLeft;
        case 2:
            return Stutter::SweepTarget::kRight;
        case 3:
            return Stutter::SweepTarget::kBoth;
        case 0:
        default:
            return Stutter::SweepTarget::kNone;
    }
}

Stutter::Program programFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return Stutter::Program::kRandomSweep;
        case 2:
            return Stutter::Program::kRandomPitch;
        case 3:
            return Stutter::Program::kJustStutter;
        case 0:
        default:
            return Stutter::Program::kTotalRandom;
    }
}
}

EventideStutterAudioProcessor::EventideStutterAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideStutterAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideStutterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kLength1Id, "Length 1", 0.0f, 0.5f, 0.1f, "s"));
    params.push_back(floatParam(kLength2Id, "Length 2", 0.0f, 0.5f, 0.05f, "s"));
    params.push_back(floatParam(kCount1Id, "Count 1", 0.0f, 16.0f, 4.0f));
    params.push_back(floatParam(kCount2Id, "Count 2", 0.0f, 16.0f, 8.0f));
    params.push_back(floatParam(kLeftCentsId, "Left Coarse/Fine", -4800.0f, 1200.0f, 0.0f, "cents"));
    params.push_back(floatParam(kRightCentsId, "Right Coarse/Fine", -4800.0f, 1200.0f, 0.0f, "cents"));
    params.push_back(floatParam(kLeftDelayId, "Left Delay", 0.0f, 0.5f, 0.0f, "s"));
    params.push_back(floatParam(kRightDelayId, "Right Delay", 0.0f, 0.5f, 0.0f, "s"));
    params.push_back(floatParam(kLeftFeedbackId, "Left Feedback", 0.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kRightFeedbackId, "Right Feedback", 0.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam(kUp1RateId, "Up 1 Rate", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kUp1MaxId, "Up 1 Max", 0.0f, 2400.0f, 1200.0f, "cents"));
    params.push_back(floatParam(kDn1RateId, "Dn 1 Rate", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kDn1MinId, "Dn 1 Min", -2400.0f, 0.0f, -1200.0f, "cents"));
    params.push_back(floatParam(kUp2RateId, "Up 2 Rate", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kUp2MaxId, "Up 2 Max", 0.0f, 2400.0f, 1200.0f, "cents"));
    params.push_back(floatParam(kDn2RateId, "Dn 2 Rate", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kDn2MinId, "Dn 2 Min", -2400.0f, 0.0f, -1200.0f, "cents"));
    params.push_back(floatParam(kRand1MaxId, "Rand 1 Max", -1200.0f, 1200.0f, 1200.0f, "cents"));
    params.push_back(floatParam(kRand2MaxId, "Rand 2 Max", -1200.0f, 1200.0f, 1200.0f, "cents"));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kSweepTarget1Id, 1 }, "Sweep 1 Target", kSweepTargetNames, 1)); // Left
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kSweepTarget2Id, 1 }, "Sweep 2 Target", kSweepTargetNames, 2)); // Right
    params.push_back(floatParam(kLeftMixId, "Left Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kRightMixId, "Right Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kAutoId, 1 }, "Auto", false));
    params.push_back(floatParam(kSpeedId, "Speed", 0.0f, 100.0f, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kProgramId, 1 }, "Program", kProgramNames, 0));

    params.push_back(triggerParam(kTriggerStutter1Id, "Trigger Stutter 1"));
    params.push_back(triggerParam(kTriggerStutter2Id, "Trigger Stutter 2"));
    params.push_back(triggerParam(kTriggerSweepUp1Id, "Trigger Sweep Up 1"));
    params.push_back(triggerParam(kTriggerSweepDown1Id, "Trigger Sweep Down 1"));
    params.push_back(triggerParam(kTriggerRandomPitch1Id, "Trigger Random Pitch 1"));
    params.push_back(triggerParam(kTriggerSweepUp2Id, "Trigger Sweep Up 2"));
    params.push_back(triggerParam(kTriggerSweepDown2Id, "Trigger Sweep Down 2"));
    params.push_back(triggerParam(kTriggerRandomPitch2Id, "Trigger Random Pitch 2"));

    return { params.begin(), params.end() };
}

void EventideStutterAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::StutterAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideStutterAudioProcessor::releaseResources() {}

bool EventideStutterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

namespace
{
// A momentary trigger checkbox: fires `action` once when the UI sets it
// true, then snaps itself back to false so the next click fires again.
void pollTrigger(juce::AudioProcessorValueTreeState& apvts, const char* id,
                  const std::function<void()>& action)
{
    auto* param = apvts.getParameter(id);
    if (param->getValue() >= 0.5f)
    {
        action();
        param->setValueNotifyingHost(0.0f);
    }
}
}

void EventideStutterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setLength1(paramValue(kLength1Id));
    engine_.setLength2(paramValue(kLength2Id));
    engine_.setCount1(static_cast<int>(paramValue(kCount1Id) + 0.5f));
    engine_.setCount2(static_cast<int>(paramValue(kCount2Id) + 0.5f));
    engine_.setLeftCoarseFineCents(paramValue(kLeftCentsId));
    engine_.setRightCoarseFineCents(paramValue(kRightCentsId));
    engine_.setLeftDelaySeconds(paramValue(kLeftDelayId));
    engine_.setRightDelaySeconds(paramValue(kRightDelayId));
    engine_.setLeftFeedback(paramValue(kLeftFeedbackId));
    engine_.setRightFeedback(paramValue(kRightFeedbackId));
    engine_.setUp1(paramValue(kUp1RateId), paramValue(kUp1MaxId));
    engine_.setDn1(paramValue(kDn1RateId), paramValue(kDn1MinId));
    engine_.setUp2(paramValue(kUp2RateId), paramValue(kUp2MaxId));
    engine_.setDn2(paramValue(kDn2RateId), paramValue(kDn2MinId));
    engine_.setRand1Max(paramValue(kRand1MaxId));
    engine_.setRand2Max(paramValue(kRand2MaxId));
    engine_.setSweepTarget1(sweepTargetFromIndex(static_cast<int>(paramValue(kSweepTarget1Id))));
    engine_.setSweepTarget2(sweepTargetFromIndex(static_cast<int>(paramValue(kSweepTarget2Id))));
    engine_.setLeftMix(paramValue(kLeftMixId));
    engine_.setRightMix(paramValue(kRightMixId));
    engine_.setAuto(paramValue(kAutoId) >= 0.5f);
    engine_.setSpeed(paramValue(kSpeedId));
    engine_.setProgram(programFromIndex(static_cast<int>(paramValue(kProgramId))));

    pollTrigger(apvts, kTriggerStutter1Id, [this] { engine_.triggerStutter1(); });
    pollTrigger(apvts, kTriggerStutter2Id, [this] { engine_.triggerStutter2(); });
    pollTrigger(apvts, kTriggerSweepUp1Id, [this] { engine_.triggerSweepUp1(); });
    pollTrigger(apvts, kTriggerSweepDown1Id, [this] { engine_.triggerSweepDown1(); });
    pollTrigger(apvts, kTriggerRandomPitch1Id, [this] { engine_.triggerRandomPitch1(); });
    pollTrigger(apvts, kTriggerSweepUp2Id, [this] { engine_.triggerSweepUp2(); });
    pollTrigger(apvts, kTriggerSweepDown2Id, [this] { engine_.triggerSweepDown2(); });
    pollTrigger(apvts, kTriggerRandomPitch2Id, [this] { engine_.triggerRandomPitch2(); });

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideStutterAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::stutterSchema());
}

void EventideStutterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideStutterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideStutterAudioProcessor();
}
