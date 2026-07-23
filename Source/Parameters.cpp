#include "Parameters.h"
#include "dsp/Presets.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Human-readable choice lists so the WebView combo boxes populate their
    // option labels directly from the parameter (via properties.choices).
    // The six rhythms in the order printed on the ST-40 rhythm slider
    // (left -> right). "Rock" (rightmost) is the classic digital-reggae rhythm.
    StringArray rhythmChoices { "Samba", "Waltz", "Swing", "Slow Rock", "Pops", "Rock" };
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

    // The ST-40 has no Off/Play/Chord switch: the bass keys are always
    // auto-chord/bass and the main keys always melody, so default to Chord.
    // (CC85 can still override to Off/Play per §6.)
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::kbdMode, 1 }, "Mode",
        StringArray { "Off", "Play", "Chord" }, 2));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::rhythmIdx, 1 }, "Rhythm", rhythmChoices, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::patchIdx, 1 }, "Preset", presetChoices, 0));

    return layout;
}
