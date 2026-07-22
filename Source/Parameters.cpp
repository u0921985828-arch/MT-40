#include "Parameters.h"
#include "dsp/Presets.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Human-readable choice lists so the WebView combo boxes populate their
    // option labels directly from the parameter (via properties.choices).
    StringArray rhythmChoices { "Sleng Teng", "Rock 2", "Pops", "Swing", "Bossa", "Waltz" };
    StringArray presetChoices;
    for (const auto& p : getMelodicPresets())
        presetChoices.add (p.name);

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::masterVolume, 1 }, "Main Volume",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::rhythmVolume, 1 }, "Rhythm Volume",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::bassVolume, 1 }, "Bass Volume",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::tempo, 1 }, "Tempo",
        NormalisableRange<float> (40.0f, 240.0f, 1.0f), 90.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamIDs::vibrato, 1 }, "Vibrato", false));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamIDs::sustain, 1 }, "Sustain", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::kbdMode, 1 }, "Mode",
        StringArray { "Off", "Play", "Chord" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::rhythmIdx, 1 }, "Rhythm", rhythmChoices, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::patchIdx, 1 }, "Preset", presetChoices, 0));

    return layout;
}
