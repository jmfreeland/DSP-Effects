#pragma once

#include "dsp/graphs/PatchFactoryAlgorithm.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

// JUCE VST3/Standalone wrapper around the same
// dsp::graphs::PatchFactoryAlgorithm engine used by the Polyend Endless
// patch, so DAW testing exercises identical DSP code. Unlike the
// hardware's 3 knobs, this plugin exposes the full patch matrix (13
// destination choices over 16 sources) alongside every basic-element
// parameter, since the patch matrix is the whole point of this
// algorithm.
class EventidePatchFactoryAudioProcessor : public juce::AudioProcessor
{
  public:
    EventidePatchFactoryAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

  private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float paramValue(const char* id) const;

    dsp::graphs::PatchFactoryAlgorithm engine_;
    std::vector<float> workingBuffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventidePatchFactoryAudioProcessor)
};
