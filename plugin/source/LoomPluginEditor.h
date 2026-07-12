#pragma once

#include "ArchitectureView.h"
#include "LoomParametersPanel.h"
#include "dsp/schema/AlgorithmSchema.h"

#include <juce_audio_processors/juce_audio_processors.h>

// The standard editor for every Loom plugin: a stage-grouped knob panel
// (LoomParametersPanel, sections in signal-flow order per the schema),
// plus a "Show Architecture" button that swaps it out for a read-only
// signal-flow diagram of the same schema (see ArchitectureView /
// dsp/schema/). One editor class shared by every plugin, parameterized
// only by which AlgorithmSchema to show - adding a new algorithm needs
// no new editor code, just a schema.
class LoomPluginEditor : public juce::AudioProcessorEditor
{
  public:
    LoomPluginEditor(juce::AudioProcessor& processor, const dsp::schema::AlgorithmSchema& schema);

    void resized() override;

  private:
    void toggleView();
    void updateArchitectureViewSize();
    void updateParametersPanelSize();

    LoomParametersPanel parametersPanel_;
    juce::Viewport parametersViewport_;

    ArchitectureView architectureView_;
    juce::Viewport architectureViewport_;

    juce::TextButton toggleButton_;
    bool showingArchitecture_ = false;
};
