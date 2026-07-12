#include "LoomTheme.h"

namespace loom
{
LookAndFeel::LookAndFeel(juce::Colour accent) : accent_(accent)
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::kPanelBackground);
    setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.87f));

    // Value wells under knobs read like small device displays: accent
    // text in a near-black inset.
    setColour(juce::Slider::textBoxTextColourId, accent_.brighter(0.15f));
    setColour(juce::Slider::textBoxBackgroundColourId, colours::kDisplayWell);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::white.withAlpha(0.12f));
    setColour(juce::Slider::rotarySliderFillColourId, accent_);
    setColour(juce::Slider::rotarySliderOutlineColourId, colours::kDisplayWell);

    setColour(juce::ComboBox::backgroundColourId, colours::kDisplayWell);
    setColour(juce::ComboBox::textColourId, accent_.brighter(0.15f));
    setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.12f));
    setColour(juce::ComboBox::arrowColourId, accent_);
    setColour(juce::PopupMenu::backgroundColourId, colours::kNodeBody);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent_.withAlpha(0.25f));

    setColour(juce::ToggleButton::tickColourId, accent_);
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::white.withAlpha(0.3f));

    setColour(juce::ScrollBar::thumbColourId, accent_.withAlpha(0.45f));
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPosProportional, float rotaryStartAngle,
                                   float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    auto centre = bounds.getCentre();
    auto lineWidth = juce::jlimit(2.5f, 5.0f, radius * 0.22f);
    auto arcRadius = radius - lineWidth * 0.5f;

    // Knob face: a slate disc with the 1px bevel pair, the SGI nod.
    g.setColour(colours::kNodeBody);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(colours::kBevelLight);
    g.drawEllipse(centre.x - radius, centre.y - radius - 0.5f, radius * 2.0f, radius * 2.0f, 1.0f);
    g.setColour(colours::kBevelDark);
    g.drawEllipse(centre.x - radius, centre.y - radius + 0.5f, radius * 2.0f, radius * 2.0f, 1.0f);

    // Track ring.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle,
                        rotaryEndAngle, true);
    g.setColour(colours::kDisplayWell);
    g.strokePath(track, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Family-accent value arc.
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    if (sliderPosProportional > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle,
                            true);
        g.setColour(accent_);
        g.strokePath(value, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // Pointer.
    auto pointerLength = radius * 0.55f;
    juce::Point<float> tip(centre.x + std::sin(angle) * arcRadius,
                           centre.y - std::cos(angle) * arcRadius);
    juce::Point<float> inner(centre.x + std::sin(angle) * (arcRadius - pointerLength),
                             centre.y - std::cos(angle) * (arcRadius - pointerLength));
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.drawLine({ inner, tip }, 2.0f);
}
}
