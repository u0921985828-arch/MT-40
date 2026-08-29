#pragma once

//==============================================================================
//  Presets.h
//
//  Factory presets as (name -> normalised value per parameter id). Values are
//  keyed by id so the table stays correct if the parameter order ever changes;
//  unused override slots hold {nullptr,0} and are skipped by the loader.
//
//  Every preset is a complete musical starting point that exercises the tone
//  section (SVF filter, drive, unison, stereo width) added to the engine, so
//  switching programs is an audible journey rather than small tweaks.
//==============================================================================

#include <array>
#include <utility>

namespace phenotype::presets
{
    struct KV { const char* id = nullptr; float value = 0.0f; };

    struct Preset
    {
        const char* name;
        std::array<KV, 12> overrides;   // params that differ from default
    };

    inline constexpr std::array<Preset, 10> kFactory { {
        //  Clean, neutral, wide-open — the reference tone.
        { "Init", { {
            {"grainDensity",0.4f}, {"grainSize",0.3f}, {"crossBlend",0.5f},
            {"filterCutoff",1.0f}, {"drive",0.08f}, {"stereoWidth",0.5f},
        } } },

        //  Lush evolving pad: dense long grains, gentle capillary swell, unison
        //  spread and a slow filter that breathes with the modulator.
        { "Chlorophyll Pad", { {
            {"grainDensity",0.72f}, {"grainSize",0.75f}, {"caudal",0.28f},
            {"soilDensity",0.72f}, {"modDepth",0.7f}, {"crossBlend",0.4f},
            {"unison",0.6f}, {"unisonDetune",0.35f}, {"filterCutoff",0.62f},
            {"filterMod",0.45f}, {"drive",0.18f}, {"stereoWidth",0.8f},
        } } },

        //  Short, bright, percussive stab with a touch of drive and reso.
        { "LED Pluck", { {
            {"grainDensity",0.85f}, {"grainSize",0.12f}, {"caudal",0.8f},
            {"soilDensity",0.22f}, {"spray",0.08f}, {"crossBlend",0.62f},
            {"filterCutoff",0.78f}, {"filterReso",0.35f}, {"drive",0.3f},
            {"unison",0.2f}, {"stereoWidth",0.55f},
        } } },

        //  Tempo-locked arpeggio with the capillary sweeping the filter for
        //  classic "moving" plucks.
        { "Capillary Arp", { {
            {"arpOn",1.0f}, {"arpRate",0.5f}, {"arpMode",0.35f}, {"arpSync",1.0f},
            {"scaleType",0.25f}, {"grainSize",0.18f}, {"grainDensity",0.82f},
            {"filterCutoff",0.55f}, {"filterReso",0.45f}, {"filterMod",0.6f},
            {"drive",0.22f}, {"stereoWidth",0.7f},
        } } },

        //  Ambient generative drift: huge spray, slow drain, wide stereo cloud.
        { "Fluid Drift", { {
            {"grainDensity",0.5f}, {"grainSize",0.92f}, {"spray",0.6f},
            {"modDepth",0.9f}, {"caudal",0.18f}, {"soilDensity",0.85f},
            {"unison",0.5f}, {"unisonDetune",0.5f}, {"filterCutoff",0.5f},
            {"filterMod",0.5f}, {"stereoWidth",1.0f}, {"drive",0.12f},
        } } },

        //  Bright bell/keys: fast grains, subtle unison shimmer, open filter.
        { "Trichome Keys", { {
            {"grainDensity",0.8f}, {"grainSize",0.22f}, {"crossBlend",0.55f},
            {"caudal",0.6f}, {"soilDensity",0.4f}, {"unison",0.4f},
            {"unisonDetune",0.2f}, {"filterCutoff",0.85f}, {"filterReso",0.2f},
            {"drive",0.15f}, {"stereoWidth",0.65f},
        } } },

        //  Warm sub-ish body with a low-pass and drive for weight (play low).
        { "Deep Genome", { {
            {"pitchA",0.42f}, {"pitchB",0.42f}, {"grainDensity",0.6f},
            {"grainSize",0.5f}, {"crossBlend",0.45f}, {"filterCutoff",0.4f},
            {"filterReso",0.18f}, {"drive",0.45f}, {"unison",0.25f},
            {"stereoWidth",0.35f}, {"outputGain",0.85f},
        } } },

        //  Aggressive resonant lead: band-pass bite, unison detune, driven.
        { "Resin Lead", { {
            {"grainDensity",0.9f}, {"grainSize",0.16f}, {"crossBlend",0.7f},
            {"filterType",0.5f}, {"filterCutoff",0.6f}, {"filterReso",0.7f},
            {"filterMod",0.4f}, {"drive",0.5f}, {"unison",0.7f},
            {"unisonDetune",0.45f}, {"stereoWidth",0.75f},
        } } },

        //  Slow textural evolution, deep capillary routing to filter + blend.
        { "Photosynthesis", { {
            {"grainDensity",0.55f}, {"grainSize",0.85f}, {"spray",0.35f},
            {"caudal",0.15f}, {"soilDensity",0.9f}, {"modDepth",1.0f},
            {"crossBlend",0.5f}, {"filterCutoff",0.5f}, {"filterMod",0.8f},
            {"unison",0.55f}, {"unisonDetune",0.4f}, {"stereoWidth",0.95f},
        } } },

        //  Tight, driven, wide stab for rhythmic hits.
        { "Neon Stab", { {
            {"grainDensity",0.88f}, {"grainSize",0.1f}, {"caudal",0.85f},
            {"soilDensity",0.18f}, {"crossBlend",0.6f}, {"filterCutoff",0.72f},
            {"filterReso",0.4f}, {"drive",0.55f}, {"unison",0.35f},
            {"unisonDetune",0.3f}, {"stereoWidth",0.85f},
        } } },
    } };
}
