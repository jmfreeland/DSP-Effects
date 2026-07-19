#include "VSOChmbPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/VSOChmbSchema.h"

#include <span>

namespace
{
constexpr auto kSendsId = "sends";
constexpr auto kReturnsId = "returns";
constexpr auto kRoutingId = "routing";
constexpr auto kRvbInLevelId = "rvbInLevel";
constexpr auto kFxInLevelId = "fxInLevel";
constexpr auto kRvbMixId = "rvbMix";
constexpr auto kFxMixId = "fxMix";

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

constexpr auto kVarispeedId = "varispeed";
constexpr auto kShiftDelayId = "shiftDelay";
constexpr auto kShiftFeedbackId = "shiftFeedback";
constexpr auto kSpliceId = "splice";
constexpr auto kFxWidthId = "fxWidth";
constexpr auto kHiCutId = "hiCut";
constexpr auto kFxAdjustId = "fxAdjust";
constexpr auto kMixId = "mix";

const juce::StringArray kSendsNames { "Stereo", "L=Rvb, R=FX", "Mono", "L=FX, R=Rvb" };
const juce::StringArray kReturnsNames { "Stereo", "Rvb=L, FX=R", "Mono", "FX=L, Rvb=R" };
const juce::StringArray kRoutingNames { "Parallel", "Rvb into FX", "FX into Rvb" };

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

VSOChmbAudioProcessor::VSOChmbAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float VSOChmbAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout VSOChmbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // -- Submixer --
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kSendsId, 1 }, "Sends", kSendsNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kReturnsId, 1 }, "Returns", kReturnsNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kRoutingId, 1 }, "Routing", kRoutingNames, 0));
    params.push_back(floatParam(kRvbInLevelId, "Rvb In Lvl", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kFxInLevelId, "FX In Lvl", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kRvbMixId, "Rvb Mix", 0.0f, 1.0f, 0.8f));
    params.push_back(floatParam(kFxMixId, "FX Mix", 0.0f, 1.0f, 1.0f));

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

    // -- Stereo Shifter (driven by Varispeed) --
    params.push_back(floatParam(kVarispeedId, "Varispeed", -35.0f, 55.0f, 0.0f, "%"));
    params.push_back(floatParam(kShiftDelayId, "Shift Delay", 0.0f, 0.5f, 0.02f, "s"));
    params.push_back(floatParam(kShiftFeedbackId, "Shift Fbk", 0.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kSpliceId, "Splice", 0.001f, 0.05f, 0.004f, "s"));

    params.push_back(floatParam(kFxWidthId, "FX Width", -360.0f, 360.0f, 45.0f, "deg"));
    params.push_back(floatParam(kHiCutId, "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
    params.push_back(floatParam(kFxAdjustId, "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

void VSOChmbAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::VSOChmbAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void VSOChmbAudioProcessor::releaseResources() {}

bool VSOChmbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VSOChmbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setSends(static_cast<dsp::Submixer::Sends>(static_cast<int>(paramValue(kSendsId))));
    engine_.setReturns(static_cast<dsp::Submixer::Returns>(static_cast<int>(paramValue(kReturnsId))));
    engine_.setRouting(
      static_cast<dsp::graphs::VSOChmbAlgorithm::Routing>(static_cast<int>(paramValue(kRoutingId))));
    engine_.setRvbInLevel(paramValue(kRvbInLevelId));
    engine_.setFxInLevel(paramValue(kFxInLevelId));
    engine_.setRvbMix(paramValue(kRvbMixId));
    engine_.setFxMix(paramValue(kFxMixId));

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

    engine_.setVarispeed(paramValue(kVarispeedId));
    engine_.setShiftDelaySeconds(paramValue(kShiftDelayId));
    engine_.setShiftFeedback(paramValue(kShiftFeedbackId));
    engine_.setSpliceSeconds(paramValue(kSpliceId));

    engine_.setFxWidth(paramValue(kFxWidthId));
    engine_.setHiCut(paramValue(kHiCutId));
    engine_.setFxAdjustDb(paramValue(kFxAdjustId));
    engine_.setMix(paramValue(kMixId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* VSOChmbAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::vsoChmbSchema());
}

void VSOChmbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void VSOChmbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new VSOChmbAudioProcessor();
}
