#include "DiatonicShiftPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/DiatonicShiftSchema.h"

#include <span>

namespace
{
constexpr auto kGrainId = "grain";
constexpr auto kDelayId = "delay";
constexpr auto kKeyId = "key";
constexpr auto kScaleId = "scale";
constexpr auto kScaleDegreeId = "scaleDegree";
constexpr auto kRegenId = "regen";
constexpr auto kMixId = "mix";
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";
constexpr auto kWidthId = "width";

const juce::StringArray kKeyNames = { "C",  "C#", "D",  "D#", "E",  "F",
                                       "F#", "G",  "G#", "A",  "A#", "B" };
const juce::StringArray kScaleNames = { "Major", "Natural Minor", "Harmonic Minor", "Dorian",
                                         "Mixolydian" };

dsp::Scale scaleFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return dsp::Scale::kNaturalMinor;
        case 2:
            return dsp::Scale::kHarmonicMinor;
        case 3:
            return dsp::Scale::kDorian;
        case 4:
            return dsp::Scale::kMixolydian;
        case 0:
        default:
            return dsp::Scale::kMajor;
    }
}

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

EventideDiatonicShiftAudioProcessor::EventideDiatonicShiftAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideDiatonicShiftAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideDiatonicShiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kGrainId, "Grain", 0.01f, 0.3f, 0.07f, "s"));
    params.push_back(floatParam(kDelayId, "Delay", 0.0f, 1.0f, 0.4f, "s"));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kKeyId, 1 }, "Key", kKeyNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{ kScaleId, 1 }, "Scale", kScaleNames, 0));
    params.push_back(floatParam(kScaleDegreeId, "Scale Degree", -14.0f, 14.0f, 2.0f));
    params.push_back(floatParam(kRegenId, "Regen", 0.0f, 0.97f, 0.5f));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kWidthId, "Width", -360.0f, 360.0f, 0.0f, "deg"));

    return { params.begin(), params.end() };
}

void EventideDiatonicShiftAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::DiatonicShiftAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideDiatonicShiftAudioProcessor::releaseResources() {}

bool EventideDiatonicShiftAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideDiatonicShiftAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                         juce::MidiBuffer&)
{
    engine_.setGrainSeconds(paramValue(kGrainId));
    engine_.setDelaySeconds(paramValue(kDelayId));
    engine_.setKey(static_cast<int>(paramValue(kKeyId)));
    engine_.setScale(scaleFromIndex(static_cast<int>(paramValue(kScaleId))));
    engine_.setScaleDegree(static_cast<int>(paramValue(kScaleDegreeId)));
    engine_.setRegen(paramValue(kRegenId));
    engine_.setMix(paramValue(kMixId));
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));
    engine_.setWidth(paramValue(kWidthId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideDiatonicShiftAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::diatonicShiftSchema());
}

void EventideDiatonicShiftAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideDiatonicShiftAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideDiatonicShiftAudioProcessor();
}
