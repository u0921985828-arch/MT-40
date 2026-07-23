#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../LookAndFeelMoog.h"

/**
    Small reusable UI building blocks shared by every panel.
*/
namespace moogui
{
    using APVTS = juce::AudioProcessorValueTreeState;

    /** A rotary knob with a caption underneath, bound to an APVTS parameter. */
    class LabeledKnob : public juce::Component
    {
    public:
        LabeledKnob (APVTS& state, const juce::String& paramID, const juce::String& text)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
            addAndMakeVisible (slider);

            caption.setText (text, juce::dontSendNotification);
            caption.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (caption);

            attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            caption.setBounds (b.removeFromBottom (16));
            slider.setBounds (b);
        }

        juce::Slider slider;

    private:
        juce::Label caption;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    /** A rocker-style toggle switch bound to a bool APVTS parameter. */
    class RockerSwitch : public juce::Component
    {
    public:
        RockerSwitch (APVTS& state, const juce::String& paramID, const juce::String& text)
        {
            button.setButtonText (text);
            addAndMakeVisible (button);
            attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramID, button);
        }

        void resized() override { button.setBounds (getLocalBounds()); }

    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    /** A combo-box selector with a caption, bound to a choice APVTS parameter. */
    class LabeledChoice : public juce::Component
    {
    public:
        LabeledChoice (APVTS& state, const juce::String& paramID,
                       const juce::String& text, const juce::StringArray& items)
        {
            combo.addItemList (items, 1);
            addAndMakeVisible (combo);

            caption.setText (text, juce::dontSendNotification);
            caption.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (caption);

            attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramID, combo);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            caption.setBounds (b.removeFromBottom (16));
            combo.setBounds (b.reduced (2, 4));
        }

    private:
        juce::ComboBox combo;
        juce::Label caption;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    /** A titled section container that draws a bordered panel with a header. */
    class SectionPanel : public juce::Component
    {
    public:
        explicit SectionPanel (juce::String sectionTitle) : title (std::move (sectionTitle)) {}

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f);
            g.setColour (LookAndFeelMoog::panelSection);
            g.fillRoundedRectangle (b, 5.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (b, 5.0f, 1.2f);

            g.setColour (LookAndFeelMoog::labelText);
            g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (22),
                        juce::Justification::centred, false);
        }

        /** Sub-classes lay out inside the area below the header. */
        juce::Rectangle<int> contentArea() const
        {
            return getLocalBounds().withTrimmedTop (24).reduced (8);
        }

    private:
        juce::String title;
    };
}
