#pragma once

#include "SynthWidgets.h"
#include "../../Parameters.h"

/**
    Mixer section: independent volume + on/off for the three oscillators, the
    noise generator (white/pink) and the feedback / external-input path.
*/
class MixerPanel : public moogui::SectionPanel
{
public:
    explicit MixerPanel (moogui::APVTS& state)
        : SectionPanel ("Mixer"),
          osc1Vol (state, ParamID::mixOsc1Vol,  "Osc 1"),
          osc2Vol (state, ParamID::mixOsc2Vol,  "Osc 2"),
          osc3Vol (state, ParamID::mixOsc3Vol,  "Osc 3"),
          noiseVol (state, ParamID::mixNoiseVol, "Noise"),
          fbVol   (state, ParamID::mixFeedback, "Feedbk"),
          osc1On (state, ParamID::mixOsc1On,     "1"),
          osc2On (state, ParamID::mixOsc2On,     "2"),
          osc3On (state, ParamID::mixOsc3On,     "3"),
          noiseOn (state, ParamID::mixNoiseOn,   "N"),
          fbOn   (state, ParamID::mixFeedbackOn, "F"),
          noiseType (state, ParamID::noiseType, "Type", ParamChoices::noiseTypes())
    {
        for (auto* c : std::initializer_list<juce::Component*> {
                 &osc1Vol, &osc2Vol, &osc3Vol, &noiseVol, &fbVol,
                 &osc1On, &osc2On, &osc3On, &noiseOn, &fbOn, &noiseType })
            addAndMakeVisible (c);
    }

    void resized() override
    {
        auto b = contentArea();
        auto switchRow = b.removeFromBottom (24);
        auto typeRow   = b.removeFromBottom (30);

        moogui::LabeledKnob* knobs[]   = { &osc1Vol, &osc2Vol, &osc3Vol, &noiseVol, &fbVol };
        moogui::RockerSwitch* switches[] = { &osc1On, &osc2On, &osc3On, &noiseOn, &fbOn };

        const int n = 5;
        const int w = b.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            knobs[i]->setBounds (b.removeFromLeft (w).reduced (2, 0));
            switches[i]->setBounds (switchRow.removeFromLeft (w).reduced (10, 2));
        }

        noiseType.setBounds (typeRow.removeFromRight (110).reduced (4, 2));
    }

private:
    moogui::LabeledKnob  osc1Vol, osc2Vol, osc3Vol, noiseVol, fbVol;
    moogui::RockerSwitch osc1On, osc2On, osc3On, noiseOn, fbOn;
    moogui::LabeledChoice noiseType;
};
