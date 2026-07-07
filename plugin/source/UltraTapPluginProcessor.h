#pragma once

#include "dsp/graphs/UltraTapAlgorithm.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

// JUCE VST3/Standalone wrapper around the same
// dsp::graphs::UltraTapAlgorithm engine used by the Polyend Endless
// patch, so DAW testing exercises identical DSP code. Unlike the
// pedal's 3 knobs, the plugin exposes the full per-tap ("Tedium") set
// for all 12 taps, the 4 Allpass delays, and the Quickset Spacing/
// Weights/Pans shape generators (applied once, on change, rather than
// continuously - matching the manual's own "presets Tedium" behavior).
class EventideUltraTapAudioProcessor : public juce::AudioProcessor
{
  public:
    EventideUltraTapAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

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

    dsp::graphs::UltraTapAlgorithm engine_;
    std::vector<float> workingBuffer_;
    int lastSpacingShape_ = -1;
    int lastWeightsShape_ = -1;
    int lastPansShape_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventideUltraTapAudioProcessor)
};
