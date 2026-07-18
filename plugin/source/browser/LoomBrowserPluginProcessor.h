#pragma once

#include "EngineAdapter.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <vector>

// The Loom browser plugin: a single AU/VST3/Standalone instance that can
// switch between any of the registered algorithms (see EngineRegistry.h)
// without the host ever seeing its parameter list change - every
// algorithm's parameters exist in the one shared APVTS from construction
// on, namespaced by that algorithm's own EngineAdapter::id(), plus one
// "algorithm" choice parameter that picks which set is actually wired to
// audio. Only one engine is ever constructed/prepared at a time, reusing
// one working buffer sized for the largest registered algorithm - see
// EngineRegistry::maxRequiredWorkingBufferSize().
//
// Switching (constructing the new adapter, calling its prepare()) happens
// at the top of processBlock() when the choice parameter's value has
// changed since the last block - simple and correct, at the cost of a
// brief audio dropout on the block where the switch happens. That's an
// acceptable tradeoff for a deliberate, infrequent user action; a glitch-
// free swap would need a lock-free handoff between the message thread and
// the audio thread, not worth the complexity for this first slice.
class LoomBrowserAudioProcessor : public juce::AudioProcessor
{
  public:
    LoomBrowserAudioProcessor();

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

    static constexpr const char* kAlgorithmParamId = "algorithm";

    // The engine driving audio right now - null before the first
    // prepareToPlay(). The editor reads this to know which schema/prefix
    // to render; safe to call from the message thread since it only
    // changes inside processBlock (the audio thread), and the editor
    // only reads the pointer, never the engine's internals directly.
    const loom::browser::EngineAdapter* activeAdapter() const { return activeAdapter_.get(); }

    // Index into EngineRegistry the editor last saw active - lets a
    // juce::Timer-polling editor detect a switch (from automation, a
    // host preset, or the picker itself) and rebuild its child views.
    int activeAdapterIndex() const { return activeIndex_.load(); }

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void switchTo(int index);

    std::unique_ptr<loom::browser::EngineAdapter> activeAdapter_;
    std::atomic<int> activeIndex_ { -1 };
    std::vector<float> workingBuffer_;
    double sampleRate_ = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoomBrowserAudioProcessor)
};
