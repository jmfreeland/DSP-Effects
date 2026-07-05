#include "PluginProcessor.h"

#include <span>

namespace
{
constexpr auto kDecayId = "decay";
constexpr auto kDampingId = "damping";
constexpr auto kMixId = "mix";
constexpr auto kFreezeId = "freeze";
}

LexiconHallAudioProcessor::LexiconHallAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    decayParam_ = apvts.getRawParameterValue(kDecayId);
    dampingParam_ = apvts.getRawParameterValue(kDampingId);
    mixParam_ = apvts.getRawParameterValue(kMixId);
    freezeParam_ = apvts.getRawParameterValue(kFreezeId);
}

juce::AudioProcessorValueTreeState::ParameterLayout
LexiconHallAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kDecayId, 1 },
      "Decay",
      juce::NormalisableRange<float>(0.3f, 8.0f, 0.01f, 0.5f),
      2.5f,
      juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kDampingId, 1 }, "Damping", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kMixId, 1 }, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void LexiconHallAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::algorithms::LexiconHall::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void LexiconHallAudioProcessor::releaseResources() {}

bool LexiconHallAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LexiconHallAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setDecaySeconds(decayParam_->load());
    engine_.setDamping(dampingParam_->load());
    engine_.setMix(mixParam_->load());
    engine_.setFrozen(freezeParam_->load() >= 0.5f);

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* LexiconHallAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void LexiconHallAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void LexiconHallAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new LexiconHallAudioProcessor();
}
