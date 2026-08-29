#pragma once

//============================================================================
//  Presets.h
//
//  60 factory presets = 10 libraries x 6 types (Pad/Key/Bass/Lead/Pluck/Arp).
//  Every library carries the full type set so each collection is self-
//  contained. Exposed to the host as a flat program list named
//  "LIBRARY > Type . Preset". Keyed by parameter id (order-independent);
//  unused override slots hold {nullptr,0} and are skipped by the loader.
//============================================================================

#include <array>

namespace phenotype::presets
{
    struct KV { const char* id = nullptr; float value = 0.0f; };

    struct Preset
    {
        const char* name;
        std::array<KV, 14> overrides;
    };

    inline constexpr std::array<Preset, 60> kFactory { {
        //== GENESIS =====================================================
        { "GENESIS > Pad • Chlorophyll", { { {"grainDensity",0.68f},{"grainSize",0.8f},{"caudal",0.3f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.45f},{"unison",0.55f},{"unisonDetune",0.35f},{"filterCutoff",0.65f},{"filterMod",0.4f},{"drive",0.14f},{"stereoWidth",0.85f}, } } },
        { "GENESIS > Key • Bell Jar", { { {"grainDensity",0.82f},{"grainSize",0.2f},{"caudal",0.62f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.35f},{"unisonDetune",0.22f},{"filterCutoff",0.85f},{"filterReso",0.22f},{"drive",0.16f},{"stereoWidth",0.65f}, } } },
        { "GENESIS > Bass • Tap Root", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.68f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterReso",0.2f},{"drive",0.42f},{"unison",0.2f},{"stereoWidth",0.32f},{"outputGain",0.85f}, } } },
        { "GENESIS > Lead • Filament", { { {"grainDensity",0.88f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.64f},{"filterReso",0.5f},{"filterMod",0.35f},{"drive",0.48f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.72f}, } } },
        { "GENESIS > Pluck • LED", { { {"grainDensity",0.9f},{"grainSize",0.11f},{"caudal",0.82f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.78f},{"filterReso",0.38f},{"drive",0.32f},{"unison",0.25f},{"stereoWidth",0.6f}, } } },
        { "GENESIS > Arp • Classic", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.84f},{"filterCutoff",0.58f},{"filterReso",0.42f},{"filterMod",0.55f},{"drive",0.22f},{"stereoWidth",0.7f}, } } },
        //== GROW ROOM ===================================================
        { "GROW ROOM > Pad • Morning Dew", { { {"grainDensity",0.68f},{"grainSize",0.8f},{"caudal",0.25f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.45f},{"unison",0.55f},{"unisonDetune",0.35f},{"filterCutoff",0.57f},{"filterMod",0.4f},{"drive",0.29f},{"stereoWidth",0.85f}, } } },
        { "GROW ROOM > Key • Crystal", { { {"grainDensity",0.82f},{"grainSize",0.2f},{"caudal",0.57f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.35f},{"unisonDetune",0.22f},{"filterCutoff",0.77f},{"filterReso",0.22f},{"drive",0.31f},{"stereoWidth",0.65f}, } } },
        { "GROW ROOM > Bass • Sub Genome", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.68f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.34f},{"filterReso",0.2f},{"drive",0.57f},{"unison",0.2f},{"stereoWidth",0.32f},{"outputGain",0.85f},{"caudal",0.45f}, } } },
        { "GROW ROOM > Lead • Solar", { { {"grainDensity",0.88f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.56f},{"filterReso",0.5f},{"filterMod",0.35f},{"drive",0.63f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.72f},{"caudal",0.45f}, } } },
        { "GROW ROOM > Pluck • Static", { { {"grainDensity",0.9f},{"grainSize",0.11f},{"caudal",0.77f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.7f},{"filterReso",0.38f},{"drive",0.47f},{"unison",0.25f},{"stereoWidth",0.6f}, } } },
        { "GROW ROOM > Arp • Up-Down", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.84f},{"filterCutoff",0.5f},{"filterReso",0.42f},{"filterMod",0.55f},{"drive",0.37f},{"stereoWidth",0.7f},{"caudal",0.45f}, } } },
        //== HARVEST =====================================================
        { "HARVEST > Pad • Glass House", { { {"grainDensity",0.71f},{"grainSize",0.8f},{"caudal",0.3f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.45f},{"unison",0.55f},{"unisonDetune",0.35f},{"filterCutoff",0.77f},{"filterMod",0.4f},{"drive",0.14f},{"stereoWidth",0.85f},{"filterReso",0.17f}, } } },
        { "HARVEST > Key • Mallet", { { {"grainDensity",0.85f},{"grainSize",0.2f},{"caudal",0.62f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.35f},{"unisonDetune",0.22f},{"filterCutoff",0.97f},{"filterReso",0.27f},{"drive",0.16f},{"stereoWidth",0.65f}, } } },
        { "HARVEST > Bass • Growl", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.71f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.54f},{"filterReso",0.25f},{"drive",0.42f},{"unison",0.2f},{"stereoWidth",0.32f},{"outputGain",0.85f}, } } },
        { "HARVEST > Lead • Saw Bite", { { {"grainDensity",0.91f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.76f},{"filterReso",0.55f},{"filterMod",0.35f},{"drive",0.48f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.72f}, } } },
        { "HARVEST > Pluck • Dew Drop", { { {"grainDensity",0.93f},{"grainSize",0.11f},{"caudal",0.82f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.9f},{"filterReso",0.43f},{"drive",0.32f},{"unison",0.25f},{"stereoWidth",0.6f}, } } },
        { "HARVEST > Arp • Random", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.87f},{"filterCutoff",0.7f},{"filterReso",0.47f},{"filterMod",0.55f},{"drive",0.22f},{"stereoWidth",0.7f}, } } },
        //== LANDRACE ====================================================
        { "LANDRACE > Pad • Photon Wash", { { {"grainDensity",0.68f},{"grainSize",0.8f},{"caudal",0.3f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.45f},{"unison",0.45f},{"unisonDetune",0.35f},{"filterCutoff",0.5f},{"filterMod",0.4f},{"drive",0.19f},{"stereoWidth",0.75f}, } } },
        { "LANDRACE > Key • Music Box", { { {"grainDensity",0.82f},{"grainSize",0.2f},{"caudal",0.62f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.25f},{"unisonDetune",0.22f},{"filterCutoff",0.7f},{"filterReso",0.22f},{"drive",0.21f},{"stereoWidth",0.55f}, } } },
        { "LANDRACE > Bass • Reese", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.68f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.27f},{"filterReso",0.2f},{"drive",0.47f},{"unison",0.1f},{"stereoWidth",0.22f},{"outputGain",0.85f}, } } },
        { "LANDRACE > Lead • Vox", { { {"grainDensity",0.88f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.49f},{"filterReso",0.5f},{"filterMod",0.35f},{"drive",0.53f},{"unison",0.5f},{"unisonDetune",0.4f},{"stereoWidth",0.62f}, } } },
        { "LANDRACE > Pluck • Nano", { { {"grainDensity",0.9f},{"grainSize",0.11f},{"caudal",0.82f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.63f},{"filterReso",0.38f},{"drive",0.37f},{"unison",0.15f}, } } },
        { "LANDRACE > Arp • Triplet", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.84f},{"filterCutoff",0.43f},{"filterReso",0.42f},{"filterMod",0.55f},{"drive",0.27f},{"stereoWidth",0.6f}, } } },
        //== NEON ========================================================
        { "NEON > Pad • Terpene Air", { { {"grainDensity",0.68f},{"grainSize",0.8f},{"caudal",0.3f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.45f},{"unison",0.55f},{"unisonDetune",0.35f},{"filterCutoff",0.65f},{"filterMod",0.55f},{"drive",0.14f},{"stereoWidth",0.97f},{"filterReso",0.3f}, } } },
        { "NEON > Key • Frozen", { { {"grainDensity",0.82f},{"grainSize",0.2f},{"caudal",0.62f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.35f},{"unisonDetune",0.22f},{"filterCutoff",0.85f},{"filterReso",0.4f},{"drive",0.16f},{"stereoWidth",0.77f},{"filterMod",0.15f}, } } },
        { "NEON > Bass • Deep House", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.68f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterReso",0.38f},{"drive",0.42f},{"unison",0.2f},{"stereoWidth",0.44f},{"outputGain",0.85f},{"filterMod",0.15f}, } } },
        { "NEON > Lead • Hard", { { {"grainDensity",0.88f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.64f},{"filterReso",0.68f},{"filterMod",0.5f},{"drive",0.48f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.84f}, } } },
        { "NEON > Pluck • Snap", { { {"grainDensity",0.9f},{"grainSize",0.11f},{"caudal",0.82f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.78f},{"filterReso",0.56f},{"drive",0.32f},{"unison",0.25f},{"stereoWidth",0.72f},{"filterMod",0.15f}, } } },
        { "NEON > Arp • Cascade", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.84f},{"filterCutoff",0.58f},{"filterReso",0.6f},{"filterMod",0.7f},{"drive",0.22f},{"stereoWidth",0.82f}, } } },
        //== ORGANIC =====================================================
        { "ORGANIC > Pad • Canopy", { { {"grainDensity",0.68f},{"grainSize",0.8f},{"caudal",0.25f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.45f},{"unison",0.55f},{"unisonDetune",0.5f},{"filterCutoff",0.65f},{"filterMod",0.4f},{"drive",0.14f},{"stereoWidth",0.93f},{"spray",0.32f}, } } },
        { "ORGANIC > Key • Dewpoint", { { {"grainDensity",0.82f},{"grainSize",0.2f},{"caudal",0.57f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.35f},{"unisonDetune",0.37f},{"filterCutoff",0.85f},{"filterReso",0.22f},{"drive",0.16f},{"stereoWidth",0.73f},{"spray",0.32f}, } } },
        { "ORGANIC > Bass • Bedrock", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.68f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterReso",0.2f},{"drive",0.42f},{"unison",0.2f},{"stereoWidth",0.4f},{"outputGain",0.85f},{"unisonDetune",0.4f},{"spray",0.32f},{"caudal",0.45f}, } } },
        { "ORGANIC > Lead • Cutting", { { {"grainDensity",0.88f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.64f},{"filterReso",0.5f},{"filterMod",0.35f},{"drive",0.48f},{"unison",0.6f},{"unisonDetune",0.55f},{"stereoWidth",0.8f},{"spray",0.32f},{"caudal",0.45f}, } } },
        { "ORGANIC > Pluck • Spark", { { {"grainDensity",0.9f},{"grainSize",0.11f},{"caudal",0.77f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.78f},{"filterReso",0.38f},{"drive",0.32f},{"unison",0.25f},{"stereoWidth",0.68f},{"unisonDetune",0.4f},{"spray",0.32f}, } } },
        { "ORGANIC > Arp • Photosynth", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.84f},{"filterCutoff",0.58f},{"filterReso",0.42f},{"filterMod",0.55f},{"drive",0.22f},{"stereoWidth",0.78f},{"unisonDetune",0.4f},{"spray",0.32f},{"caudal",0.45f}, } } },
        //== RESIN LAB ===================================================
        { "RESIN LAB > Pad • Stomata", { { {"grainDensity",0.68f},{"grainSize",0.8f},{"caudal",0.3f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.45f},{"unison",0.55f},{"unisonDetune",0.35f},{"filterCutoff",0.65f},{"filterMod",0.5f},{"drive",0.36f},{"stereoWidth",0.85f},{"filterReso",0.27f}, } } },
        { "RESIN LAB > Key • Prism", { { {"grainDensity",0.82f},{"grainSize",0.2f},{"caudal",0.62f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.35f},{"unisonDetune",0.22f},{"filterCutoff",0.85f},{"filterReso",0.37f},{"drive",0.38f},{"stereoWidth",0.65f},{"filterMod",0.1f}, } } },
        { "RESIN LAB > Bass • Rhizome", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.68f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterReso",0.35f},{"drive",0.64f},{"unison",0.2f},{"stereoWidth",0.32f},{"outputGain",0.85f},{"filterMod",0.1f}, } } },
        { "RESIN LAB > Lead • Helix", { { {"grainDensity",0.88f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.64f},{"filterReso",0.65f},{"filterMod",0.45f},{"drive",0.7f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.72f}, } } },
        { "RESIN LAB > Pluck • Pollen", { { {"grainDensity",0.9f},{"grainSize",0.11f},{"caudal",0.82f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.78f},{"filterReso",0.53f},{"drive",0.54f},{"unison",0.25f},{"stereoWidth",0.6f},{"filterMod",0.1f}, } } },
        { "RESIN LAB > Arp • Drip", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.84f},{"filterCutoff",0.58f},{"filterReso",0.57f},{"filterMod",0.65f},{"drive",0.44f},{"stereoWidth",0.7f}, } } },
        //== SPECTRUM ====================================================
        { "SPECTRUM > Pad • Aurora Leaf", { { {"grainDensity",0.68f},{"grainSize",0.8f},{"caudal",0.3f},{"soilDensity",0.72f},{"modDepth",0.85f},{"crossBlend",0.45f},{"unison",0.55f},{"unisonDetune",0.35f},{"filterCutoff",0.65f},{"filterMod",0.65f},{"drive",0.14f},{"stereoWidth",1.0f}, } } },
        { "SPECTRUM > Key • Glass Rod", { { {"grainDensity",0.82f},{"grainSize",0.2f},{"caudal",0.62f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.35f},{"unisonDetune",0.22f},{"filterCutoff",0.85f},{"filterReso",0.22f},{"drive",0.16f},{"stereoWidth",0.8f},{"filterMod",0.25f},{"modDepth",0.65f}, } } },
        { "SPECTRUM > Bass • Mycelium", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.68f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterReso",0.2f},{"drive",0.42f},{"unison",0.2f},{"stereoWidth",0.47f},{"outputGain",0.85f},{"filterMod",0.25f},{"modDepth",0.65f}, } } },
        { "SPECTRUM > Lead • Nitro", { { {"grainDensity",0.88f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.64f},{"filterReso",0.5f},{"filterMod",0.6f},{"drive",0.48f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.87f},{"modDepth",0.65f}, } } },
        { "SPECTRUM > Pluck • Pixel", { { {"grainDensity",0.9f},{"grainSize",0.11f},{"caudal",0.82f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.78f},{"filterReso",0.38f},{"drive",0.32f},{"unison",0.25f},{"stereoWidth",0.75f},{"filterMod",0.25f},{"modDepth",0.65f}, } } },
        { "SPECTRUM > Arp • Lattice", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.84f},{"filterCutoff",0.58f},{"filterReso",0.42f},{"filterMod",0.8f},{"drive",0.22f},{"stereoWidth",0.85f},{"modDepth",0.65f}, } } },
        //== HYBRID ======================================================
        { "HYBRID > Pad • Xylem", { { {"grainDensity",0.7f},{"grainSize",0.8f},{"caudal",0.3f},{"soilDensity",0.72f},{"modDepth",0.7f},{"crossBlend",0.45f},{"unison",0.6f},{"unisonDetune",0.35f},{"filterCutoff",0.65f},{"filterMod",0.4f},{"drive",0.14f},{"stereoWidth",0.85f}, } } },
        { "HYBRID > Key • Chime", { { {"grainDensity",0.84f},{"grainSize",0.2f},{"caudal",0.62f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.4f},{"unisonDetune",0.22f},{"filterCutoff",0.85f},{"filterReso",0.22f},{"drive",0.16f},{"stereoWidth",0.65f}, } } },
        { "HYBRID > Bass • Fault Line", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.7f},{"grainSize",0.34f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterReso",0.2f},{"drive",0.42f},{"unison",0.25f},{"stereoWidth",0.32f},{"outputGain",0.85f}, } } },
        { "HYBRID > Lead • Apex", { { {"grainDensity",0.9f},{"grainSize",0.17f},{"crossBlend",0.62f},{"filterCutoff",0.64f},{"filterReso",0.5f},{"filterMod",0.35f},{"drive",0.48f},{"unison",0.65f},{"unisonDetune",0.4f},{"stereoWidth",0.72f}, } } },
        { "HYBRID > Pluck • Needle", { { {"grainDensity",0.92f},{"grainSize",0.11f},{"caudal",0.82f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.78f},{"filterReso",0.38f},{"drive",0.32f},{"unison",0.3f},{"stereoWidth",0.6f}, } } },
        { "HYBRID > Arp • Pulse", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.17f},{"grainDensity",0.86f},{"filterCutoff",0.58f},{"filterReso",0.42f},{"filterMod",0.55f},{"drive",0.22f},{"stereoWidth",0.7f},{"unison",0.05f}, } } },
        //== SPORE =======================================================
        { "SPORE > Pad • Sea of Green", { { {"grainDensity",0.68f},{"grainSize",0.9f},{"caudal",0.3f},{"soilDensity",0.72f},{"modDepth",0.9f},{"crossBlend",0.45f},{"unison",0.55f},{"unisonDetune",0.35f},{"filterCutoff",0.65f},{"filterMod",0.6f},{"drive",0.14f},{"stereoWidth",0.85f},{"spray",0.45f}, } } },
        { "SPORE > Key • Icicle", { { {"grainDensity",0.82f},{"caudal",0.62f},{"soilDensity",0.4f},{"crossBlend",0.55f},{"unison",0.35f},{"unisonDetune",0.22f},{"filterCutoff",0.85f},{"filterReso",0.22f},{"drive",0.16f},{"stereoWidth",0.65f},{"spray",0.45f},{"modDepth",0.7f},{"filterMod",0.2f}, } } },
        { "SPORE > Bass • Magma", { { {"pitchA",0.4f},{"pitchB",0.4f},{"grainDensity",0.68f},{"grainSize",0.44f},{"crossBlend",0.45f},{"filterCutoff",0.42f},{"filterReso",0.2f},{"drive",0.42f},{"unison",0.2f},{"stereoWidth",0.32f},{"outputGain",0.85f},{"spray",0.45f},{"modDepth",0.7f},{"filterMod",0.2f}, } } },
        { "SPORE > Lead • Signal", { { {"grainDensity",0.88f},{"grainSize",0.27f},{"crossBlend",0.62f},{"filterCutoff",0.64f},{"filterReso",0.5f},{"filterMod",0.55f},{"drive",0.48f},{"unison",0.6f},{"unisonDetune",0.4f},{"stereoWidth",0.72f},{"spray",0.45f},{"modDepth",0.7f}, } } },
        { "SPORE > Pluck • Frost", { { {"grainDensity",0.9f},{"grainSize",0.21f},{"caudal",0.82f},{"soilDensity",0.22f},{"crossBlend",0.6f},{"filterCutoff",0.78f},{"filterReso",0.38f},{"drive",0.32f},{"unison",0.25f},{"stereoWidth",0.6f},{"spray",0.45f},{"modDepth",0.7f},{"filterMod",0.2f}, } } },
        { "SPORE > Arp • Spiral", { { {"arpOn",1.0f},{"arpRate",0.5f},{"arpSync",1.0f},{"scaleType",0.25f},{"grainSize",0.27f},{"grainDensity",0.84f},{"filterCutoff",0.58f},{"filterReso",0.42f},{"filterMod",0.75f},{"drive",0.22f},{"stereoWidth",0.7f},{"spray",0.45f},{"modDepth",0.7f}, } } },
    } };
}
