#pragma once

#include "EngineRegistry.h"
#include "LoomBrowserPluginProcessor.h"
#include "LoomParametersPanel.h"
#include "LoomTheme.h"
#include "ArchitectureView.h"
#include "pcm80/Pcm80Archive.h"
#include "pcm80/Pcm80TempoOverride.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
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
//
// Also owns the "Import PCM80 Preset..." button, enabled only while the
// selected algorithm's adapter has a PCM80 mapping (EngineAdapter::
// pcm80AlgorithmName() != nullptr - see PlateAdapter.h for the first
// one). Clicking it loads a decoded PCM80 archive JSON (produced by
// tools/pcm80-import/extract_presets.py from a ROM the user owns; never
// bundled with the plugin) and offers that algorithm's presets in a
// popup menu; picking one calls the active adapter's
// importPcm80Preset(), which writes engineering-unit values straight
// into the shared APVTS. Two toggles next to it modify that import (see
// applyPcm80Preset()): "Use DAW Tempo" recomputes tempo-synced Echo:Beat
// fields against the host's current tempo instead of the preset's own
// baked-in Tempo Rate (Pcm80TempoOverride.h) - the same override the
// PCM81 hardware itself offers via a master unit tempo setting. "Keep
// Current Mix" leaves the current Mix parameter alone rather than
// letting the import overwrite it, since many factory presets bake in
// Mix 100% wet.
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

    void showPcm80ImportMenu();
    void choosePcm80ArchiveFile();
    void applyPcm80Preset(const loom::browser::pcm80::Preset& preset);
    void updatePcm80ButtonEnablement();

    LoomBrowserAudioProcessor& processor_;
    loom::LookAndFeel lookAndFeel_;

    juce::ComboBox algorithmPicker_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> pickerAttachment_;

    juce::TextButton pcm80ImportButton_ { "Import PCM80 Preset..." };
    // "Use DAW Tempo": recompute any tempo-synced Echo:Beat field (delay
    // times) against the host's current tempo instead of the value baked
    // into the preset's own ROM data - the same override the PCM81
    // hardware itself offers via a master unit tempo setting. "Keep
    // Current Mix": many factory presets bake in Mix 100% wet; leave
    // whichever Mix value is already dialed in alone rather than having
    // every import stomp it - see Pcm80TempoOverride.h and
    // applyPcm80Preset()'s own comment for both.
    juce::ToggleButton useDawTempoToggle_ { "Use DAW Tempo" };
    juce::ToggleButton keepMixToggle_ { "Keep Current Mix" };
    loom::browser::pcm80::Archive pcm80Archive_;
    bool pcm80ArchiveLoaded_ = false;
    std::unique_ptr<juce::FileChooser> pcm80FileChooser_;
    std::map<int, const loom::browser::pcm80::Preset*> pcm80MenuPresets_;

    juce::Viewport architectureViewport_;
    juce::Viewport parametersViewport_;
    std::unique_ptr<ArchitectureView> architectureView_;
    std::unique_ptr<LoomParametersPanel> parametersPanel_;

    int renderedIndex_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoomBrowserPluginEditor)
};
