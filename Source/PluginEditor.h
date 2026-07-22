#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// CasioMT40AudioProcessorEditor — minimal control surface exposing the §6
// parameters. Rotary controls for the gains/tempo, toggles for vibrato and
// sustain, and combo boxes for mode / rhythm / preset.
// ---------------------------------------------------------------------------
class CasioMT40AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit CasioMT40AudioProcessorEditor (CasioMT40AudioProcessor&);
    ~CasioMT40AudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    juce::Slider makeKnob();
    void addLabelled (juce::Component& c, const juce::String& text);

    CasioMT40AudioProcessor& processor;

    juce::Slider masterKnob, rhythmKnob, bassKnob, tempoKnob;
    juce::ToggleButton vibratoButton { "Vibrato" }, sustainButton { "Sustain" };
    juce::ComboBox modeBox, rhythmBox, presetBox;

    juce::OwnedArray<juce::Label> labels;

    std::unique_ptr<APVTS::SliderAttachment> masterAtt, rhythmAtt, bassAtt, tempoAtt;
    std::unique_ptr<APVTS::ButtonAttachment> vibratoAtt, sustainAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> modeAtt, rhythmBoxAtt, presetAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CasioMT40AudioProcessorEditor)
};
