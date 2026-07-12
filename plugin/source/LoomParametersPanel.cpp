#include "LoomParametersPanel.h"

#include "LoomTheme.h"

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
    // Receive child controls' mouse events too, so hovering a knob (not
    // just the gaps between knobs) reports its section for highlighting.
    addMouseListener(this, true);
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
    collectStageSections(schema, unclaimed, 0, nullptr);

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
                                               int depth, const char* rootStageId)
{
    if (depth >= kMaxDrillDepth)
    {
        return;
    }

    for (const auto& stage : schema.stages)
    {
        // At the root level each stage is its own ancestor; nested stages
        // keep the top-level stage they live under, so highlighting can
        // map them back to a root diagram box.
        const auto* thisRootId = depth == 0 ? stage.id : rootStageId;
        Section section;
        section.title = stage.label;
        section.stageId = stage.id;
        section.rootStageId = thisRootId != nullptr ? thisRootId : "";
        section.kind = stage.kind;
        section.hasKind = true;
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
            collectStageSections(*stage.drillDown, unclaimed, depth + 1, thisRootId);
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
        auto sectionBottom = y + rows * kCellHeight;
        section.bounds = { 0, section.headerBounds.getY(), getWidth(), sectionBottom - section.headerBounds.getY() };
        y = sectionBottom + kSectionGap;
    }
}

void LoomParametersPanel::setAccentColour(juce::Colour accent)
{
    accent_ = accent;
    // The controls cache their colours when constructed (before the
    // editor's LookAndFeel attaches), so push the display-well styling
    // - accent text in a near-black inset - onto each one explicitly.
    for (auto& section : sections_)
    {
        for (auto& cell : section.cells)
        {
            if (cell.slider != nullptr)
            {
                cell.slider->setColour(juce::Slider::textBoxTextColourId, accent_.brighter(0.15f));
                cell.slider->setColour(juce::Slider::textBoxBackgroundColourId,
                                       loom::colours::kDisplayWell);
                cell.slider->setColour(juce::Slider::textBoxOutlineColourId,
                                       juce::Colours::white.withAlpha(0.12f));
            }
            if (cell.comboBox != nullptr)
            {
                cell.comboBox->setColour(juce::ComboBox::backgroundColourId,
                                         loom::colours::kDisplayWell);
                cell.comboBox->setColour(juce::ComboBox::textColourId, accent_.brighter(0.15f));
                cell.comboBox->setColour(juce::ComboBox::outlineColourId,
                                         juce::Colours::white.withAlpha(0.12f));
                cell.comboBox->setColour(juce::ComboBox::arrowColourId, accent_);
            }
            if (cell.toggle != nullptr)
            {
                cell.toggle->setColour(juce::ToggleButton::tickColourId, accent_);
            }
        }
    }
    repaint();
}

void LoomParametersPanel::paint(juce::Graphics& g)
{
    g.fillAll(loom::colours::kPanelBackground);

    for (const auto& section : sections_)
    {
        auto highlighted =
          highlightedStageId_.isNotEmpty() && sectionMatches(section, highlightedStageId_);
        auto bounds = section.headerBounds.toFloat();
        if (highlighted)
        {
            g.setColour(accent_.withAlpha(0.09f));
            g.fillRect(section.bounds.toFloat());
        }

        // Small uppercase title (the Bitwig cue) over a hairline, with a
        // short role-colored underline segment tying the section to its
        // diagram node's header strip.
        g.setColour(highlighted ? accent_ : juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
        g.drawText(section.title.toUpperCase(), bounds.reduced(2.0f, 0.0f),
                    juce::Justification::bottomLeft);

        auto underline = bounds.removeFromBottom(highlighted ? 2.0f : 1.0f);
        g.setColour((highlighted ? accent_ : juce::Colours::white).withAlpha(highlighted ? 0.8f : 0.18f));
        g.fillRect(underline);
        if (section.hasKind)
        {
            g.setColour(loom::colourForStageKind(section.kind).brighter(highlighted ? 0.3f : 0.0f));
            g.fillRect(juce::Rectangle<float>(underline.getX(), underline.getY() - 2.0f, 46.0f, 3.0f));
        }
    }
}

bool LoomParametersPanel::sectionMatches(const Section& section, const juce::String& stageId)
{
    return section.stageId == stageId || section.rootStageId == stageId;
}

void LoomParametersPanel::setHighlightedStage(const juce::String& stageId)
{
    if (highlightedStageId_ != stageId)
    {
        highlightedStageId_ = stageId;
        repaint();
    }
}

int LoomParametersPanel::sectionTopForStage(const juce::String& stageId) const
{
    for (const auto& section : sections_)
    {
        if (sectionMatches(section, stageId))
        {
            return section.bounds.getY();
        }
    }
    return -1;
}

const LoomParametersPanel::Section* LoomParametersPanel::sectionAt(juce::Point<int> position) const
{
    for (const auto& section : sections_)
    {
        if (section.bounds.contains(position))
        {
            return &section;
        }
    }
    return nullptr;
}

void LoomParametersPanel::mouseMove(const juce::MouseEvent& event)
{
    // Events may arrive via the child-listener registration, relative to
    // whichever knob the mouse is over - normalize to panel coordinates.
    auto position = event.getEventRelativeTo(this).position.toInt();
    const auto* section = sectionAt(position);
    if (section != hoveredSection_)
    {
        hoveredSection_ = section;
        if (onSectionHovered != nullptr)
        {
            if (section != nullptr)
            {
                onSectionHovered(section->stageId, section->rootStageId);
            }
            else
            {
                onSectionHovered({}, {});
            }
        }
    }
}

void LoomParametersPanel::mouseExit(const juce::MouseEvent& event)
{
    // Child->parent transitions fire exits whose position is still inside
    // the panel; only a real departure clears the hover.
    auto position = event.getEventRelativeTo(this).position.toInt();
    if (!getLocalBounds().contains(position) && hoveredSection_ != nullptr)
    {
        hoveredSection_ = nullptr;
        if (onSectionHovered != nullptr)
        {
            onSectionHovered({}, {});
        }
    }
}
