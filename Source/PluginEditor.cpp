#include "PluginEditor.h"
#include "dsp/Presets.h"
#include "dsp/RhythmEngine.h"

CasioMT40AudioProcessorEditor::CasioMT40AudioProcessorEditor (CasioMT40AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    auto setupKnob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        addAndMakeVisible (s);
    };
    setupKnob (masterKnob);
    setupKnob (rhythmKnob);
    setupKnob (bassKnob);
    setupKnob (tempoKnob);

    addAndMakeVisible (vibratoButton);
    addAndMakeVisible (sustainButton);

    modeBox.addItemList ({ "Off", "Play", "Chord" }, 1);
    addAndMakeVisible (modeBox);

    const auto& presets = getMelodicPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    addAndMakeVisible (presetBox);

    static const char* rhythmNames[RhythmEngine::kNumRhythms] =
        { "Sleng Teng", "Rock 2", "Pops", "Swing", "Bossa", "Waltz" };
    for (int i = 0; i < RhythmEngine::kNumRhythms; ++i)
        rhythmBox.addItem (rhythmNames[i], i + 1);
    addAndMakeVisible (rhythmBox);

    addLabelled (masterKnob, "Main");
    addLabelled (rhythmKnob, "Rhythm");
    addLabelled (bassKnob,   "Bass");
    addLabelled (tempoKnob,  "Tempo");
    addLabelled (modeBox,    "Mode");
    addLabelled (rhythmBox,  "Rhythm Sel");
    addLabelled (presetBox,  "Preset");

    auto& apvts = processor.apvts;
    masterAtt  = std::make_unique<APVTS::SliderAttachment> (apvts, ParamIDs::masterVolume, masterKnob);
    rhythmAtt  = std::make_unique<APVTS::SliderAttachment> (apvts, ParamIDs::rhythmVolume, rhythmKnob);
    bassAtt    = std::make_unique<APVTS::SliderAttachment> (apvts, ParamIDs::bassVolume,   bassKnob);
    tempoAtt   = std::make_unique<APVTS::SliderAttachment> (apvts, ParamIDs::tempo,        tempoKnob);
    vibratoAtt = std::make_unique<APVTS::ButtonAttachment> (apvts, ParamIDs::vibrato,      vibratoButton);
    sustainAtt = std::make_unique<APVTS::ButtonAttachment> (apvts, ParamIDs::sustain,      sustainButton);
    modeAtt    = std::make_unique<APVTS::ComboBoxAttachment> (apvts, ParamIDs::kbdMode,    modeBox);
    rhythmBoxAtt = std::make_unique<APVTS::ComboBoxAttachment> (apvts, ParamIDs::rhythmIdx, rhythmBox);
    presetAtt  = std::make_unique<APVTS::ComboBoxAttachment> (apvts, ParamIDs::patchIdx,   presetBox);

    setSize (560, 280);
}

void CasioMT40AudioProcessorEditor::addLabelled (juce::Component& c, const juce::String& text)
{
    auto* l = new juce::Label ({}, text);
    l->setJustificationType (juce::Justification::centred);
    l->attachToComponent (&c, false);
    addAndMakeVisible (l);
    labels.add (l);
}

void CasioMT40AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2b30));
    g.setColour (juce::Colours::whitesmoke);
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText ("CASIO MT-40  (DSP Emulation)", getLocalBounds().removeFromTop (34),
                juce::Justification::centred);
}

void CasioMT40AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (40);

    auto knobRow = area.removeFromTop (110);
    const int kw = knobRow.getWidth() / 4;
    masterKnob.setBounds (knobRow.removeFromLeft (kw).reduced (8));
    rhythmKnob.setBounds (knobRow.removeFromLeft (kw).reduced (8));
    bassKnob.setBounds   (knobRow.removeFromLeft (kw).reduced (8));
    tempoKnob.setBounds  (knobRow.removeFromLeft (kw).reduced (8));

    area.removeFromTop (24);
    auto toggleRow = area.removeFromTop (30);
    vibratoButton.setBounds (toggleRow.removeFromLeft (120));
    sustainButton.setBounds (toggleRow.removeFromLeft (120));

    area.removeFromTop (24);
    auto comboRow = area.removeFromTop (30);
    const int cw = comboRow.getWidth() / 3;
    modeBox.setBounds   (comboRow.removeFromLeft (cw).reduced (6, 0));
    rhythmBox.setBounds (comboRow.removeFromLeft (cw).reduced (6, 0));
    presetBox.setBounds (comboRow.removeFromLeft (cw).reduced (6, 0));
}
