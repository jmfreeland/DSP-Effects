#pragma once

#include "dsp/graphs/StringModellerAlgorithm.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

// JUCE VST3/Standalone wrapper around the same
// dsp::graphs::StringModellerAlgorithm engine used by the Polyend Endless
// patch, so DAW testing exercises identical DSP code. Exposes the full
// stimulation/tuning/chorus parameter set the hardware's 3 knobs can't
// reach, plus a manual pluck trigger button standing in for the manual's
// MIDI note-on (see dsp/algorithms/StringModeller.h).
class EventideStringModellerAudioProcessor : public juce::AudioProcessor
{
  public:
    EventideStringModellerAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

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

    dsp::graphs::StringModellerAlgorithm engine_;
    std::vector<float> workingBuffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventideStringModellerAudioProcessor)
};
