#include "PluginProcessor.h"

#include <span>

namespace
{
constexpr auto kDecayId = "decay";
constexpr auto kLowRatioId = "lowRatio";
constexpr auto kCrossoverId = "crossover";
constexpr auto kDampingId = "damping";
constexpr auto kDiffusionId = "diffusion";
constexpr auto kSizeId = "size";
constexpr auto kPreDelayId = "preDelay";
constexpr auto kEarlyReflectionLevelId = "earlyReflectionLevel";
constexpr auto kSpinId = "spin";
constexpr auto kChorusId = "chorus";
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
    lowRatioParam_ = apvts.getRawParameterValue(kLowRatioId);
    crossoverParam_ = apvts.getRawParameterValue(kCrossoverId);
    dampingParam_ = apvts.getRawParameterValue(kDampingId);
    diffusionParam_ = apvts.getRawParameterValue(kDiffusionId);
    sizeParam_ = apvts.getRawParameterValue(kSizeId);
    preDelayParam_ = apvts.getRawParameterValue(kPreDelayId);
    earlyReflectionLevelParam_ = apvts.getRawParameterValue(kEarlyReflectionLevelId);
    spinParam_ = apvts.getRawParameterValue(kSpinId);
    chorusParam_ = apvts.getRawParameterValue(kChorusId);
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
      juce::ParameterID{ kLowRatioId, 1 },
      "Low Ratio",
      juce::NormalisableRange<float>(0.2f, 2.0f, 0.01f),
      1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kCrossoverId, 1 },
      "Crossover",
      juce::NormalisableRange<float>(100.0f, 2000.0f, 1.0f, 0.4f),
      400.0f,
      juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kDampingId, 1 }, "Damping", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kDiffusionId, 1 }, "Diffusion", juce::NormalisableRange<float>(0.0f, 1.0f),
      0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kSizeId, 1 }, "Size", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kPreDelayId, 1 },
      "Pre Delay",
      juce::NormalisableRange<float>(0.0f, 0.93f, 0.001f),
      0.0f,
      juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kEarlyReflectionLevelId, 1 }, "Early Reflections",
      juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kSpinId, 1 }, "Spin", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kChorusId, 1 }, "Chorus", juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kMixId, 1 }, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void LexiconHallAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::algorithms::ConcertHall::requiredWorkingBufferSize(), 0.0f);
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
    engine_.setLowRatio(lowRatioParam_->load());
    engine_.setCrossoverFrequency(crossoverParam_->load());
    engine_.setDamping(dampingParam_->load());
    engine_.setDiffusion(diffusionParam_->load());
    engine_.setSize(sizeParam_->load());
    engine_.setPreDelaySeconds(preDelayParam_->load());
    engine_.setEarlyReflectionLevel(earlyReflectionLevelParam_->load());
    engine_.setSpin(spinParam_->load());
    engine_.setChorus(chorusParam_->load());
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
