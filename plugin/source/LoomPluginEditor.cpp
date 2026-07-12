#include "LoomPluginEditor.h"

namespace
{
constexpr int kToggleBarHeight = 30;
constexpr int kEditorWidth = 560;
constexpr int kEditorHeight = 680;
}

LoomPluginEditor::LoomPluginEditor(juce::AudioProcessor& audioProcessor,
                                    const dsp::schema::AlgorithmSchema& schema)
  : AudioProcessorEditor(audioProcessor), parametersPanel_(audioProcessor, schema),
    architectureView_(schema)
{
    addAndMakeVisible(toggleButton_);
    toggleButton_.setButtonText("Show Architecture");
    toggleButton_.onClick = [this] { toggleView(); };

    addAndMakeVisible(parametersViewport_);
    parametersViewport_.setViewedComponent(&parametersPanel_, false);

    addChildComponent(architectureViewport_);
    architectureViewport_.setViewedComponent(&architectureView_, false);
    architectureView_.onContentSizeChanged = [this] { updateArchitectureViewSize(); };

    setResizable(true, true);
    setSize(kEditorWidth, kEditorHeight);
}

void LoomPluginEditor::resized()
{
    auto bounds = getLocalBounds();
    toggleButton_.setBounds(bounds.removeFromTop(kToggleBarHeight).reduced(4));

    parametersViewport_.setBounds(bounds);
    updateParametersPanelSize();

    architectureViewport_.setBounds(bounds);
    updateArchitectureViewSize();
}

void LoomPluginEditor::updateParametersPanelSize()
{
    auto panelWidth = parametersViewport_.getWidth() - parametersViewport_.getScrollBarThickness();
    parametersPanel_.setSize(panelWidth, parametersPanel_.preferredHeightForWidth(panelWidth));
}

void LoomPluginEditor::updateArchitectureViewSize()
{
    auto architectureWidth = architectureViewport_.getWidth() - architectureViewport_.getScrollBarThickness();
    architectureView_.setSize(architectureWidth, architectureView_.preferredHeightForWidth(architectureWidth));
}

void LoomPluginEditor::toggleView()
{
    showingArchitecture_ = !showingArchitecture_;
    parametersViewport_.setVisible(!showingArchitecture_);
    architectureViewport_.setVisible(showingArchitecture_);
    toggleButton_.setButtonText(showingArchitecture_ ? "Show Parameters" : "Show Architecture");
}
