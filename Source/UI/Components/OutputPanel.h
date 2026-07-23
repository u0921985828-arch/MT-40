#pragma once

#include "SynthWidgets.h"
#include "../../Parameters.h"

/**
    Output section: the loudness (amp) ADSR envelope and the master volume.
*/
class OutputPanel : public moogui::SectionPanel
{
public:
    explicit OutputPanel (moogui::APVTS& state)
        : SectionPanel ("Output  —  Loudness"),
          attack  (state, ParamID::ampAttack,  "Attack"),
          decay   (state, ParamID::ampDecay,   "Decay"),
          sustain (state, ParamID::ampSustain, "Sustain"),
          release (state, ParamID::ampRelease, "Release"),
          master  (state, ParamID::masterVolume, "Volume")
    {
        for (auto* c : std::initializer_list<juce::Component*> {
                 &attack, &decay, &sustain, &release, &master })
            addAndMakeVisible (c);
    }

    void resized() override
    {
        auto b = contentArea();
        master.setBounds (b.removeFromRight (76));

        const int w = b.getWidth() / 4;
        attack.setBounds  (b.removeFromLeft (w));
        decay.setBounds   (b.removeFromLeft (w));
        sustain.setBounds (b.removeFromLeft (w));
        release.setBounds (b);
    }

private:
    moogui::LabeledKnob attack, decay, sustain, release, master;
};
