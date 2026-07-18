#include "LoomBrowserPluginEditor.h"

#include <cmath>

namespace
{
constexpr int kEditorWidth = 560;
constexpr int kEditorHeight = 860;
constexpr int kPickerHeight = 32;
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
    resized();
    repaint();
}

void LoomBrowserPluginEditor::resized()
{
    auto bounds = getLocalBounds();
    algorithmPicker_.setBounds(bounds.removeFromTop(kPickerHeight).reduced(10, 4));

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
