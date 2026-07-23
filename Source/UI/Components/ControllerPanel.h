#pragma once

#include "SynthWidgets.h"
#include "../../Parameters.h"

/** Controllers section: master tuning, glide and pitch-bend range. */
class ControllerPanel : public moogui::SectionPanel
{
public:
    explicit ControllerPanel (moogui::APVTS& state)
        : SectionPanel ("Controllers"),
          tune   (state, ParamID::masterTune,     "Tune"),
          glide  (state, ParamID::glideTime,      "Glide"),
          bend   (state, ParamID::pitchBendRange, "Bend Rng"),
          glideOn (state, ParamID::glideOn, "GLIDE")
    {
        addAndMakeVisible (tune);
        addAndMakeVisible (glide);
        addAndMakeVisible (bend);
        addAndMakeVisible (glideOn);
    }

    void resized() override
    {
        auto b = contentArea();
        auto switchRow = b.removeFromBottom (26);
        glideOn.setBounds (switchRow.reduced (20, 2));

        const int w = b.getWidth() / 3;
        tune.setBounds  (b.removeFromLeft (w));
        glide.setBounds (b.removeFromLeft (w));
        bend.setBounds  (b);
    }

private:
    moogui::LabeledKnob   tune, glide, bend;
    moogui::RockerSwitch  glideOn;
};
