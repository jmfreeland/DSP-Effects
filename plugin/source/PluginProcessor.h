#pragma once

#include "dsp/algorithms/ConcertHall.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

// JUCE VST3/Standalone wrapper around the same dsp::algorithms::ConcertHall
// engine used by the Polyend Endless patch, so DAW testing (Ableton,
// Bitwig, ...) exercises identical DSP code. Unlike the pedal's 3 knobs,
// the plugin exposes the engine's full parameter set for deep editing.
class LexiconHallAudioProcessor : public juce::AudioProcessor
{
  public:
    LexiconHallAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

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

    dsp::algorithms::ConcertHall engine_;
    std::vector<float> workingBuffer_;

    std::atomic<float>* decayParam_ = nullptr;
    std::atomic<float>* lowRatioParam_ = nullptr;
    std::atomic<float>* crossoverParam_ = nullptr;
    std::atomic<float>* dampingParam_ = nullptr;
    std::atomic<float>* diffusionParam_ = nullptr;
    std::atomic<float>* sizeParam_ = nullptr;
    std::atomic<float>* preDelayParam_ = nullptr;
    std::atomic<float>* earlyReflectionLevelParam_ = nullptr;
    std::atomic<float>* spinParam_ = nullptr;
    std::atomic<float>* chorusParam_ = nullptr;
    std::atomic<float>* mixParam_ = nullptr;
    std::atomic<float>* freezeParam_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LexiconHallAudioProcessor)
};
