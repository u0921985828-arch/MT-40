#pragma once

#include "SynthWidgets.h"
#include "../../Parameters.h"

/**
    Oscillators-bank section: three oscillator rows.  Osc 1 exposes waveform +
    range; Osc 2 and Osc 3 add coarse/fine detune; Osc 3 additionally offers an
    LFO mode with modulation depth.
*/
class OscillatorPanel : public moogui::SectionPanel
{
public:
    explicit OscillatorPanel (moogui::APVTS& state)
        : SectionPanel ("Oscillators Bank")
    {
        auto addRow = [this, &state] (const juce::String& label,
                                      const juce::String& waveID, const juce::String& rangeID)
        {
            auto* row = new Row();
            row->title.setText (label, juce::dontSendNotification);
            row->title.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (row->title);

            row->wave  = std::make_unique<moogui::LabeledChoice> (state, waveID,  "Wave",  ParamChoices::waveforms());
            row->range = std::make_unique<moogui::LabeledChoice> (state, rangeID, "Range", ParamChoices::ranges());
            addAndMakeVisible (*row->wave);
            addAndMakeVisible (*row->range);
            rows.add (row);
            return row;
        };

        addRow ("OSC 1", ParamID::osc1Wave, ParamID::osc1Range);

        auto* r2 = addRow ("OSC 2", ParamID::osc2Wave, ParamID::osc2Range);
        r2->coarse = std::make_unique<moogui::LabeledKnob> (state, ParamID::osc2Coarse, "Coarse");
        r2->fine   = std::make_unique<moogui::LabeledKnob> (state, ParamID::osc2Detune, "Fine");
        addAndMakeVisible (*r2->coarse);
        addAndMakeVisible (*r2->fine);

        auto* r3 = addRow ("OSC 3", ParamID::osc3Wave, ParamID::osc3Range);
        r3->coarse = std::make_unique<moogui::LabeledKnob> (state, ParamID::osc3Coarse, "Coarse");
        r3->fine   = std::make_unique<moogui::LabeledKnob> (state, ParamID::osc3Detune, "Fine");
        r3->mod    = std::make_unique<moogui::LabeledKnob> (state, ParamID::osc3ModDepth, "Mod");
        r3->lfo    = std::make_unique<moogui::RockerSwitch> (state, ParamID::osc3LfoMode, "LFO");
        addAndMakeVisible (*r3->coarse);
        addAndMakeVisible (*r3->fine);
        addAndMakeVisible (*r3->mod);
        addAndMakeVisible (*r3->lfo);
    }

    void resized() override
    {
        auto b = contentArea();
        const int rowH = b.getHeight() / rows.size();

        for (auto* row : rows)
        {
            auto r = b.removeFromTop (rowH).reduced (0, 2);
            row->title.setBounds (r.removeFromLeft (48));
            row->wave->setBounds  (r.removeFromLeft (100));
            row->range->setBounds (r.removeFromLeft (80));

            if (row->coarse) row->coarse->setBounds (r.removeFromLeft (56));
            if (row->fine)   row->fine->setBounds   (r.removeFromLeft (56));
            if (row->mod)    row->mod->setBounds     (r.removeFromLeft (56));
            if (row->lfo)    row->lfo->setBounds     (r.removeFromLeft (48).reduced (2, 18));
        }
    }

private:
    struct Row
    {
        juce::Label title;
        std::unique_ptr<moogui::LabeledChoice> wave, range;
        std::unique_ptr<moogui::LabeledKnob>   coarse, fine, mod;
        std::unique_ptr<moogui::RockerSwitch>  lfo;
    };

    juce::OwnedArray<Row> rows;
};
