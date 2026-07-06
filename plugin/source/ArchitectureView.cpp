#include "ArchitectureView.h"

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
            return juce::Colour(0xff7a4a2a);
        case dsp::schema::StageKind::kOutput:
            return juce::Colour(0xff5a3a68);
    }
    return juce::Colours::darkgrey;
}

bool sameId(const char* a, const char* b)
{
    return std::strcmp(a, b) == 0;
}
}

ArchitectureView::ArchitectureView(const dsp::schema::AlgorithmSchema& schema) : schema_(schema) {}

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

void ArchitectureView::layoutBoxes(int width)
{
    boxes_.clear();
    auto boxX = kMargin + kSideLaneWidth;
    auto boxWidth = static_cast<float>(width) - 2.0f * boxX;
    float y = kHeaderHeight;
    for (auto& stage : schema_.stages)
    {
        boxes_.push_back({ &stage, { boxX, y, boxWidth, kBoxHeight } });
        y += kBoxHeight + kBoxGap;
    }
}

int ArchitectureView::preferredHeightForWidth(int width) const
{
    juce::ignoreUnused(width);
    return static_cast<int>(kHeaderHeight +
                             static_cast<float>(schema_.stages.size()) * (kBoxHeight + kBoxGap) + kMargin);
}

void ArchitectureView::resized()
{
    layoutBoxes(getWidth());
}

void ArchitectureView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e22));

    juce::Rectangle<float> header(kMargin, 4.0f, static_cast<float>(getWidth()) - 2.0f * kMargin,
                                   kHeaderHeight - 8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText(schema_.name, header.removeFromTop(24.0f), juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(juce::Colours::lightgrey);
    g.drawFittedText(schema_.characterNote, header.toNearestInt(), juce::Justification::topLeft, 2);

    // Connections first so boxes + labels draw on top of the arrows.
    for (auto& connection : schema_.connections)
    {
        drawConnection(g, connection);
    }

    for (auto& box : boxes_)
    {
        g.setColour(colourForKind(box.stage->kind));
        g.fillRoundedRectangle(box.bounds, 8.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(box.bounds, 8.0f, 1.5f);

        auto textArea = box.bounds.reduced(10.0f, 6.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        g.drawText(box.stage->label, textArea.removeFromTop(20.0f), juce::Justification::centredLeft);
        if (box.stage->detail != nullptr)
        {
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.drawFittedText(box.stage->detail, textArea.toNearestInt(), juce::Justification::topLeft, 3);
        }
    }
}

void ArchitectureView::drawConnection(juce::Graphics& g, const dsp::schema::Connection& connection)
{
    auto* from = findBox(connection.fromId);
    auto* to = findBox(connection.toId);
    if (from == nullptr || to == nullptr)
    {
        return;
    }

    g.setColour(juce::Colours::white.withAlpha(0.6f));

    if (from == to)
    {
        // Self-loop: a small arc off the right edge of the box, kept
        // tight to the box so it doesn't reach as far into the right
        // lane as the "earlyReflections -> output" skip-curve below.
        auto b = from->bounds;
        juce::Path loop;
        auto loopWidth = 22.0f;
        loop.startNewSubPath(b.getRight(), b.getY() + b.getHeight() * 0.3f);
        loop.cubicTo(b.getRight() + loopWidth, b.getY(), b.getRight() + loopWidth, b.getBottom(),
                     b.getRight(), b.getY() + b.getHeight() * 0.7f);
        g.strokePath(loop, juce::PathStrokeType(1.8f));

        juce::Path arrowHead;
        auto tip = juce::Point<float>(b.getRight(), b.getY() + b.getHeight() * 0.7f);
        arrowHead.addTriangle(tip, tip.translated(9.0f, -6.0f), tip.translated(2.0f, -10.0f));
        g.fillPath(arrowHead);

        if (connection.label != nullptr)
        {
            juce::Rectangle<float> labelArea(b.getRight() + loopWidth + 4.0f, b.getY() - 4.0f,
                                              kSideLaneWidth - loopWidth - 8.0f, b.getHeight() + 8.0f);
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::italic)));
            g.setColour(juce::Colours::orange);
            g.drawFittedText(connection.label, labelArea.toNearestInt(), juce::Justification::centredLeft,
                              4);
        }
        return;
    }

    auto fromIndex = static_cast<int>(from - boxes_.data());
    auto toIndex = static_cast<int>(to - boxes_.data());
    juce::Path path;

    if (toIndex == fromIndex + 1)
    {
        // Adjacent in the stack: a straight vertical arrow.
        auto start = from->bounds.getBottomLeft().withX(from->bounds.getCentreX());
        auto end = to->bounds.getTopLeft().withX(to->bounds.getCentreX());
        path.startNewSubPath(start);
        path.lineTo(end);
        g.strokePath(path, juce::PathStrokeType(1.8f));

        juce::Path arrowHead;
        arrowHead.addTriangle(end, end.translated(-6.0f, -10.0f), end.translated(6.0f, -10.0f));
        g.fillPath(arrowHead);

        if (connection.label != nullptr)
        {
            juce::Rectangle<float> labelArea(end.x + 8.0f, (start.y + end.y) * 0.5f - 8.0f, 220.0f, 16.0f);
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::italic)));
            g.setColour(juce::Colours::orange);
            g.drawText(connection.label, labelArea, juce::Justification::centredLeft);
        }
        return;
    }

    // Non-adjacent (a branch that skips stages, e.g. the parallel early-
    // reflections path): curve out to whichever side the connection
    // starts from - "input" bulges left, anything else (e.g. the
    // rejoin into "output") bulges right - so the two skip-connections
    // our schemas actually have don't overlap.
    auto bulgeLeft = sameId(connection.fromId, "input");
    auto sideX = bulgeLeft ? kMargin + 12.0f : static_cast<float>(getWidth()) - kMargin - 12.0f;

    auto start = bulgeLeft ? from->bounds.getBottomLeft() : from->bounds.getBottomRight();
    auto end = bulgeLeft ? to->bounds.getTopLeft() : to->bounds.getTopRight();

    path.startNewSubPath(start);
    path.cubicTo({ sideX, start.y }, { sideX, end.y }, end);
    g.strokePath(path, juce::PathStrokeType(1.5f));

    juce::Path arrowHead;
    auto dir = bulgeLeft ? 1.0f : -1.0f;
    arrowHead.addTriangle(end, end.translated(-6.0f * dir, -10.0f), end.translated(6.0f * dir, -10.0f));
    g.fillPath(arrowHead);

    if (connection.label != nullptr)
    {
        // Keep the label within this side's lane so it doesn't run under
        // the boxes (left lane: [0, boxX]; right lane: [box.right, width]).
        auto labelWidth = kSideLaneWidth - 6.0f;
        auto labelX = bulgeLeft ? kMargin : static_cast<float>(getWidth()) - kMargin - labelWidth;
        juce::Rectangle<float> labelArea(labelX, (start.y + end.y) * 0.5f - 20.0f, labelWidth, 40.0f);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::italic)));
        g.setColour(juce::Colours::orange);
        g.drawFittedText(connection.label, labelArea.toNearestInt(),
                          bulgeLeft ? juce::Justification::centredLeft : juce::Justification::centredRight,
                          3);
    }
}
