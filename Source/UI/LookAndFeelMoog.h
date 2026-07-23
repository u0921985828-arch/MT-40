#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Custom LookAndFeel emulating the Minimoog Model D aesthetic: black pointer
    knobs with a white indicator, walnut/dark-metal panels, and brightly
    coloured retro rocker switches (rendered as toggle buttons).
*/
class LookAndFeelMoog : public juce::LookAndFeel_V4
{
public:
    LookAndFeelMoog();

    // Colours used across the panels.
    static const juce::Colour chassis;      // dark polished metal
    static const juce::Colour woodwork;     // walnut side panels
    static const juce::Colour panelSection; // slightly lighter section fill
    static const juce::Colour labelText;    // soft yellow/white labels
    static const juce::Colour accentRed;
    static const juce::Colour accentBlue;
    static const juce::Colour accentYellow;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};
