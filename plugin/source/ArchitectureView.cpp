#include "ArchitectureView.h"

#include <algorithm>
#include <cstring>

namespace
{
juce::Colour colourForKind(dsp::schema::StageKind kind)
{
    switch (kind)
    {
        case dsp::schema::StageKind::kInput:
            return juce::Colour(0xff3a5a78);
        case dsp::schema::StageKind::kProcessing:
            return juce::Colour(0xff3a5a4a);
        case dsp::schema::StageKind::kFeedback:
            // The manuals draw the central recirculating block (REVERB,
            // the tank, ...) as a shaded box - keep it visibly darker
            // and warmer than plain processing stages.
            return juce::Colour(0xff6a3f24);
        case dsp::schema::StageKind::kOutput:
            return juce::Colour(0xff5a3a68);
    }
    return juce::Colours::darkgrey;
}

bool sameId(const char* a, const char* b)
{
    return std::strcmp(a, b) == 0;
}

// "inLevelLeft" -> "In Level Left", "voice0Delay" -> "Voice 0 Delay" -
// readable manual-style callouts from APVTS ids without needing the
// processor's display names.
juce::String humanizeId(const char* id)
{
    juce::String source(id);
    juce::String out;
    for (int i = 0; i < source.length(); ++i)
    {
        auto c = source[i];
        auto previous = i > 0 ? source[i - 1] : juce::juce_wchar(' ');
        auto boundary = (juce::CharacterFunctions::isUpperCase(c) &&
                         !juce::CharacterFunctions::isUpperCase(previous)) ||
                        (juce::CharacterFunctions::isDigit(c) &&
                         !juce::CharacterFunctions::isDigit(previous));
        if (i > 0 && boundary)
        {
            out += ' ';
        }
        out += juce::String::charToString(i == 0 ? juce::CharacterFunctions::toUpperCase(c) : c);
    }
    return out;
}

// The callout line under a box: first few parameter names, then "+N".
juce::String calloutTextFor(const dsp::schema::Stage& stage)
{
    constexpr int kMaxNamed = 4;
    juce::StringArray names;
    auto count = static_cast<int>(stage.parameterIds.size());
    for (int i = 0; i < count && i < kMaxNamed; ++i)
    {
        names.add(humanizeId(stage.parameterIds[static_cast<std::size_t>(i)]));
    }
    auto text = names.joinIntoString(", ");
    if (count > kMaxNamed)
    {
        text << " +" << (count - kMaxNamed);
    }
    return text;
}

void drawArrowHead(juce::Graphics& g, juce::Point<float> tip, juce::Point<float> direction)
{
    // direction is a unit-ish vector along the final segment.
    auto back = tip - direction * 9.0f;
    auto normal = juce::Point<float>(-direction.y, direction.x) * 4.5f;
    juce::Path head;
    head.addTriangle(tip, back + normal, back - normal);
    g.fillPath(head);
}
}

ArchitectureView::ArchitectureView(const dsp::schema::AlgorithmSchema& rootSchema)
  : currentSchema_(&rootSchema)
{
    setInterceptsMouseClicks(true, true);
    computeLayout();
}

const ArchitectureView::BoxLayout* ArchitectureView::findBox(const char* id) const
{
    for (auto& box : boxes_)
    {
        if (sameId(box.stage->id, id))
        {
            return &box;
        }
    }
    return nullptr;
}

const ArchitectureView::BoxLayout* ArchitectureView::boxAt(juce::Point<float> position) const
{
    for (auto& box : boxes_)
    {
        if (box.bounds.contains(position))
        {
            return &box;
        }
    }
    return nullptr;
}

juce::Rectangle<float> ArchitectureView::backButtonBounds() const
{
    return { kMargin, 4.0f, 70.0f, 20.0f };
}

void ArchitectureView::computeLayout()
{
    boxes_.clear();
    const auto& stages = currentSchema_->stages;
    auto n = static_cast<int>(stages.size());
    if (n == 0)
    {
        numColumns_ = 0;
        maxLanesInColumn_ = 0;
        numBackEdges_ = 0;
        return;
    }

    auto indexOf = [&](const char* id) {
        for (int i = 0; i < n; ++i)
        {
            if (sameId(stages[static_cast<std::size_t>(i)].id, id))
            {
                return i;
            }
        }
        return -1;
    };

    // Columns: longest path over the connections that run *forward*
    // through the declaration order (schemas are authored in flow
    // order, so this is acyclic by construction). Everything else -
    // self-loops and backward connections - is drawn as feedback and
    // doesn't influence placement.
    std::vector<int> column(static_cast<std::size_t>(n), 0);
    numBackEdges_ = 0;
    for (int pass = 0; pass < n; ++pass)
    {
        for (const auto& connection : currentSchema_->connections)
        {
            auto from = indexOf(connection.fromId);
            auto to = indexOf(connection.toId);
            if (from < 0 || to < 0 || from >= to)
            {
                continue;
            }
            column[static_cast<std::size_t>(to)] =
              std::max(column[static_cast<std::size_t>(to)], column[static_cast<std::size_t>(from)] + 1);
        }
    }
    for (const auto& connection : currentSchema_->connections)
    {
        auto from = indexOf(connection.fromId);
        auto to = indexOf(connection.toId);
        if (from >= 0 && to >= 0 && from > to)
        {
            ++numBackEdges_;
        }
    }

    numColumns_ = 1 + *std::max_element(column.begin(), column.end());

    // Lanes: stages sharing a column stack symmetrically around the
    // center line, in declaration order - the manual's parallel-branch
    // look (voices above, reverb line center, more voices below).
    std::vector<int> laneCount(static_cast<std::size_t>(numColumns_), 0);
    std::vector<int> lane(static_cast<std::size_t>(n), 0);
    for (int i = 0; i < n; ++i)
    {
        lane[static_cast<std::size_t>(i)] = laneCount[static_cast<std::size_t>(column[static_cast<std::size_t>(i)])]++;
    }
    maxLanesInColumn_ = *std::max_element(laneCount.begin(), laneCount.end());

    centerY_ = kHeaderHeight + (static_cast<float>(maxLanesInColumn_) * kLaneHeight) * 0.5f;

    for (int i = 0; i < n; ++i)
    {
        auto col = column[static_cast<std::size_t>(i)];
        auto lanesHere = laneCount[static_cast<std::size_t>(col)];
        auto laneOffset = static_cast<float>(lane[static_cast<std::size_t>(i)]) -
                          (static_cast<float>(lanesHere) - 1.0f) * 0.5f;
        auto x = kMargin + static_cast<float>(col) * (kBoxWidth + kColumnGap);
        auto y = centerY_ + laneOffset * kLaneHeight - kBoxHeight * 0.5f;
        boxes_.push_back({ &stages[static_cast<std::size_t>(i)], { x, y, kBoxWidth, kBoxHeight },
                           col, lane[static_cast<std::size_t>(i)] });
    }
}

int ArchitectureView::preferredWidth() const
{
    return static_cast<int>(2.0f * kMargin + static_cast<float>(numColumns_) * kBoxWidth +
                            static_cast<float>(std::max(numColumns_ - 1, 0)) * kColumnGap);
}

int ArchitectureView::preferredHeightForWidth(int width) const
{
    juce::ignoreUnused(width);
    return static_cast<int>(kHeaderHeight + static_cast<float>(maxLanesInColumn_) * kLaneHeight +
                            static_cast<float>(numBackEdges_) * kBackEdgeSpacing + kFooterHeight +
                            kMargin);
}

void ArchitectureView::resized()
{
    computeLayout();
}

void ArchitectureView::mouseMove(const juce::MouseEvent& event)
{
    auto position = event.position;
    auto* box = boxAt(position);
    auto clickable = box != nullptr;
    if (!clickable && !history_.empty())
    {
        clickable = backButtonBounds().contains(position);
    }
    setMouseCursor(clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);

    auto* stage = box != nullptr ? box->stage : nullptr;
    if (stage != hoveredStage_)
    {
        hoveredStage_ = stage;
        repaint(); // footer shows the hovered stage's detail
        if (onStageHovered != nullptr)
        {
            onStageHovered(stage);
        }
    }
}

void ArchitectureView::mouseExit(const juce::MouseEvent&)
{
    if (hoveredStage_ != nullptr)
    {
        hoveredStage_ = nullptr;
        repaint();
        if (onStageHovered != nullptr)
        {
            onStageHovered(nullptr);
        }
    }
}

void ArchitectureView::setHighlightedStages(const juce::StringArray& ids)
{
    if (highlightedStageIds_ != ids)
    {
        highlightedStageIds_ = ids;
        repaint();
    }
}

void ArchitectureView::navigateTo(const dsp::schema::AlgorithmSchema* schema)
{
    currentSchema_ = schema;
    hoveredStage_ = nullptr;
    computeLayout();
    repaint();
    if (onContentSizeChanged != nullptr)
    {
        onContentSizeChanged();
    }
}

void ArchitectureView::mouseUp(const juce::MouseEvent& event)
{
    if (!history_.empty() && backButtonBounds().contains(event.position))
    {
        auto* previous = history_.back();
        history_.pop_back();
        navigateTo(previous);
        return;
    }

    auto* box = boxAt(event.position);
    if (box == nullptr)
    {
        return;
    }
    if (box->stage->drillDown != nullptr)
    {
        history_.push_back(currentSchema_);
        navigateTo(box->stage->drillDown);
        return;
    }
    if (onStageClicked != nullptr)
    {
        onStageClicked(*box->stage);
    }
}

void ArchitectureView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e22));

    juce::Rectangle<float> header(kMargin, 4.0f, static_cast<float>(getWidth()) - 2.0f * kMargin,
                                   kHeaderHeight - 8.0f);
    if (!history_.empty())
    {
        g.setColour(juce::Colours::lightblue);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("< Back", backButtonBounds(), juce::Justification::centredLeft);
        header.removeFromTop(22.0f);
    }
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText(currentSchema_->name, header.removeFromTop(24.0f), juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(juce::Colours::lightgrey);
    g.drawFittedText(currentSchema_->characterNote, header.toNearestInt(), juce::Justification::topLeft, 2);

    // Connections first so boxes + callouts draw on top of the wires.
    int backEdgeIndex = 0;
    for (auto& connection : currentSchema_->connections)
    {
        drawConnection(g, connection, backEdgeIndex);
    }

    for (auto& box : boxes_)
    {
        drawBox(g, box);
    }

    drawFooter(g);
}

void ArchitectureView::drawBox(juce::Graphics& g, const BoxLayout& box)
{
    auto clickable = box.stage->drillDown != nullptr;
    auto highlighted = highlightedStageIds_.contains(juce::String(box.stage->id));
    auto fill = colourForKind(box.stage->kind);

    g.setColour(highlighted ? fill.brighter(0.35f) : fill);
    g.fillRoundedRectangle(box.bounds, 4.0f);
    if (highlighted)
    {
        g.setColour(juce::Colours::orange.withAlpha(0.9f));
        g.drawRoundedRectangle(box.bounds, 4.0f, 2.5f);
    }
    else
    {
        g.setColour(clickable ? juce::Colours::lightblue.withAlpha(0.85f)
                               : juce::Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle(box.bounds, 4.0f, clickable ? 2.0f : 1.2f);
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    auto label = clickable ? juce::String(box.stage->label) + "  \xe2\x8c\x95"
                            : juce::String(box.stage->label);
    g.drawFittedText(label, box.bounds.reduced(6.0f, 3.0f).toNearestInt(),
                      juce::Justification::centred, 2);

    // Manual-style parameter callout under the box.
    auto callout = calloutTextFor(*box.stage);
    if (callout.isNotEmpty())
    {
        juce::Rectangle<float> calloutArea(box.bounds.getX() - kColumnGap * 0.25f,
                                            box.bounds.getBottom() + 2.0f,
                                            box.bounds.getWidth() + kColumnGap * 0.5f, kCalloutHeight);
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(juce::Colours::white.withAlpha(0.65f));
        g.drawFittedText(callout, calloutArea.toNearestInt(), juce::Justification::centredTop, 2);
    }
}

void ArchitectureView::drawConnection(juce::Graphics& g, const dsp::schema::Connection& connection,
                                      int& backEdgeIndex)
{
    auto* from = findBox(connection.fromId);
    auto* to = findBox(connection.toId);
    if (from == nullptr || to == nullptr)
    {
        return;
    }

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::italic)));

    if (from == to)
    {
        // Self-loop: a return arc under the box, manual-style.
        auto b = from->bounds;
        auto loopDrop = 14.0f;
        juce::Path loop;
        loop.startNewSubPath(b.getRight() - 18.0f, b.getBottom());
        loop.lineTo(b.getRight() - 18.0f, b.getBottom() + loopDrop);
        loop.lineTo(b.getX() + 18.0f, b.getBottom() + loopDrop);
        loop.lineTo(b.getX() + 18.0f, b.getBottom() + 1.0f);
        g.strokePath(loop, juce::PathStrokeType(1.4f));
        drawArrowHead(g, { b.getX() + 18.0f, b.getBottom() + 1.0f }, { 0.0f, -1.0f });
        if (connection.label != nullptr)
        {
            juce::Rectangle<float> labelArea(b.getX(), b.getBottom() + loopDrop + 1.0f, b.getWidth(),
                                              12.0f);
            g.setColour(juce::Colours::orange);
            g.drawFittedText(connection.label, labelArea.toNearestInt(), juce::Justification::centred, 1);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
        }
        return;
    }

    auto forward = from->column < to->column ||
                   (from->column == to->column && from->lane <= to->lane);
    if (from->column > to->column)
    {
        // Feedback: route below the whole diagram, right-to-left, and
        // back up into the target's left edge - the manuals' own return
        // path placement. Stack multiple returns so they don't overlap.
        auto returnY = kHeaderHeight + static_cast<float>(maxLanesInColumn_) * kLaneHeight +
                       static_cast<float>(backEdgeIndex) * kBackEdgeSpacing + 4.0f;
        ++backEdgeIndex;

        auto start = juce::Point<float>(from->bounds.getRight() + 6.0f, from->bounds.getCentreY());
        auto entry = juce::Point<float>(to->bounds.getX() - kColumnGap * 0.4f, to->bounds.getCentreY());
        juce::Path path;
        path.startNewSubPath(start);
        path.lineTo(start.x + kColumnGap * 0.35f, start.y);
        path.lineTo(start.x + kColumnGap * 0.35f, returnY);
        path.lineTo(entry.x, returnY);
        path.lineTo(entry.x, entry.y);
        path.lineTo(to->bounds.getX(), entry.y);
        g.strokePath(path, juce::PathStrokeType(1.4f));
        drawArrowHead(g, { to->bounds.getX(), entry.y }, { 1.0f, 0.0f });

        if (connection.label != nullptr)
        {
            auto midX = (start.x + entry.x) * 0.5f;
            juce::Rectangle<float> labelArea(midX - 110.0f, returnY - 12.0f, 220.0f, 11.0f);
            g.setColour(juce::Colours::orange);
            g.drawFittedText(connection.label, labelArea.toNearestInt(), juce::Justification::centred, 1);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
        }
        return;
    }
    juce::ignoreUnused(forward);

    // Forward connection: orthogonal H-V-H elbow (straight when the
    // lanes already align).
    auto start = juce::Point<float>(from->bounds.getRight(), from->bounds.getCentreY());
    auto end = juce::Point<float>(to->bounds.getX(), to->bounds.getCentreY());
    auto elbowX = end.x - kColumnGap * 0.45f;
    juce::Path path;
    path.startNewSubPath(start);
    if (std::abs(start.y - end.y) < 1.0f)
    {
        path.lineTo(end);
    }
    else
    {
        path.lineTo(elbowX, start.y);
        path.lineTo(elbowX, end.y);
        path.lineTo(end);
    }
    g.strokePath(path, juce::PathStrokeType(1.4f));
    drawArrowHead(g, end, { 1.0f, 0.0f });

    if (connection.label != nullptr)
    {
        // Sit the label just above the first horizontal segment.
        juce::Rectangle<float> labelArea(start.x - 4.0f, start.y - 14.0f,
                                          std::max(elbowX - start.x + 8.0f, 72.0f), 11.0f);
        g.setColour(juce::Colours::orange);
        g.drawFittedText(connection.label, labelArea.toNearestInt(), juce::Justification::centredLeft, 1);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
    }
}

void ArchitectureView::drawFooter(juce::Graphics& g)
{
    auto y = static_cast<float>(getHeight()) - kFooterHeight;
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillRect(0.0f, y, static_cast<float>(getWidth()), 1.0f);
    juce::Rectangle<float> area(kMargin, y + 3.0f, static_cast<float>(getWidth()) - 2.0f * kMargin,
                                 kFooterHeight - 6.0f);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    if (hoveredStage_ != nullptr && hoveredStage_->detail != nullptr)
    {
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawFittedText(juce::String(hoveredStage_->label) + ": " + hoveredStage_->detail,
                          area.toNearestInt(), juce::Justification::centredLeft, 2);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawFittedText("Hover a stage for details; click one to jump to its knobs.",
                          area.toNearestInt(), juce::Justification::centredLeft, 1);
    }
}
