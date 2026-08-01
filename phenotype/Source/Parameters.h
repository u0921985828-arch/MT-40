#pragma once

//==============================================================================
//  Parameters.h
//
//  Host-facing parameter definitions. All 12 controls are normalised 0..1
//  AudioParameterFloats so the DAW can automate and persist them, and so the
//  WebView UI (which speaks 0..1) maps 1:1. The audio thread never touches the
//  APVTS tree directly — it reads cached raw std::atomic<float>* snapshots.
//==============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace phenotype::params
{
    struct Def
    {
        const char* id;
        const char* name;
        float       defaultValue;
    };

    //  Order is stable and mirrors dsp::ParameterSnapshot / the UI ids.
    inline constexpr std::array<Def, 12> kDefs { {
        { "caudal",       "Caudal",            0.5f },
        { "soilDensity",  "Densidad Suelo",    0.5f },
        { "saturation",   "Saturacion",        0.9f },
        { "grainDensity", "Densidad Grano",    0.4f },
        { "grainSize",    "Tamano Grano",      0.3f },
        { "position",     "Posicion",          0.5f },
        { "spray",        "Dispersion",        0.2f },
        { "pitchA",       "Cromosoma A",       0.5f },
        { "pitchB",       "Cromosoma B",       0.5f },
        { "crossBlend",   "Cross-Synthesis",   0.5f },
        { "modDepth",     "Profundidad Mod",   0.5f },
        { "outputGain",   "Salida",            0.8f },
    } };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (const auto& d : kDefs)
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { d.id, 1 },
                juce::String { d.name },
                juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0001f },
                d.defaultValue));
        return layout;
    }
}
