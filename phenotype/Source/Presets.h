#pragma once

//==============================================================================
//  Presets.h
//
//  Factory content organised as 10 libraries, each holding 5 presets, exposed
//  to the host as a flat program list named "LIBRARY > Type • Preset" so they
//  group in the host's program menu. Keyed by parameter id (order-independent);
//  unused override slots hold {nullptr,0} and are skipped by the loader.
//
//  Every preset is a complete musical starting point exercising the full engine
//  (granular cloud + capillary modulator + SVF filter + drive + unison + width).
//==============================================================================

#include <array>

namespace phenotype::presets
{
    struct KV { const char* id = nullptr; float value = 0.0f; };

    struct Preset
    {
        const char* name;
        std::array<KV, 12> overrides;
    };

    //  10 libraries x 5 presets, flattened. Names carry their library prefix.
    inline constexpr std::array<Preset, 50> kFactory { {
        //== SATIVA PADS ======================================================
        { "SATIVA > Pad • Chlorophyll", { { {"grainDensity",0.72f},{"grainSize",0.78f},{"caudal",0.28f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.4f},{"unison",0.6f},{"unisonDetune",0.35f},{"filterCutoff",0.62f},{"filterMod",0.45f},{"drive",0.16f},{"stereoWidth",0.85f} } } },
        { "SATIVA > Pad • Morning Dew",  { { {"grainDensity",0.6f},{"grainSize",0.85f},{"caudal",0.22f},{"soilDensity",0.8f},{"modDepth",0.55f},{"crossBlend",0.5f},{"unison",0.5f},{"unisonDetune",0.3f},{"filterCutoff",0.7f},{"filterMod",0.35f},{"stereoWidth",0.9f} } } },
        { "SATIVA > Pad • Glass House",  { { {"grainDensity",0.65f},{"grainSize",0.7f},{"caudal",0.35f},{"soilDensity",0.65f},{"crossBlend",0.6f},{"unison",0.7f},{"unisonDetune",0.45f},{"filterCutoff",0.75f},{"filterReso",0.2f},{"stereoWidth",1.0f},{"drive",0.12f} } } },
        { "SATIVA > Pad • Photon Wash",  { { {"grainDensity",0.8f},{"grainSize",0.9f},{"caudal",0.18f},{"soilDensity",0.85f},{"modDepth",0.85f},{"crossBlend",0.35f},{"unison",0.55f},{"filterCutoff",0.55f},{"filterMod",0.6f},{"stereoWidth",0.95f} } } },
        { "SATIVA > Pad • Terpene Air",  { { {"grainDensity",0.5f},{"grainSize",0.8f},{"caudal",0.3f},{"soilDensity",0.6f},{"spray",0.3f},{"crossBlend",0.55f},{"unison",0.4f},{"unisonDetune",0.5f},{"filterCutoff",0.68f},{"stereoWidth",0.8f} } } },

        //== INDICA DRONES ====================================================
        { "INDICA > Drone • Deep Soil",   { { {"grainDensity",0.45f},{"grainSize",0.95f},{"caudal",0.12f},{"soilDensity",0.9f},{"modDepth",1.0f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterMod",0.7f},{"unison",0.5f},{"unisonDetune",0.4f},{"stereoWidth",1.0f},{"drive",0.2f} } } },
        { "INDICA > Drone • Tectonic",    { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.5f},{"grainSize",0.92f},{"soilDensity",0.88f},{"filterCutoff",0.35f},{"filterReso",0.2f},{"drive",0.35f},{"unison",0.4f},{"stereoWidth",0.7f} } } },
        { "INDICA > Drone • Resin Fog",   { { {"grainDensity",0.55f},{"grainSize",0.88f},{"spray",0.45f},{"caudal",0.15f},{"soilDensity",0.92f},{"modDepth",0.9f},{"filterCutoff",0.5f},{"filterMod",0.8f},{"unison",0.6f},{"stereoWidth",0.95f} } } },
        { "INDICA > Drone • Gravity Well",{ { {"pitchA",0.44f},{"pitchB",0.38f},{"grainDensity",0.4f},{"grainSize",0.9f},{"soilDensity",0.85f},{"filterType",0.0f},{"filterCutoff",0.4f},{"filterMod",0.5f},{"drive",0.4f},{"stereoWidth",0.6f} } } },
        { "INDICA > Drone • Midnight",    { { {"grainDensity",0.5f},{"grainSize",0.85f},{"caudal",0.1f},{"soilDensity",0.95f},{"modDepth",0.95f},{"crossBlend",0.4f},{"filterCutoff",0.45f},{"filterMod",0.65f},{"unison",0.55f},{"unisonDetune",0.45f},{"stereoWidth",0.9f} } } },

        //== RESIN KEYS =======================================================
        { "RESIN > Key • Bell Jar",     { { {"grainDensity",0.82f},{"grainSize",0.22f},{"caudal",0.6f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.4f},{"unisonDetune",0.2f},{"filterCutoff",0.85f},{"filterReso",0.2f},{"drive",0.15f},{"stereoWidth",0.65f} } } },
        { "RESIN > Key • Crystal",      { { {"grainDensity",0.85f},{"grainSize",0.18f},{"caudal",0.7f},{"soilDensity",0.35f},{"crossBlend",0.65f},{"filterCutoff",0.9f},{"filterReso",0.25f},{"unison",0.3f},{"stereoWidth",0.6f} } } },
        { "RESIN > Key • Mallet",       { { {"grainDensity",0.9f},{"grainSize",0.15f},{"caudal",0.8f},{"soilDensity",0.28f},{"crossBlend",0.5f},{"filterCutoff",0.78f},{"drive",0.25f},{"unison",0.2f},{"stereoWidth",0.55f} } } },
        { "RESIN > Key • Music Box",    { { {"grainDensity",0.8f},{"grainSize",0.2f},{"caudal",0.65f},{"soilDensity",0.45f},{"crossBlend",0.6f},{"filterCutoff",0.88f},{"filterReso",0.3f},{"unison",0.35f},{"unisonDetune",0.25f},{"stereoWidth",0.7f} } } },
        { "RESIN > Key • Frozen",       { { {"grainDensity",0.75f},{"grainSize",0.25f},{"caudal",0.55f},{"soilDensity",0.5f},{"crossBlend",0.45f},{"filterType",0.5f},{"filterCutoff",0.7f},{"filterReso",0.4f},{"unison",0.45f},{"stereoWidth",0.75f} } } },

        //== TRICHOME PLUCKS ==================================================
        { "TRICHOME > Pluck • LED",       { { {"grainDensity",0.85f},{"grainSize",0.12f},{"caudal",0.8f},{"soilDensity",0.22f},{"spray",0.08f},{"crossBlend",0.62f},{"filterCutoff",0.78f},{"filterReso",0.35f},{"drive",0.3f},{"unison",0.2f},{"stereoWidth",0.55f} } } },
        { "TRICHOME > Pluck • Static",    { { {"grainDensity",0.9f},{"grainSize",0.1f},{"caudal",0.85f},{"soilDensity",0.2f},{"crossBlend",0.7f},{"filterCutoff",0.72f},{"filterReso",0.45f},{"drive",0.4f},{"stereoWidth",0.6f} } } },
        { "TRICHOME > Pluck • Dew Drop",  { { {"grainDensity",0.8f},{"grainSize",0.14f},{"caudal",0.75f},{"soilDensity",0.3f},{"crossBlend",0.55f},{"filterCutoff",0.82f},{"filterMod",0.4f},{"unison",0.25f},{"stereoWidth",0.65f} } } },
        { "TRICHOME > Pluck • Nano",      { { {"grainDensity",0.95f},{"grainSize",0.08f},{"caudal",0.9f},{"soilDensity",0.18f},{"crossBlend",0.6f},{"filterCutoff",0.8f},{"drive",0.35f},{"unison",0.3f},{"unisonDetune",0.3f},{"stereoWidth",0.7f} } } },
        { "TRICHOME > Pluck • Snap",      { { {"grainDensity",0.88f},{"grainSize",0.11f},{"caudal",0.82f},{"soilDensity",0.25f},{"crossBlend",0.65f},{"filterType",0.5f},{"filterCutoff",0.7f},{"filterReso",0.5f},{"drive",0.45f},{"stereoWidth",0.6f} } } },

        //== ROOT BASS ========================================================
        { "ROOT > Bass • Tap Root",    { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.7f},{"grainSize",0.35f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterReso",0.18f},{"drive",0.45f},{"unison",0.15f},{"stereoWidth",0.3f},{"outputGain",0.85f} } } },
        { "ROOT > Bass • Sub Genome",  { { {"pitchA",0.38f},{"pitchB",0.38f},{"grainDensity",0.6f},{"grainSize",0.4f},{"crossBlend",0.4f},{"filterCutoff",0.35f},{"drive",0.35f},{"stereoWidth",0.25f},{"outputGain",0.88f} } } },
        { "ROOT > Bass • Growl",       { { {"pitchA",0.42f},{"pitchB",0.42f},{"grainDensity",0.75f},{"grainSize",0.28f},{"crossBlend",0.6f},{"filterType",0.5f},{"filterCutoff",0.5f},{"filterReso",0.55f},{"filterMod",0.4f},{"drive",0.55f},{"stereoWidth",0.4f} } } },
        { "ROOT > Bass • Reese",       { { {"pitchA",0.41f},{"pitchB",0.39f},{"grainDensity",0.8f},{"grainSize",0.3f},{"crossBlend",0.5f},{"filterCutoff",0.48f},{"filterReso",0.25f},{"drive",0.4f},{"unison",0.6f},{"unisonDetune",0.55f},{"stereoWidth",0.5f} } } },
        { "ROOT > Bass • Deep House",  { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.65f},{"grainSize",0.32f},{"caudal",0.7f},{"soilDensity",0.35f},{"crossBlend",0.45f},{"filterCutoff",0.4f},{"drive",0.3f},{"stereoWidth",0.35f} } } },

        //== PHENO LEADS ======================================================
        { "PHENO > Lead • Resin",     { { {"grainDensity",0.9f},{"grainSize",0.16f},{"crossBlend",0.7f},{"filterType",0.5f},{"filterCutoff",0.6f},{"filterReso",0.7f},{"filterMod",0.4f},{"drive",0.5f},{"unison",0.7f},{"unisonDetune",0.45f},{"stereoWidth",0.75f} } } },
        { "PHENO > Lead • Solar",     { { {"grainDensity",0.85f},{"grainSize",0.2f},{"crossBlend",0.6f},{"caudal",0.6f},{"filterCutoff",0.72f},{"filterReso",0.4f},{"drive",0.4f},{"unison",0.5f},{"unisonDetune",0.35f},{"stereoWidth",0.7f} } } },
        { "PHENO > Lead • Saw Bite",  { { {"grainDensity",0.88f},{"grainSize",0.18f},{"crossBlend",0.3f},{"filterCutoff",0.65f},{"filterReso",0.5f},{"filterMod",0.35f},{"drive",0.6f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.65f} } } },
        { "PHENO > Lead • Vox",       { { {"grainDensity",0.8f},{"grainSize",0.24f},{"crossBlend",0.55f},{"filterType",0.5f},{"filterCutoff",0.68f},{"filterReso",0.45f},{"filterMod",0.5f},{"unison",0.45f},{"stereoWidth",0.7f} } } },
        { "PHENO > Lead • Hard",      { { {"grainDensity",0.92f},{"grainSize",0.14f},{"crossBlend",0.75f},{"filterCutoff",0.6f},{"filterReso",0.6f},{"drive",0.7f},{"unison",0.7f},{"unisonDetune",0.5f},{"stereoWidth",0.8f} } } },

        //== CAPILLARY ARPS ===================================================
        { "CAPILLARY > Arp • Classic",  { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpMode",0.0f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.18f},{"grainDensity",0.82f},{"filterCutoff",0.55f},{"filterReso",0.45f},{"filterMod",0.6f},{"drive",0.22f} } } },
        { "CAPILLARY > Arp • Up-Down",  { { {"arpOn",1.0f},{"arpRate",0.6f},{"arpMode",0.6f},{"arpSync",1.0f},{"scaleType",0.5f},{"grainSize",0.16f},{"grainDensity",0.85f},{"filterCutoff",0.62f},{"filterMod",0.5f},{"unison",0.3f},{"stereoWidth",0.7f} } } },
        { "CAPILLARY > Arp • Random",   { { {"arpOn",1.0f},{"arpRate",0.55f},{"arpMode",1.0f},{"arpSync",1.0f},{"scaleType",0.75f},{"grainSize",0.2f},{"grainDensity",0.8f},{"filterCutoff",0.6f},{"filterReso",0.5f},{"filterMod",0.7f} } } },
        { "CAPILLARY > Arp • Triplet",  { { {"arpOn",1.0f},{"arpRate",0.66f},{"arpMode",0.35f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.15f},{"grainDensity",0.88f},{"filterCutoff",0.7f},{"drive",0.3f},{"stereoWidth",0.75f} } } },
        { "CAPILLARY > Arp • Cascade",  { { {"arpOn",1.0f},{"arpRate",0.8f},{"arpMode",0.6f},{"arpSync",1.0f},{"scaleType",0.5f},{"grainSize",0.12f},{"grainDensity",0.9f},{"filterCutoff",0.58f},{"filterReso",0.55f},{"filterMod",0.8f},{"unison",0.35f} } } },

        //== MYCELIUM TEXTURES ================================================
        { "MYCELIUM > Texture • Spore Cloud",   { { {"grainDensity",0.55f},{"grainSize",0.85f},{"spray",0.65f},{"caudal",0.15f},{"soilDensity",0.9f},{"modDepth",1.0f},{"crossBlend",0.5f},{"filterCutoff",0.5f},{"filterMod",0.8f},{"unison",0.55f},{"unisonDetune",0.4f},{"stereoWidth",1.0f} } } },
        { "MYCELIUM > Texture • Network",       { { {"grainDensity",0.6f},{"grainSize",0.75f},{"spray",0.55f},{"caudal",0.2f},{"soilDensity",0.85f},{"modDepth",0.9f},{"filterCutoff",0.55f},{"filterMod",0.6f},{"unison",0.5f},{"stereoWidth",0.95f} } } },
        { "MYCELIUM > Texture • Damp Earth",    { { {"grainDensity",0.5f},{"grainSize",0.8f},{"spray",0.7f},{"soilDensity",0.88f},{"filterCutoff",0.45f},{"filterMod",0.55f},{"drive",0.2f},{"unison",0.6f},{"unisonDetune",0.5f},{"stereoWidth",0.9f} } } },
        { "MYCELIUM > Texture • Bioluminescent",{ { {"grainDensity",0.65f},{"grainSize",0.7f},{"spray",0.5f},{"caudal",0.3f},{"soilDensity",0.7f},{"modDepth",0.8f},{"filterCutoff",0.6f},{"filterMod",0.7f},{"unison",0.45f},{"stereoWidth",1.0f} } } },
        { "MYCELIUM > Texture • Slow Bloom",    { { {"grainDensity",0.45f},{"grainSize",0.92f},{"spray",0.4f},{"caudal",0.12f},{"soilDensity",0.95f},{"modDepth",1.0f},{"crossBlend",0.45f},{"filterCutoff",0.48f},{"filterMod",0.75f},{"stereoWidth",0.95f} } } },

        //== CHLOROPHYLL STABS ================================================
        { "CHLOROPHYLL > Stab • Neon",   { { {"grainDensity",0.88f},{"grainSize",0.1f},{"caudal",0.85f},{"soilDensity",0.18f},{"crossBlend",0.6f},{"filterCutoff",0.72f},{"filterReso",0.4f},{"drive",0.55f},{"unison",0.35f},{"unisonDetune",0.3f},{"stereoWidth",0.85f} } } },
        { "CHLOROPHYLL > Stab • Brass",  { { {"grainDensity",0.85f},{"grainSize",0.14f},{"caudal",0.7f},{"soilDensity",0.3f},{"crossBlend",0.35f},{"filterCutoff",0.6f},{"filterReso",0.35f},{"filterMod",0.4f},{"drive",0.5f},{"unison",0.5f},{"stereoWidth",0.7f} } } },
        { "CHLOROPHYLL > Stab • House",  { { {"grainDensity",0.9f},{"grainSize",0.12f},{"caudal",0.8f},{"soilDensity",0.25f},{"crossBlend",0.55f},{"filterCutoff",0.68f},{"filterReso",0.5f},{"drive",0.4f},{"unison",0.4f},{"unisonDetune",0.35f},{"stereoWidth",0.8f} } } },
        { "CHLOROPHYLL > Stab • Orch",   { { {"grainDensity",0.8f},{"grainSize",0.16f},{"caudal",0.6f},{"soilDensity",0.4f},{"crossBlend",0.45f},{"filterCutoff",0.65f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.9f} } } },
        { "CHLOROPHYLL > Stab • Punch",  { { {"grainDensity",0.92f},{"grainSize",0.09f},{"caudal",0.9f},{"soilDensity",0.2f},{"crossBlend",0.65f},{"filterCutoff",0.75f},{"filterReso",0.45f},{"drive",0.6f},{"stereoWidth",0.75f} } } },

        //== SPORE FX =========================================================
        { "SPORE > FX • Riser",       { { {"grainDensity",0.7f},{"grainSize",0.6f},{"spray",0.5f},{"caudal",0.9f},{"soilDensity",0.1f},{"modDepth",1.0f},{"filterCutoff",0.5f},{"filterMod",1.0f},{"filterReso",0.6f},{"unison",0.7f},{"stereoWidth",1.0f} } } },
        { "SPORE > FX • Downlifter",  { { {"grainDensity",0.6f},{"grainSize",0.7f},{"spray",0.6f},{"caudal",0.1f},{"soilDensity",0.9f},{"modDepth",1.0f},{"filterCutoff",0.7f},{"filterMod",0.9f},{"stereoWidth",1.0f} } } },
        { "SPORE > FX • Metallic",    { { {"grainDensity",0.85f},{"grainSize",0.3f},{"spray",0.4f},{"crossBlend",0.5f},{"filterType",0.5f},{"filterCutoff",0.6f},{"filterReso",0.85f},{"drive",0.5f},{"unison",0.6f},{"unisonDetune",0.7f},{"stereoWidth",0.9f} } } },
        { "SPORE > FX • Glitch",      { { {"grainDensity",0.95f},{"grainSize",0.06f},{"spray",0.8f},{"caudal",0.9f},{"soilDensity",0.15f},{"crossBlend",0.6f},{"filterCutoff",0.7f},{"filterReso",0.6f},{"drive",0.6f},{"stereoWidth",1.0f} } } },
        { "SPORE > FX • Alien Choir", { { {"grainDensity",0.6f},{"grainSize",0.8f},{"spray",0.55f},{"caudal",0.25f},{"soilDensity",0.75f},{"modDepth",0.95f},{"crossBlend",0.5f},{"filterType",0.5f},{"filterCutoff",0.55f},{"filterMod",0.85f},{"unison",0.8f},{"stereoWidth",1.0f} } } },
    } };
}
