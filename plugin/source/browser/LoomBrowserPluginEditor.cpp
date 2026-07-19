#include "LoomBrowserPluginEditor.h"

#include "AdapterHelpers.h"

#include <cmath>

namespace
{
constexpr int kEditorWidth = 560;
constexpr int kEditorHeight = 860;
constexpr int kPickerHeight = 32;
constexpr int kPcm80OptionsHeight = 24;
constexpr int kSeparatorHeight = 6;
// Diagram share of the remaining (post-picker) height; the rest is
// knobs. Matches LoomPluginEditor's own split.
constexpr float kDiagramShare = 0.42f;
}

LoomBrowserPluginEditor::LoomBrowserPluginEditor(LoomBrowserAudioProcessor& processor)
  : AudioProcessorEditor(processor), processor_(processor), lookAndFeel_(loom::colours::kLexiconPhosphor)
{
    setLookAndFeel(&lookAndFeel_);

    addAndMakeVisible(algorithmPicker_);
    for (int i = 0; i < loom::browser::engineRegistrySize(); ++i)
    {
        auto adapter = loom::browser::createAdapter(i);
        algorithmPicker_.addItem(adapter->displayName(), i + 1);
    }
    if (auto* algorithmParam = dynamic_cast<juce::AudioParameterChoice*>(
          processor_.apvts.getParameter(LoomBrowserAudioProcessor::kAlgorithmParamId)))
    {
        pickerAttachment_ =
          std::make_unique<juce::ComboBoxParameterAttachment>(*algorithmParam, algorithmPicker_);
    }

    addAndMakeVisible(pcm80ImportButton_);
    pcm80ImportButton_.onClick = [this] { showPcm80ImportMenu(); };
    addAndMakeVisible(useDawTempoToggle_);
    addAndMakeVisible(keepMixToggle_);

    addAndMakeVisible(architectureViewport_);
    addAndMakeVisible(parametersViewport_);

    rebuildForAlgorithm(selectedAlgorithmIndex());

    setResizable(true, true);
    setSize(kEditorWidth, kEditorHeight);
    startTimerHz(5);
}

LoomBrowserPluginEditor::~LoomBrowserPluginEditor()
{
    setLookAndFeel(nullptr);
}

int LoomBrowserPluginEditor::selectedAlgorithmIndex() const
{
    auto raw =
      processor_.apvts.getRawParameterValue(LoomBrowserAudioProcessor::kAlgorithmParamId)->load();
    return static_cast<int>(std::lround(raw));
}

void LoomBrowserPluginEditor::timerCallback()
{
    auto index = selectedAlgorithmIndex();
    if (index != renderedIndex_)
    {
        rebuildForAlgorithm(index);
    }
}

void LoomBrowserPluginEditor::rebuildForAlgorithm(int index)
{
    // A throwaway adapter purely for its metadata (schema/id/display
    // name) - deliberately independent of whichever engine the audio
    // thread has actually switched to (see the class doc comment).
    auto adapter = loom::browser::createAdapter(index);
    auto accent = loom::accentForPluginName(adapter->displayName());

    architectureView_ = std::make_unique<ArchitectureView>(adapter->schema());
    parametersPanel_ =
      std::make_unique<LoomParametersPanel>(processor_, adapter->schema(), adapter->id());

    architectureView_->setAccentColour(accent);
    parametersPanel_->setAccentColour(accent);

    architectureViewport_.setViewedComponent(architectureView_.get(), false);
    architectureView_->onContentSizeChanged = [this] { updateArchitectureViewSize(); };

    parametersViewport_.setViewedComponent(parametersPanel_.get(), false);

    parametersPanel_->onSectionHovered = [this](const juce::String& stageId,
                                                const juce::String& rootStageId) {
        juce::StringArray ids;
        if (stageId.isNotEmpty())
        {
            ids.add(stageId);
        }
        if (rootStageId.isNotEmpty() && rootStageId != stageId)
        {
            ids.add(rootStageId);
        }
        architectureView_->setHighlightedStages(ids);
    };
    architectureView_->onStageHovered = [this](const dsp::schema::Stage* stage) {
        parametersPanel_->setHighlightedStage(stage != nullptr ? juce::String(stage->id)
                                                                : juce::String());
    };
    architectureView_->onStageClicked = [this](const dsp::schema::Stage& stage) {
        auto target = parametersPanel_->sectionTopForStage(stage.id);
        if (target >= 0)
        {
            parametersViewport_.setViewPosition(0, target);
        }
    };

    renderedIndex_ = index;
    updatePcm80ButtonEnablement();
    resized();
    repaint();
}

void LoomBrowserPluginEditor::resized()
{
    auto bounds = getLocalBounds();
    auto pickerRow = bounds.removeFromTop(kPickerHeight).reduced(10, 4);
    pcm80ImportButton_.setBounds(pickerRow.removeFromRight(180));
    pickerRow.removeFromRight(6);
    algorithmPicker_.setBounds(pickerRow);

    auto pcm80OptionsRow = bounds.removeFromTop(kPcm80OptionsHeight).reduced(10, 2);
    keepMixToggle_.setBounds(pcm80OptionsRow.removeFromRight(150));
    pcm80OptionsRow.removeFromRight(6);
    useDawTempoToggle_.setBounds(pcm80OptionsRow.removeFromRight(150));

    auto diagramHeight = static_cast<int>(static_cast<float>(bounds.getHeight()) * kDiagramShare);
    architectureViewport_.setBounds(bounds.removeFromTop(diagramHeight));
    updateArchitectureViewSize();

    bounds.removeFromTop(kSeparatorHeight);
    parametersViewport_.setBounds(bounds);
    updateParametersPanelSize();
}

void LoomBrowserPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(loom::colours::kPanelBackground);

    auto drawSeam = [&](int y) {
        g.setColour(loom::colours::kBevelDark);
        g.fillRect(0, y, getWidth(), 1);
        g.setColour(loom::colours::kBevelLight);
        g.fillRect(0, y + 1, getWidth(), 1);
    };
    drawSeam(kPickerHeight);
    drawSeam(architectureViewport_.getBottom() + kSeparatorHeight / 2);
}

void LoomBrowserPluginEditor::updateParametersPanelSize()
{
    if (parametersPanel_ == nullptr)
    {
        return;
    }
    auto panelWidth = parametersViewport_.getWidth() - parametersViewport_.getScrollBarThickness();
    parametersPanel_->setSize(panelWidth, parametersPanel_->preferredHeightForWidth(panelWidth));
}

void LoomBrowserPluginEditor::updatePcm80ButtonEnablement()
{
    auto adapter = loom::browser::createAdapter(selectedAlgorithmIndex());
    pcm80ImportButton_.setEnabled(adapter->pcm80AlgorithmName() != nullptr);
}

void LoomBrowserPluginEditor::choosePcm80ArchiveFile()
{
    pcm80FileChooser_ = std::make_unique<juce::FileChooser>(
      "Select a decoded PCM80 preset archive (from tools/pcm80-import/extract_presets.py)",
      juce::File(), "*.json");

    pcm80FileChooser_->launchAsync(
      juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
      [this](const juce::FileChooser& chooser) {
          auto file = chooser.getResult();
          if (!file.existsAsFile())
          {
              return;
          }
          juce::String error;
          if (pcm80Archive_.loadFromFile(file, &error))
          {
              pcm80ArchiveLoaded_ = true;
              showPcm80ImportMenu();
          }
          else
          {
              juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                  .withIconType(juce::MessageBoxIconType::WarningIcon)
                  .withTitle("Couldn't load PCM80 archive")
                  .withMessage(error)
                  .withButton("OK"),
                nullptr);
          }
      });
}

void LoomBrowserPluginEditor::showPcm80ImportMenu()
{
    auto adapter = loom::browser::createAdapter(selectedAlgorithmIndex());
    auto* algorithmName = adapter->pcm80AlgorithmName();
    if (algorithmName == nullptr)
    {
        return;
    }

    juce::PopupMenu menu;
    int itemId = 1;
    menu.addItem(itemId++, pcm80ArchiveLoaded_ ? "Load Different Archive..." : "Load PCM80 Archive...");

    pcm80MenuPresets_.clear();
    if (pcm80ArchiveLoaded_)
    {
        auto matches = pcm80Archive_.presetsForAlgorithm(algorithmName);
        menu.addSeparator();
        if (matches.empty())
        {
            menu.addItem(itemId++, "(no " + juce::String(algorithmName) + " presets in this archive)", false);
        }
        else
        {
            for (auto* preset : matches)
            {
                auto label = preset->name;
                if (!preset->reliable)
                {
                    label += " (partial decode)";
                }
                menu.addItem(itemId, label);
                pcm80MenuPresets_[itemId] = preset;
                ++itemId;
            }
        }
    }

    menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result) {
        if (result == 0)
        {
            return;
        }
        if (result == 1)
        {
            choosePcm80ArchiveFile();
            return;
        }
        auto it = pcm80MenuPresets_.find(result);
        if (it != pcm80MenuPresets_.end())
        {
            applyPcm80Preset(*it->second);
        }
    });
}

void LoomBrowserPluginEditor::applyPcm80Preset(const loom::browser::pcm80::Preset& preset)
{
    using namespace loom::browser::pcm80;

    // "Use DAW Tempo": swap in the host's current tempo for every
    // tempo-synced Echo:Beat field before importing, rather than the
    // preset's own baked-in Tempo Rate - see Pcm80TempoOverride.h. Falls
    // back to the preset's own tempo (a silent no-op copy) if the host
    // isn't reporting a tempo right now (e.g. transport stopped on a
    // host that only sends tempo while playing).
    auto effectivePreset = preset;
    if (useDawTempoToggle_.getToggleState())
    {
        juce::Optional<double> hostBpm;
        if (auto* playHead = processor_.getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                hostBpm = position->getBpm();
            }
        }
        if (hostBpm.hasValue() && *hostBpm > 0.0)
        {
            effectivePreset = withTempoOverride(preset, *hostBpm);
        }
    }

    // Applying onto processor_.activeAdapter() (the engine actually
    // driving audio) rather than a throwaway metadata adapter, so this
    // stays consistent with LoomBrowserPluginProcessor::switchTo() -
    // both write the same APVTS, but the active instance is the one the
    // audio thread's process() calls will read from immediately after.
    auto* adapter = processor_.activeAdapter();
    if (adapter == nullptr)
    {
        return;
    }

    // "Keep Current Mix": many factory presets bake in Mix 100% wet,
    // which would stomp whatever dry/wet blend is already dialed in for
    // this session - snapshot it and restore it after import rather than
    // excluding it from every adapter's importPcm80Preset() individually
    // (every adapter uses the same "mix" parameter id suffix).
    auto mixParamId = loom::browser::prefixedId(adapter->id(), "mix");
    auto keepMix = keepMixToggle_.getToggleState();
    auto previousMix = keepMix ? loom::browser::paramValue(processor_.apvts, mixParamId) : 0.0f;

    adapter->importPcm80Preset(effectivePreset, processor_.apvts);

    if (keepMix)
    {
        loom::browser::setParamValue(processor_.apvts, mixParamId, previousMix);
    }
}

void LoomBrowserPluginEditor::updateArchitectureViewSize()
{
    if (architectureView_ == nullptr)
    {
        return;
    }
    auto viewportWidth = architectureViewport_.getWidth() - architectureViewport_.getScrollBarThickness();
    auto architectureWidth = juce::jmax(viewportWidth, architectureView_->preferredWidth());
    auto architectureHeight =
      juce::jmax(architectureViewport_.getHeight() - architectureViewport_.getScrollBarThickness(),
                 architectureView_->preferredHeightForWidth(architectureWidth));
    architectureView_->setSize(architectureWidth, architectureHeight);
}
