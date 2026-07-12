#pragma once

#include "ArchitectureView.h"
#include "LoomParametersPanel.h"
#include "dsp/schema/AlgorithmSchema.h"

#include <juce_audio_processors/juce_audio_processors.h>

// The standard editor for every Loom plugin: the schema's signal-flow
// diagram (ArchitectureView) on top and the stage-grouped knob panel
// (LoomParametersPanel) below, side by side rather than toggled, with
// bidirectional cross-highlighting - hovering a knob glows its stage box
// in the diagram (or, when the diagram is showing the root while the
// knob belongs to a drilled-into stage, that stage's top-level ancestor),
// hovering a stage box glows its parameter sections, and clicking a
// stage box without a drill-down scrolls the panel to its section
// (drill-down boxes navigate into their sub-diagram, as before). One
// editor class shared by every plugin, parameterized only by which
// AlgorithmSchema to show - adding a new algorithm needs no new editor
// code, just a schema.
class LoomPluginEditor : public juce::AudioProcessorEditor
{
  public:
    LoomPluginEditor(juce::AudioProcessor& processor, const dsp::schema::AlgorithmSchema& schema);

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:
    void updateArchitectureViewSize();
    void updateParametersPanelSize();

    ArchitectureView architectureView_;
    juce::Viewport architectureViewport_;

    LoomParametersPanel parametersPanel_;
    juce::Viewport parametersViewport_;
};
