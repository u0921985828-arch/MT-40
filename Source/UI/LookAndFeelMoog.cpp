#include "LookAndFeelMoog.h"

const juce::Colour LookAndFeelMoog::chassis      { 0xff1a1a1c };
const juce::Colour LookAndFeelMoog::woodwork     { 0xff2c1a0e };
const juce::Colour LookAndFeelMoog::panelSection { 0xff242427 };
const juce::Colour LookAndFeelMoog::labelText    { 0xfff2e6c2 };
const juce::Colour LookAndFeelMoog::accentRed    { 0xffc0392b };
const juce::Colour LookAndFeelMoog::accentBlue   { 0xff2a6fb0 };
const juce::Colour LookAndFeelMoog::accentYellow { 0xffe0c040 };

LookAndFeelMoog::LookAndFeelMoog()
{
    setColour (juce::Slider::textBoxTextColourId, labelText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, labelText);
    setColour (juce::ComboBox::backgroundColourId, chassis.brighter (0.1f));
    setColour (juce::ComboBox::textColourId, labelText);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::black);
    setColour (juce::PopupMenu::backgroundColourId, chassis.brighter (0.05f));
    setColour (juce::PopupMenu::textColourId, labelText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentBlue);
}

void LookAndFeelMoog::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                            .reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Faint outer ring / scale.
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    const float knobR = radius * 0.78f;

    // Body with a subtle vertical gradient for a soft 3D feel.
    juce::ColourGradient grad (juce::Colour (0xff3a3a3d), centre.x, centre.y - knobR,
                               juce::Colour (0xff101012), centre.x, centre.y + knobR, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

    g.setColour (juce::Colours::black);
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    // White pointer indicator.
    juce::Path pointer;
    const float pointerW = juce::jmax (2.0f, knobR * 0.12f);
    pointer.addRoundedRectangle (-pointerW * 0.5f, -knobR * 0.95f, pointerW, knobR * 0.6f, pointerW * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (juce::Colours::white);
    g.fillPath (pointer);
}

void LookAndFeelMoog::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat();

    // Rocker-switch body: a rounded rectangle that "rocks" up (on) or down (off).
    const float corner = 4.0f;
    const bool on = button.getToggleState();

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds, corner);

    auto rocker = bounds.reduced (2.0f);
    const auto rockerColour = on ? accentBlue : juce::Colour (0xff3a3a3d);

    juce::ColourGradient grad (rockerColour.brighter (0.3f),
                               rocker.getX(), on ? rocker.getY() : rocker.getBottom(),
                               rockerColour.darker (0.4f),
                               rocker.getX(), on ? rocker.getBottom() : rocker.getY(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (rocker, corner);

    if (shouldDrawButtonAsHighlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRoundedRectangle (rocker, corner);
    }

    // Label to the right of / below the switch.
    g.setColour (labelText);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
}

void LookAndFeelMoog::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);

    g.setColour (findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // Simple down-arrow marker on the right.
    juce::Path arrow;
    const float aw = 8.0f, ah = 4.0f;
    const float cx = bounds.getRight() - 12.0f;
    const float cy = bounds.getCentreY();
    arrow.addTriangle (cx - aw * 0.5f, cy - ah * 0.5f,
                       cx + aw * 0.5f, cy - ah * 0.5f,
                       cx, cy + ah * 0.5f);
    g.setColour (labelText.withAlpha (box.isEnabled() ? 0.9f : 0.4f));
    g.fillPath (arrow);
}

juce::Font LookAndFeelMoog::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
}

juce::Font LookAndFeelMoog::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (12.0f, juce::Font::plain));
}
