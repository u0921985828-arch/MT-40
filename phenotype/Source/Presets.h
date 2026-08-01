#pragma once

//==============================================================================
//  Presets.h
//
//  Factory presets as (name -> normalised value per parameter id). Values are
//  keyed by id so the table stays correct if the parameter order ever changes.
//==============================================================================

#include <array>
#include <utility>

namespace phenotype::presets
{
    struct KV { const char* id; float value; };

    struct Preset
    {
        const char* name;
        std::array<KV, 6> overrides;   // only the params that differ from default
    };

    //  Compact, musically-distinct starting points.
    inline constexpr std::array<Preset, 5> kFactory { {
        { "Init",          { { {"outputGain",0.8f}, {"grainDensity",0.4f}, {"grainSize",0.3f}, {"arpOn",0.0f}, {"scaleType",0.0f}, {"crossBlend",0.5f} } } },
        { "Chlorophyll Pad", { { {"grainDensity",0.7f}, {"grainSize",0.7f}, {"caudal",0.3f}, {"soilDensity",0.7f}, {"modDepth",0.7f}, {"crossBlend",0.35f} } } },
        { "LED Pluck",     { { {"grainDensity",0.85f}, {"grainSize",0.12f}, {"caudal",0.8f}, {"soilDensity",0.25f}, {"spray",0.1f}, {"crossBlend",0.65f} } } },
        { "Capillary Arp", { { {"arpOn",1.0f}, {"arpRate",0.6f}, {"arpMode",0.35f}, {"scaleType",0.25f}, {"grainSize",0.2f}, {"grainDensity",0.8f} } } },
        { "Fluid Drift",   { { {"grainDensity",0.5f}, {"grainSize",0.9f}, {"spray",0.6f}, {"modDepth",0.9f}, {"caudal",0.2f}, {"soilDensity",0.85f} } } },
    } };
}
