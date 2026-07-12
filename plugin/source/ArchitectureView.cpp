#include "ArchitectureView.h"

#include "LoomTheme.h"

#include <algorithm>
#include <cstring>

namespace
{
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
        juce::juce_wchar c = source[i];
        juce::juce_wchar previous = i > 0 ? source[i - 1] : juce::juce_wchar(' ');
        if (i > 0 && ((juce::CharacterFunctions::isUpperCase(c) &&
                       !juce::CharacterFunctions::isUpperCase(previous)) ||
                      (juce::CharacterFunctions::isDigit(c) &&
                       !juce::CharacterFunctions::isDigit(previous))))
        {
            out << ' ';
        }
        out << (i == 0 ? juce::CharacterFunctions::toUpperCase(c) : c);
    }
    return out;
}

// The callout inside a node body: first few parameter names, then "+N".
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

void ArchitectureView::setAccentColour(juce::Colour accent)
{
    accent_ = accent;
    repaint();
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
        numSkipEdges_ = 0;
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

    numColumns_ = 1 + *std::max_element(column.begin(), column.end());

    numSkipEdges_ = 0;
    for (const auto& connection : currentSchema_->connections)
    {
        auto from = indexOf(connection.fromId);
        auto to = indexOf(connection.toId);
        if (from < 0 || to < 0)
        {
            continue;
        }
        if (from > to)
        {
            ++numBackEdges_;
        }
        else if (from < to &&
                 column[static_cast<std::size_t>(to)] - column[static_cast<std::size_t>(from)] > 1)
        {
            ++numSkipEdges_; // routed over the top of the diagram
        }
    }

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

    // Long skip connections route above the boxes; leave a lane strip
    // for them between the header and the top row.
    auto skipSpace = static_cast<float>(numSkipEdges_) * kSkipEdgeSpacing + (numSkipEdges_ > 0 ? 6.0f : 0.0f);
    centerY_ = kHeaderHeight + skipSpace + (static_cast<float>(maxLanesInColumn_) * kLaneHeight) * 0.5f;

    for (int i = 0; i < n; ++i)
    {
        auto col = column[static_cast<std::size_t>(i)];
        auto lanesHere = laneCount[static_cast<std::size_t>(col)];
        auto laneOffset = static_cast<float>(lane[static_cast<std::size_t>(i)]) -
                          (static_cast<float>(lanesHere) - 1.0f) * 0.5f;
        auto x = kMargin + kTerminalLength + static_cast<float>(col) * (kBoxWidth + kColumnGap);
        auto y = centerY_ + laneOffset * kLaneHeight - kBoxHeight * 0.5f;
        boxes_.push_back({ &stages[static_cast<std::size_t>(i)], { x, y, kBoxWidth, kBoxHeight },
                           col, lane[static_cast<std::size_t>(i)], 0, 0 });
    }

    // Wire counts per node, for junction dots (splits), sum glyphs
    // (merges), sockets, and I/O terminal stubs.
    for (const auto& connection : currentSchema_->connections)
    {
        auto from = indexOf(connection.fromId);
        auto to = indexOf(connection.toId);
        if (from < 0 || to < 0 || from == to)
        {
            continue;
        }
        ++boxes_[static_cast<std::size_t>(from)].numOutgoing;
        ++boxes_[static_cast<std::size_t>(to)].numIncoming;
    }
}

int ArchitectureView::preferredWidth() const
{
    return static_cast<int>(2.0f * (kMargin + kTerminalLength) +
                            static_cast<float>(numColumns_) * kBoxWidth +
                            static_cast<float>(std::max(numColumns_ - 1, 0)) * kColumnGap);
}

int ArchitectureView::preferredHeightForWidth(int width) const
{
    juce::ignoreUnused(width);
    auto skipSpace = static_cast<float>(numSkipEdges_) * kSkipEdgeSpacing + (numSkipEdges_ > 0 ? 6.0f : 0.0f);
    return static_cast<int>(kHeaderHeight + skipSpace +
                            static_cast<float>(maxLanesInColumn_) * kLaneHeight + kSelfLoopClearance +
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
        repaint(); // footer display shows the hovered stage's detail
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
    g.fillAll(loom::colours::kDiagramBackground);

    juce::Rectangle<float> header(kMargin, 4.0f, static_cast<float>(getWidth()) - 2.0f * kMargin,
                                   kHeaderHeight - 8.0f);
    if (!history_.empty())
    {
        g.setColour(accent_);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("< Back", backButtonBounds(), juce::Justification::centredLeft);
        header.removeFromTop(22.0f);
    }
    auto titleRow = header.removeFromTop(24.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText(currentSchema_->name, titleRow, juce::Justification::centredLeft);

    // Wordmark, quiet and letter-spaced, in the family accent - anchored
    // within the typical visible width so it doesn't drift off-screen on
    // wide, horizontally-scrolled diagrams.
    auto wordmarkRow = titleRow.withWidth(juce::jmin(titleRow.getWidth(), 530.0f));
    g.setColour(accent_.withAlpha(0.55f));
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)).withExtraKerningFactor(0.35f));
    g.drawText("LOOM", wordmarkRow, juce::Justification::centredRight);

    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(juce::Colours::lightgrey);
    g.drawFittedText(currentSchema_->characterNote, header.toNearestInt(), juce::Justification::topLeft, 2);

    // Wires first so nodes and glyphs draw on top of them.
    int backEdgeIndex = 0;
    int skipEdgeIndex = 0;
    for (auto& connection : currentSchema_->connections)
    {
        drawConnection(g, connection, backEdgeIndex, skipEdgeIndex);
    }

    for (auto& box : boxes_)
    {
        // I/O terminal stubs - the manuals' "L In ->" / "-> Left" arrows.
        if (box.numIncoming == 0 && box.stage->kind == dsp::schema::StageKind::kInput)
        {
            auto y = box.bounds.getCentreY();
            g.setColour(loom::colours::kWire);
            g.drawLine(box.bounds.getX() - kTerminalLength, y, box.bounds.getX() - 2.0f, y, 1.4f);
            drawArrowHead(g, { box.bounds.getX(), y }, { 1.0f, 0.0f });
        }
        if (box.numOutgoing == 0 && box.stage->kind == dsp::schema::StageKind::kOutput)
        {
            auto y = box.bounds.getCentreY();
            g.setColour(loom::colours::kWire);
            g.drawLine(box.bounds.getRight(), y, box.bounds.getRight() + kTerminalLength - 9.0f, y, 1.4f);
            drawArrowHead(g, { box.bounds.getRight() + kTerminalLength, y }, { 1.0f, 0.0f });
        }

        // Junction dot where one output splits to several destinations.
        if (box.numOutgoing >= 2)
        {
            g.setColour(loom::colours::kWire.withAlpha(1.0f));
            auto y = box.bounds.getCentreY();
            g.fillEllipse(box.bounds.getRight() + 6.0f, y - 3.0f, 6.0f, 6.0f);
        }

        // Sum glyph where several wires merge into one input - the
        // manuals' own circled-plus junction.
        if (box.numIncoming >= 2)
        {
            auto cx = box.bounds.getX() - 13.0f;
            auto cy = box.bounds.getCentreY();
            g.setColour(loom::colours::kDiagramBackground);
            g.fillEllipse(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
            g.setColour(loom::colours::kWire.withAlpha(1.0f));
            g.drawEllipse(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f, 1.4f);
            g.drawLine(cx - 4.0f, cy, cx + 4.0f, cy, 1.4f);
            g.drawLine(cx, cy - 4.0f, cx, cy + 4.0f, 1.4f);
            drawArrowHead(g, { box.bounds.getX(), cy }, { 1.0f, 0.0f });
        }
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
    auto role = loom::colourForStageKind(box.stage->kind);

    // Node body: slate, with the role hue as a header strip - the
    // Blender-node anatomy - plus the restrained SGI bevel pair.
    auto body = box.bounds;
    g.setColour(highlighted ? loom::colours::kNodeBody.brighter(0.15f) : loom::colours::kNodeBody);
    g.fillRoundedRectangle(body, 4.0f);
    auto headerStrip = body.withHeight(kHeaderStripHeight);
    g.setColour(highlighted ? role.brighter(0.3f) : role);
    juce::Path headerPath;
    headerPath.addRoundedRectangle(headerStrip.getX(), headerStrip.getY(), headerStrip.getWidth(),
                                    headerStrip.getHeight(), 4.0f, 4.0f, true, true, false, false);
    g.fillPath(headerPath);

    g.setColour(loom::colours::kBevelLight);
    g.drawLine(body.getX() + 3.0f, body.getY() + 0.7f, body.getRight() - 3.0f, body.getY() + 0.7f, 1.0f);
    g.setColour(loom::colours::kBevelDark);
    g.drawLine(body.getX() + 3.0f, body.getBottom() - 0.7f, body.getRight() - 3.0f, body.getBottom() - 0.7f,
               1.0f);

    if (highlighted)
    {
        g.setColour(accent_);
        g.drawRoundedRectangle(body, 4.0f, 2.0f);
    }
    else
    {
        g.setColour(clickable ? accent_.withAlpha(0.65f) : juce::Colours::white.withAlpha(0.25f));
        g.drawRoundedRectangle(body, 4.0f, clickable ? 1.6f : 1.0f);
    }

    // Sockets where wires attach.
    g.setColour(loom::colours::kWire.withAlpha(0.95f));
    if (box.numIncoming > 0 || box.stage->kind == dsp::schema::StageKind::kInput)
    {
        g.fillEllipse(box.bounds.getX() - 2.5f, box.bounds.getCentreY() - 2.5f, 5.0f, 5.0f);
    }
    if (box.numOutgoing > 0 || box.stage->kind == dsp::schema::StageKind::kOutput)
    {
        g.fillEllipse(box.bounds.getRight() - 2.5f, box.bounds.getCentreY() - 2.5f, 5.0f, 5.0f);
    }

    // Header label.
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    auto label = clickable ? juce::String(box.stage->label) + "  \xe2\x8c\x95"
                            : juce::String(box.stage->label);
    g.drawFittedText(label, headerStrip.reduced(6.0f, 1.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);

    // Parameter callout inside the body.
    auto callout = calloutTextFor(*box.stage);
    if (callout.isNotEmpty())
    {
        auto bodyArea = body.withTrimmedTop(kHeaderStripHeight).reduced(6.0f, 3.0f);
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(juce::Colours::white.withAlpha(0.62f));
        g.drawFittedText(callout, bodyArea.toNearestInt(), juce::Justification::topLeft, 3);
    }
}

void ArchitectureView::drawConnection(juce::Graphics& g, const dsp::schema::Connection& connection,
                                      int& backEdgeIndex, int& skipEdgeIndex)
{
    auto* from = findBox(connection.fromId);
    auto* to = findBox(connection.toId);
    if (from == nullptr || to == nullptr)
    {
        return;
    }

    g.setColour(loom::colours::kWire);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::italic)));

    // Wires stop at the sum glyph when the target merges several inputs.
    auto merge = to->numIncoming >= 2;
    auto targetX = merge ? to->bounds.getX() - 20.0f : to->bounds.getX();
    auto annotate = [&](juce::Rectangle<float> area, juce::Justification justification) {
        if (connection.label != nullptr)
        {
            g.setColour(accent_.withAlpha(0.95f));
            g.drawFittedText(connection.label, area.toNearestInt(), justification, 1);
            g.setColour(loom::colours::kWire);
        }
    };

    if (from == to)
    {
        // Self-loop: a return arc under the node, manual-style.
        auto b = from->bounds;
        auto loopDrop = 14.0f;
        juce::Path loop;
        loop.startNewSubPath(b.getRight() - 18.0f, b.getBottom());
        loop.lineTo(b.getRight() - 18.0f, b.getBottom() + loopDrop);
        loop.lineTo(b.getX() + 18.0f, b.getBottom() + loopDrop);
        loop.lineTo(b.getX() + 18.0f, b.getBottom() + 1.0f);
        g.strokePath(loop, juce::PathStrokeType(1.4f));
        drawArrowHead(g, { b.getX() + 18.0f, b.getBottom() + 1.0f }, { 0.0f, -1.0f });
        annotate({ b.getX(), b.getBottom() + loopDrop + 1.0f, b.getWidth(), 12.0f },
                 juce::Justification::centred);
        return;
    }

    if (from->column > to->column)
    {
        // Feedback: route below the whole diagram, right-to-left, and
        // back up into the target's left edge - the manuals' own return
        // path placement. Stack multiple returns so they don't overlap.
        auto skipSpace = static_cast<float>(numSkipEdges_) * kSkipEdgeSpacing + (numSkipEdges_ > 0 ? 6.0f : 0.0f);
        auto returnY = kHeaderHeight + skipSpace + static_cast<float>(maxLanesInColumn_) * kLaneHeight +
                       kSelfLoopClearance + static_cast<float>(backEdgeIndex) * kBackEdgeSpacing;
        ++backEdgeIndex;

        auto start = juce::Point<float>(from->bounds.getRight(), from->bounds.getCentreY());
        auto entryX = to->bounds.getX() - kColumnGap * 0.4f;
        juce::Path path;
        path.startNewSubPath(start);
        path.lineTo(start.x + kColumnGap * 0.35f, start.y);
        path.lineTo(start.x + kColumnGap * 0.35f, returnY);
        path.lineTo(entryX, returnY);
        path.lineTo(entryX, to->bounds.getCentreY());
        path.lineTo(targetX, to->bounds.getCentreY());
        g.strokePath(path, juce::PathStrokeType(1.4f));
        if (!merge)
        {
            drawArrowHead(g, { targetX, to->bounds.getCentreY() }, { 1.0f, 0.0f });
        }

        auto midX = (start.x + entryX) * 0.5f;
        annotate({ midX - 110.0f, returnY - 12.0f, 220.0f, 11.0f }, juce::Justification::centred);
        return;
    }

    if (from->column < to->column - 1)
    {
        // Long skip connection (spans intermediate columns): route over
        // the top of the diagram so it doesn't run through - and
        // overprint - the nodes and wires of the columns it passes,
        // stacked per skip edge like the manuals' outer bus runs.
        auto skipY = kHeaderHeight + 4.0f + static_cast<float>(skipEdgeIndex) * kSkipEdgeSpacing;
        ++skipEdgeIndex;

        auto start = juce::Point<float>(from->bounds.getRight(), from->bounds.getCentreY());
        auto entryX = to->bounds.getX() - kColumnGap * 0.4f;
        juce::Path path;
        path.startNewSubPath(start);
        path.lineTo(start.x + kColumnGap * 0.3f, start.y);
        path.lineTo(start.x + kColumnGap * 0.3f, skipY);
        path.lineTo(entryX, skipY);
        path.lineTo(entryX, to->bounds.getCentreY());
        path.lineTo(targetX, to->bounds.getCentreY());
        g.strokePath(path, juce::PathStrokeType(1.4f));
        if (!merge)
        {
            drawArrowHead(g, { targetX, to->bounds.getCentreY() }, { 1.0f, 0.0f });
        }

        auto midX = (start.x + entryX) * 0.5f;
        annotate({ midX - 110.0f, skipY - 12.0f, 220.0f, 11.0f }, juce::Justification::centred);
        return;
    }

    // Forward connection: orthogonal H-V-H elbow (straight when the
    // lanes already align).
    auto start = juce::Point<float>(from->bounds.getRight(), from->bounds.getCentreY());
    auto end = juce::Point<float>(targetX, to->bounds.getCentreY());
    auto elbowX = to->bounds.getX() - kColumnGap * 0.45f;
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
    if (!merge)
    {
        drawArrowHead(g, end, { 1.0f, 0.0f });
    }

    if (std::abs(start.y - end.y) < 1.0f)
    {
        // Straight run: label just above the wire.
        annotate({ start.x + 2.0f, start.y - 14.0f, std::max(end.x - start.x - 4.0f, 72.0f), 11.0f },
                 juce::Justification::centredLeft);
    }
    else
    {
        // Elbowed run: label beside the vertical segment, at its
        // midpoint - unique per target row, so several branches leaving
        // one node (a patch-bay fan-out) don't overprint each other the
        // way shared start-point labels would.
        auto midY = (start.y + end.y) * 0.5f - 5.5f;
        auto available = elbowX - start.x - 6.0f;
        if (available >= 40.0f)
        {
            annotate({ start.x + 2.0f, midY, available, 11.0f }, juce::Justification::centredRight);
        }
        else
        {
            annotate({ elbowX + 4.0f, midY, 84.0f, 11.0f }, juce::Justification::centredLeft);
        }
    }
}

void ArchitectureView::drawFooter(juce::Graphics& g)
{
    // Styled after the devices' own displays: accent text in a
    // near-black inset well.
    auto y = static_cast<float>(getHeight()) - kFooterHeight;
    juce::Rectangle<float> well(kMargin, y + 3.0f, static_cast<float>(getWidth()) - 2.0f * kMargin,
                                 kFooterHeight - 8.0f);
    g.setColour(loom::colours::kDisplayWell);
    g.fillRoundedRectangle(well, 3.0f);
    g.setColour(loom::colours::kBevelDark);
    g.drawRoundedRectangle(well, 3.0f, 1.0f);

    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f,
                                           juce::Font::plain)));
    auto area = well.reduced(8.0f, 2.0f);
    if (hoveredStage_ != nullptr && hoveredStage_->detail != nullptr)
    {
        g.setColour(accent_);
        g.drawFittedText(juce::String(hoveredStage_->label) + ": " + hoveredStage_->detail,
                          area.toNearestInt(), juce::Justification::centredLeft, 2);
    }
    else
    {
        g.setColour(accent_.withAlpha(0.45f));
        g.drawFittedText("HOVER A STAGE FOR DETAILS - CLICK TO JUMP TO ITS KNOBS",
                          area.toNearestInt(), juce::Justification::centredLeft, 1);
    }
}
