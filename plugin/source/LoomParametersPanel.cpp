#include "LoomParametersPanel.h"

namespace
{
// Depth guard for schema drill-down recursion; hand-authored schemas are
// shallow (2 levels today), this just makes a future accidental cycle
// impossible to hang on.
constexpr int kMaxDrillDepth = 4;

juce::RangedAudioParameter* findParameter(juce::AudioProcessor& processor, const char* id)
{
    for (auto* parameter : processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
        {
            if (ranged->getParameterID() == juce::String(id))
            {
                return ranged;
            }
        }
    }
    return nullptr;
}
}

LoomParametersPanel::LoomParametersPanel(juce::AudioProcessor& processor,
                                         const dsp::schema::AlgorithmSchema& schema)
{
    buildSections(processor, schema);
}

LoomParametersPanel::~LoomParametersPanel() = default;

void LoomParametersPanel::buildSections(juce::AudioProcessor& processor,
                                        const dsp::schema::AlgorithmSchema& schema)
{
    // Everything not claimed by a stage ends up here, in processor order.
    std::vector<juce::RangedAudioParameter*> unclaimed;
    for (auto* parameter : processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
        {
            unclaimed.push_back(ranged);
        }
    }

    processor_ = &processor;
    collectStageSections(schema, unclaimed, 0);

    if (!unclaimed.empty())
    {
        Section section;
        section.title = sections_.empty() ? "Parameters" : "More Parameters";
        for (auto* parameter : unclaimed)
        {
            section.cells.push_back(makeCell(*parameter));
        }
        sections_.push_back(std::move(section));
    }

    for (auto& section : sections_)
    {
        for (auto& cell : section.cells)
        {
            addAndMakeVisible(*cell.nameLabel);
            if (cell.slider != nullptr)
            {
                addAndMakeVisible(*cell.slider);
            }
            if (cell.comboBox != nullptr)
            {
                addAndMakeVisible(*cell.comboBox);
            }
            if (cell.toggle != nullptr)
            {
                addAndMakeVisible(*cell.toggle);
            }
        }
    }
}

void LoomParametersPanel::collectStageSections(const dsp::schema::AlgorithmSchema& schema,
                                               std::vector<juce::RangedAudioParameter*>& unclaimed,
                                               int depth)
{
    if (depth >= kMaxDrillDepth)
    {
        return;
    }

    for (const auto& stage : schema.stages)
    {
        Section section;
        section.title = stage.label;
        for (const char* id : stage.parameterIds)
        {
            auto* parameter = processor_ != nullptr ? findParameter(*processor_, id) : nullptr;
            // A schema-listed ID that doesn't resolve is drift between the
            // hand-authored schema and the processor - fail loudly in
            // debug, skip quietly in release.
            jassertquiet(parameter != nullptr);
            if (parameter == nullptr)
            {
                continue;
            }
            std::erase(unclaimed, parameter);
            section.cells.push_back(makeCell(*parameter));
        }
        if (!section.cells.empty())
        {
            sections_.push_back(std::move(section));
        }
        if (stage.drillDown != nullptr)
        {
            collectStageSections(*stage.drillDown, unclaimed, depth + 1);
        }
    }
}

LoomParametersPanel::Cell LoomParametersPanel::makeCell(juce::RangedAudioParameter& parameter)
{
    Cell cell;
    cell.parameter = &parameter;

    cell.nameLabel = std::make_unique<juce::Label>(juce::String(), parameter.getName(64));
    cell.nameLabel->setJustificationType(juce::Justification::centred);
    cell.nameLabel->setFont(juce::FontOptions(12.0f));
    cell.nameLabel->setMinimumHorizontalScale(0.7f);
    cell.nameLabel->setInterceptsMouseClicks(false, false);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(&parameter))
    {
        cell.comboBox = std::make_unique<juce::ComboBox>();
        cell.comboBox->addItemList(choice->choices, 1);
        cell.comboAttachment =
          std::make_unique<juce::ComboBoxParameterAttachment>(parameter, *cell.comboBox);
    }
    else if (dynamic_cast<juce::AudioParameterBool*>(&parameter) != nullptr)
    {
        cell.toggle = std::make_unique<juce::ToggleButton>();
        cell.buttonAttachment = std::make_unique<juce::ButtonParameterAttachment>(parameter, *cell.toggle);
    }
    else
    {
        cell.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag,
                                                     juce::Slider::TextBoxBelow);
        cell.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, kCellWidth - 18, 16);
        cell.sliderAttachment = std::make_unique<juce::SliderParameterAttachment>(parameter, *cell.slider);
    }
    return cell;
}

int LoomParametersPanel::columnsForWidth(int width) const
{
    return juce::jmax(1, (width - 2 * kMargin) / kCellWidth);
}

int LoomParametersPanel::preferredHeightForWidth(int width) const
{
    auto columns = columnsForWidth(width);
    int height = kMargin;
    for (const auto& section : sections_)
    {
        auto rows = (static_cast<int>(section.cells.size()) + columns - 1) / columns;
        height += kSectionHeaderHeight + rows * kCellHeight + kSectionGap;
    }
    return height + kMargin;
}

void LoomParametersPanel::resized()
{
    auto columns = columnsForWidth(getWidth());
    int y = kMargin;
    for (auto& section : sections_)
    {
        section.headerBounds = { kMargin, y, getWidth() - 2 * kMargin, kSectionHeaderHeight };
        y += kSectionHeaderHeight;

        int index = 0;
        for (auto& cell : section.cells)
        {
            auto column = index % columns;
            auto row = index / columns;
            juce::Rectangle<int> cellBounds(kMargin + column * kCellWidth, y + row * kCellHeight,
                                            kCellWidth, kCellHeight);
            auto content = cellBounds.reduced(4);
            cell.nameLabel->setBounds(content.removeFromTop(16));
            if (cell.slider != nullptr)
            {
                cell.slider->setBounds(content);
            }
            else if (cell.comboBox != nullptr)
            {
                cell.comboBox->setBounds(content.withSizeKeepingCentre(content.getWidth(), 24));
            }
            else if (cell.toggle != nullptr)
            {
                cell.toggle->setBounds(content.withSizeKeepingCentre(24, 24));
            }
            ++index;
        }
        auto rows = (static_cast<int>(section.cells.size()) + columns - 1) / columns;
        y += rows * kCellHeight + kSectionGap;
    }
}

void LoomParametersPanel::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    for (const auto& section : sections_)
    {
        auto bounds = section.headerBounds.toFloat();
        g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText(section.title, bounds.reduced(2.0f, 0.0f), juce::Justification::bottomLeft);
        g.setColour(getLookAndFeel().findColour(juce::Label::textColourId).withAlpha(0.25f));
        g.fillRect(bounds.removeFromBottom(1.0f));
    }
}
