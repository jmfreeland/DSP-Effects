#include "LoomBrowserPluginProcessor.h"

#include "EngineRegistry.h"
#include "LoomBrowserPluginEditor.h"

#include <cmath>
#include <span>

LoomBrowserAudioProcessor::LoomBrowserAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LoomBrowserAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray names;
    for (int i = 0; i < loom::browser::engineRegistrySize(); ++i)
    {
        names.add(loom::browser::createAdapter(i)->displayName());
    }
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ kAlgorithmParamId, 1 },
                                                                   "Algorithm", names, 0));

    for (int i = 0; i < loom::browser::engineRegistrySize(); ++i)
    {
        auto adapterParams = loom::browser::createAdapter(i)->createParameters();
        for (auto& p : adapterParams)
        {
            params.push_back(std::move(p));
        }
    }

    return { params.begin(), params.end() };
}

void LoomBrowserAudioProcessor::switchTo(int index)
{
    activeAdapter_ = loom::browser::createAdapter(index);
    auto required = activeAdapter_->requiredWorkingBufferSize();
    jassert(required <= workingBuffer_.size());
    activeAdapter_->prepare(static_cast<float>(sampleRate_),
                            std::span<float>(workingBuffer_.data(), required));
    activeIndex_.store(index);
}

void LoomBrowserAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    sampleRate_ = sampleRate;
    workingBuffer_.assign(loom::browser::maxRequiredWorkingBufferSize(), 0.0f);
    auto index = static_cast<int>(std::lround(apvts.getRawParameterValue(kAlgorithmParamId)->load()));
    switchTo(index);
}

void LoomBrowserAudioProcessor::releaseResources() {}

bool LoomBrowserAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LoomBrowserAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto index = static_cast<int>(std::lround(apvts.getRawParameterValue(kAlgorithmParamId)->load()));
    if (index != activeIndex_.load())
    {
        switchTo(index);
    }

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    activeAdapter_->process(apvts, left, right);
}

juce::AudioProcessorEditor* LoomBrowserAudioProcessor::createEditor()
{
    return new LoomBrowserPluginEditor(*this);
}

void LoomBrowserAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void LoomBrowserAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new LoomBrowserAudioProcessor();
}
