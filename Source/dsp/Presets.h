#pragma once

#include <array>

// ---------------------------------------------------------------------------
// MelodicPreset — per-patch metadata for the DCO / CV melodic engine (§3).
//
// A patch is defined entirely by DSP metadata (no samples): the duty cycle of
// each of the two multiplexed pulse waves, the fixed detune between them (the
// inherent MT-40 "chorused" fatness), the static analog LPF cutoff, and the
// two-stage envelope times.
// ---------------------------------------------------------------------------
struct MelodicPreset
{
    const char* name;
    float duty1;        // duty cycle of pulse osc 1 (0..1)
    float duty2;        // duty cycle of pulse osc 2
    float detuneCents;  // fixed offset between the two internal pulses
    float lpfCutoff;    // static analog LPF cutoff in Hz (§3.2)
    float attack;       // linear attack time, seconds
    float release;      // exponential release time, seconds
    float gain;         // per-patch output trim
};

// 22 patches (Program Change 0-21). Values are tuned to the descriptions in
// §3: 50% square -> clarinet family, 12.5% narrow pulse -> strings, bright
// patches ~4 kHz cutoff, dark patches ~1.2 kHz.
inline const std::array<MelodicPreset, 22>& getMelodicPresets()
{
    static const std::array<MelodicPreset, 22> presets = {{
        //  name          duty1  duty2  detune  lpf     atk     rel    gain
        { "Piano",        0.50f, 0.25f,  6.0f, 3200.0f, 0.002f, 0.35f, 0.85f },
        { "Flute",        0.50f, 0.50f,  4.0f, 1200.0f, 0.030f, 0.30f, 0.80f },
        { "Clarinet",     0.50f, 0.50f,  5.0f, 2000.0f, 0.010f, 0.28f, 0.80f },
        { "Oboe",         0.30f, 0.30f,  6.0f, 2600.0f, 0.012f, 0.26f, 0.78f },
        { "Trumpet",      0.35f, 0.20f,  7.0f, 4000.0f, 0.008f, 0.22f, 0.82f },
        { "Trombone",     0.40f, 0.25f,  7.0f, 2200.0f, 0.014f, 0.28f, 0.80f },
        { "Violin",       0.125f,0.125f, 8.0f, 3000.0f, 0.040f, 0.35f, 0.72f },
        { "Cello",        0.125f,0.20f,  9.0f, 1600.0f, 0.045f, 0.40f, 0.72f },
        { "Strings",      0.125f,0.125f,12.0f, 2800.0f, 0.060f, 0.55f, 0.70f },
        { "Pipe Organ",   0.50f, 0.50f,  3.0f, 3600.0f, 0.004f, 0.10f, 0.85f },
        { "Reed Organ",   0.50f, 0.33f,  5.0f, 2400.0f, 0.010f, 0.14f, 0.82f },
        { "Jazz Organ",   0.50f, 0.25f,  6.0f, 3800.0f, 0.003f, 0.12f, 0.84f },
        { "Accordion",    0.40f, 0.40f, 10.0f, 2600.0f, 0.012f, 0.20f, 0.78f },
        { "Harpsichord",  0.25f, 0.15f,  6.0f, 4000.0f, 0.001f, 0.20f, 0.80f },
        { "Vibraphone",   0.50f, 0.50f,  4.0f, 2600.0f, 0.001f, 0.60f, 0.75f },
        { "Music Box",    0.30f, 0.30f,  5.0f, 3400.0f, 0.001f, 0.45f, 0.72f },
        { "Guitar",       0.40f, 0.20f,  6.0f, 2800.0f, 0.001f, 0.30f, 0.80f },
        { "Banjo",        0.25f, 0.15f,  7.0f, 3600.0f, 0.001f, 0.18f, 0.78f },
        { "Fantasy",      0.20f, 0.40f, 14.0f, 3000.0f, 0.030f, 0.50f, 0.70f },
        { "Cosmic",       0.15f, 0.35f, 16.0f, 2400.0f, 0.050f, 0.60f, 0.68f },
        { "Whistle",      0.50f, 0.50f,  2.0f, 1400.0f, 0.020f, 0.24f, 0.78f },
        { "Human Voice",  0.35f, 0.45f, 10.0f, 1800.0f, 0.040f, 0.40f, 0.72f },
    }};
    return presets;
}
