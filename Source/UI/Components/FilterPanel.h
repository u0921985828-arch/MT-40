#pragma once

#include "SynthWidgets.h"
#include "../../Parameters.h"

/**
    Modifiers section: the Moog ladder filter and its dedicated contour (ADSR)
    envelope.
*/
class FilterPanel : public moogui::SectionPanel
{
public:
    explicit FilterPanel (moogui::APVTS& state)
        : SectionPanel ("Modifiers  —  Filter"),
          cutoff  (state, ParamID::filterCutoff, "Cutoff"),
          reso    (state, ParamID::filterReso,   "Reso"),
          contour (state, ParamID::filterEnv,    "Contour"),
          keyTrack (state, ParamID::filterKeyTrack, "Key Trk", ParamChoices::keyTracking()),
          attack  (state, ParamID::filterAttack,  "Attack"),
          decay   (state, ParamID::filterDecay,   "Decay"),
          sustain (state, ParamID::filterSustain, "Sustain"),
          release (state, ParamID::filterRelease, "Release")
    {
        for (auto* c : std::initializer_list<juce::Component*> {
                 &cutoff, &reso, &contour,
                 &attack, &decay, &sustain, &release, &keyTrack })
            addAndMakeVisible (c);
    }

    void resized() override
    {
        auto b = contentArea();

        // Top row: filter controls.
        auto top = b.removeFromTop (b.getHeight() / 2);
        keyTrack.setBounds (top.removeFromRight (90).reduced (2, 12));
        {
            const int w = top.getWidth() / 3;
            cutoff.setBounds  (top.removeFromLeft (w));
            reso.setBounds    (top.removeFromLeft (w));
            contour.setBounds (top);
        }

        // Bottom row: filter envelope ADSR.
        const int w = b.getWidth() / 4;
        attack.setBounds  (b.removeFromLeft (w));
        decay.setBounds   (b.removeFromLeft (w));
        sustain.setBounds (b.removeFromLeft (w));
        release.setBounds (b);
    }

private:
    moogui::LabeledKnob   cutoff, reso, contour;
    moogui::LabeledChoice keyTrack;
    moogui::LabeledKnob   attack, decay, sustain, release;
};
