#pragma once

#include "ArchitectureView.h"
#include "dsp/schema/AlgorithmSchema.h"

#include <juce_audio_processors/juce_audio_processors.h>

// The standard editor for every Loom plugin: the full generic parameter
// list (unchanged from before), plus a "Show Architecture" button that
// swaps it out for a read-only signal-flow diagram of whichever reverb
// core the plugin wraps (see ArchitectureView / dsp/schema/). One editor
// class shared by all five plugins, parameterized only by which
// AlgorithmSchema to show - adding a sixth algorithm needs no new editor
// code, just a schema.
class LoomPluginEditor : public juce::AudioProcessorEditor
{
  public:
    LoomPluginEditor(juce::AudioProcessor& processor, const dsp::schema::AlgorithmSchema& schema);

    void resized() override;

  private:
    void toggleView();

    juce::GenericAudioProcessorEditor parametersEditor_;
    juce::Viewport parametersViewport_;

    ArchitectureView architectureView_;
    juce::Viewport architectureViewport_;

    juce::TextButton toggleButton_;
    bool showingArchitecture_ = false;
};
