#pragma once

#include "EngineRegistry.h"
#include "LoomBrowserPluginProcessor.h"
#include "LoomParametersPanel.h"
#include "LoomTheme.h"
#include "ArchitectureView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

// The Loom browser plugin's editor: an algorithm picker above the same
// split diagram/knobs view every single-algorithm Loom plugin uses (see
// LoomPluginEditor), rebuilt for whichever algorithm is currently
// selected. Unlike LoomPluginEditor, the schema isn't fixed for the
// editor's lifetime, so ArchitectureView/LoomParametersPanel are owned
// as rebuildable pointers rather than plain members.
//
// The picker updates the "algorithm" APVTS parameter directly (a normal
// ComboBoxParameterAttachment); a polling juce::Timer notices when that
// parameter's value differs from what's currently rendered - whether
// from the picker, host automation, or a loaded preset - and rebuilds.
// Rendering never depends on which engine the audio thread has actually
// switched to (see LoomBrowserPluginProcessor's switchTo() comment):
// schema()/id()/displayName() are pure metadata a throwaway adapter can
// answer immediately, so the UI stays correct even if the audio thread
// hasn't processed a block since the parameter changed.
class LoomBrowserPluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit LoomBrowserPluginEditor(LoomBrowserAudioProcessor& processor);
    ~LoomBrowserPluginEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:
    void timerCallback() override;
    void rebuildForAlgorithm(int index);
    void updateArchitectureViewSize();
    void updateParametersPanelSize();
    int selectedAlgorithmIndex() const;

    LoomBrowserAudioProcessor& processor_;
    loom::LookAndFeel lookAndFeel_;

    juce::ComboBox algorithmPicker_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> pickerAttachment_;

    juce::Viewport architectureViewport_;
    juce::Viewport parametersViewport_;
    std::unique_ptr<ArchitectureView> architectureView_;
    std::unique_ptr<LoomParametersPanel> parametersPanel_;

    int renderedIndex_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoomBrowserPluginEditor)
};
