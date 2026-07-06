#pragma once

#include "dsp/graphs/DiatonicShiftAlgorithm.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

// JUCE VST3/Standalone wrapper around the same
// dsp::graphs::DiatonicShiftAlgorithm engine used by the Polyend Endless
// patch, so DAW testing exercises identical DSP code. Unlike the pedal's
// 3 knobs, the plugin exposes the engine's full parameter set (including
// Scale, which the hardware patch fixes to Major for lack of a 4th knob).
class EventideDiatonicShiftAudioProcessor : public juce::AudioProcessor
{
  public:
    EventideDiatonicShiftAudioProcessor();

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

    dsp::graphs::DiatonicShiftAlgorithm engine_;
    std::vector<float> workingBuffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventideDiatonicShiftAudioProcessor)
};
