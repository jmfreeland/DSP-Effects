#include "PitchCorrectPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/PitchCorrectSchema.h"

#include <span>

namespace
{
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";
constexpr auto kDelayId = "delay";
constexpr auto kLowPitchId = "lowPitch";
constexpr auto kHighPitchId = "highPitch";
constexpr auto kTuningId = "tuning";
constexpr auto kCorrectionId = "correction";
constexpr auto kTrackingId = "tracking";
constexpr auto kGrainId = "grain";
constexpr auto kShiftCentsId = "shiftCents";
constexpr auto kShiftSemitonesId = "shiftSemitones";

constexpr auto kDecayId = "decay";
constexpr auto kLowRatioId = "lowRatio";
constexpr auto kCrossoverId = "crossover";
constexpr auto kDampingId = "damping";
constexpr auto kDiffusionId = "diffusion";
constexpr auto kSizeId = "size";
constexpr auto kLinkId = "link";
constexpr auto kShapeId = "shape";
constexpr auto kSpreadId = "spread";
constexpr auto kRvbInId = "rvbIn";
constexpr auto kRvbOutId = "rvbOut";
constexpr auto kPreDelayId = "preDelay";
constexpr auto kEarlyReflectionLevelLeftId = "earlyReflectionLevelLeft";
constexpr auto kEarlyReflectionLevelRightId = "earlyReflectionLevelRight";
constexpr auto kEarlyReflectionDelayLeftId = "earlyReflectionDelayLeft";
constexpr auto kEarlyReflectionDelayRightId = "earlyReflectionDelayRight";
constexpr auto kSpinId = "spin";
constexpr auto kEkoFeedbackLeftId = "ekoFeedbackLeft";
constexpr auto kEkoFeedbackRightId = "ekoFeedbackRight";
constexpr auto kEkoDelayLeftId = "ekoDelayLeft";
constexpr auto kEkoDelayRightId = "ekoDelayRight";

constexpr auto kFxMixId = "fxMix";
constexpr auto kFxWidthId = "fxWidth";
constexpr auto kHiCutId = "hiCut";
constexpr auto kFxAdjustId = "fxAdjust";
constexpr auto kMixId = "mix";

const juce::StringArray kTrackingNames { "Fastest", "Fast", "Moderate", "Slow", "Hold" };

std::unique_ptr<juce::AudioParameterFloat> floatParam(const char* id, const juce::String& name,
                                                        float min, float max, float defaultValue,
                                                        const char* label = nullptr,
                                                        float skew = 1.0f)
{
    juce::AudioParameterFloatAttributes attributes;
    if (label != nullptr)
    {
        attributes = attributes.withLabel(label);
    }
    auto interval = (max - min) / 10000.0f;
    return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ id, 1 }, name, juce::NormalisableRange<float>(min, max, interval, skew),
      defaultValue, attributes);
}
}

PitchCorrectAudioProcessor::PitchCorrectAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float PitchCorrectAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchCorrectAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // -- Input / pitch detection & correction --
    params.push_back(floatParam(kInLevelLeftId, "In Lvl L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Lvl R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kDelayId, "Delay", 0.0f, 0.1f, 0.01f, "s"));
    params.push_back(floatParam(kLowPitchId, "Low Pitch", 30.0f, 1000.0f, 80.0f, "Hz", 0.4f));
    params.push_back(floatParam(kHighPitchId, "High Pitch", 30.0f, 2000.0f, 800.0f, "Hz", 0.4f));
    params.push_back(floatParam(kTuningId, "Tuning", 410.0f, 470.0f, 440.0f, "Hz"));
    params.push_back(floatParam(kCorrectionId, "Correction", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kTrackingId, 1 }, "Tracking", kTrackingNames, 0));
    params.push_back(floatParam(kGrainId, "Grain", 0.001f, 0.05f, 0.008f, "s"));
    params.push_back(floatParam(kShiftCentsId, "Shift Cents", -100.0f, 100.0f, 0.0f));
    params.push_back(floatParam(kShiftSemitonesId, "Shift Semitones", -24.0f, 24.0f, 0.0f));

    // -- Chamber reverb block --
    params.push_back(floatParam(kDecayId, "Decay", 0.3f, 8.0f, 2.0f, "s", 0.5f));
    params.push_back(floatParam(kLowRatioId, "Low Ratio", 0.2f, 2.0f, 1.0f));
    params.push_back(floatParam(kCrossoverId, "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
    params.push_back(floatParam(kDampingId, "Damping", 0.0f, 1.0f, 0.4f));
    params.push_back(floatParam(kDiffusionId, "Diffusion", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kSizeId, "Size", 0.0f, 1.0f, 0.5f));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kLinkId, 1 }, "Link", false));
    params.push_back(floatParam(kShapeId, "Shape", 0.0f, 1.0f, 0.3f));
    params.push_back(floatParam(kSpreadId, "Spread", 0.0f, 1.0f, 0.4f));
    params.push_back(floatParam(kRvbInId, "Rvb In", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kRvbOutId, "Rvb Out", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kPreDelayId, "Pre Delay", 0.0f, 0.93f, 0.0f, "s"));
    params.push_back(floatParam(kEarlyReflectionLevelLeftId, "Early Reflections L", 0.0f, 1.0f, 0.2f));
    params.push_back(floatParam(kEarlyReflectionLevelRightId, "Early Reflections R", 0.0f, 1.0f, 0.2f));
    params.push_back(
      floatParam(kEarlyReflectionDelayLeftId, "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
    params.push_back(
      floatParam(kEarlyReflectionDelayRightId, "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
    params.push_back(floatParam(kSpinId, "Spin", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kEkoFeedbackLeftId, "Eko Fbk L", 0.0f, 0.95f, 0.2f));
    params.push_back(floatParam(kEkoFeedbackRightId, "Eko Fbk R", 0.0f, 0.95f, 0.2f));
    params.push_back(floatParam(kEkoDelayLeftId, "Eko Dly L", 0.0f, 1.2f, 0.06f, "s"));
    params.push_back(floatParam(kEkoDelayRightId, "Eko Dly R", 0.0f, 1.2f, 0.07f, "s"));

    // -- Output --
    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 0.0f));
    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

void PitchCorrectAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::PitchCorrectAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void PitchCorrectAudioProcessor::releaseResources() {}

bool PitchCorrectAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PitchCorrectAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));
    engine_.setDelaySeconds(paramValue(kDelayId));
    engine_.setPitchRange(paramValue(kLowPitchId), paramValue(kHighPitchId));
    engine_.setTuning(paramValue(kTuningId));
    engine_.setCorrection(paramValue(kCorrectionId));
    engine_.setTracking(
      static_cast<dsp::graphs::PitchCorrectAlgorithm::Tracking>(static_cast<int>(paramValue(kTrackingId))));
    engine_.setGrainSeconds(paramValue(kGrainId));
    engine_.setShiftCents(paramValue(kShiftCentsId));
    engine_.setShiftSemitones(static_cast<int>(paramValue(kShiftSemitonesId)));

    engine_.setDecaySeconds(paramValue(kDecayId));
    engine_.setLowRatio(paramValue(kLowRatioId));
    engine_.setCrossoverFrequency(paramValue(kCrossoverId));
    engine_.setDamping(paramValue(kDampingId));
    engine_.setDiffusion(paramValue(kDiffusionId));
    engine_.setSize(paramValue(kSizeId));
    engine_.setLink(paramValue(kLinkId) >= 0.5f);
    engine_.setShape(paramValue(kShapeId));
    engine_.setSpread(paramValue(kSpreadId));
    engine_.setRvbIn(paramValue(kRvbInId));
    engine_.setRvbOut(paramValue(kRvbOutId));
    engine_.setPreDelaySeconds(paramValue(kPreDelayId));
    engine_.setEarlyReflectionLevel(paramValue(kEarlyReflectionLevelLeftId),
                                     paramValue(kEarlyReflectionLevelRightId));
    engine_.setEarlyReflectionDelaySeconds(paramValue(kEarlyReflectionDelayLeftId),
                                            paramValue(kEarlyReflectionDelayRightId));
    engine_.setSpin(paramValue(kSpinId));
    engine_.setEkoFeedback(paramValue(kEkoFeedbackLeftId), paramValue(kEkoFeedbackRightId));
    engine_.setEkoDelaySeconds(paramValue(kEkoDelayLeftId), paramValue(kEkoDelayRightId));

    engine_.setFxMix(paramValue(kFxMixId));
    engine_.setFxWidth(paramValue(kFxWidthId));
    engine_.setHiCut(paramValue(kHiCutId));
    engine_.setFxAdjustDb(paramValue(kFxAdjustId));
    engine_.setMix(paramValue(kMixId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* PitchCorrectAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::pitchCorrectSchema());
}

void PitchCorrectAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void PitchCorrectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new PitchCorrectAudioProcessor();
}
