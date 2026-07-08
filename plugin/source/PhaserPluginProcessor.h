#pragma once

#include "dsp/graphs/PhaserAlgorithm.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

// JUCE VST3/Standalone wrapper around the same
// dsp::graphs::PhaserAlgorithm engine used by the Polyend Endless patch,
// so DAW testing exercises identical DSP code. Exposes all three sweep
// modes (LFO/Envelope/ADSR), the ADSR's own rates/thresholds, the
// Envelope Channel sidechain option, and a manual ADSR trigger button -
// everything the hardware's 3 knobs can't reach.
class EventidePhaserAudioProcessor : public juce::AudioProcessor
{
  public:
    EventidePhaserAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

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

    dsp::graphs::PhaserAlgorithm engine_;
    std::vector<float> workingBuffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventidePhaserAudioProcessor)
};
