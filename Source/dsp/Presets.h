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

// The 22 real MT-40 tones, in panel order (numbers 1-22 printed above the
// keyboard). DSP metadata per §3: 50% square -> reed/flute family, narrow
// pulse -> plucked/strings, bright patches ~4 kHz cutoff, dark ~1.2 kHz.
inline const std::array<MelodicPreset, 22>& getMelodicPresets()
{
    static const std::array<MelodicPreset, 22> presets = {{
        //  name             duty1  duty2  detune  lpf     atk     rel    gain
        { "electric piano", 0.50f, 0.25f,  6.0f, 3000.0f, 0.002f, 0.35f, 0.85f },
        { "banjo",          0.20f, 0.12f,  7.0f, 3600.0f, 0.001f, 0.16f, 0.80f },
        { "guitar",         0.40f, 0.20f,  6.0f, 2800.0f, 0.001f, 0.30f, 0.82f },
        { "harpsichord",    0.25f, 0.15f,  6.0f, 4000.0f, 0.001f, 0.22f, 0.80f },
        { "xylophone",      0.50f, 0.50f,  4.0f, 3200.0f, 0.001f, 0.28f, 0.74f },
        { "celesta",        0.50f, 0.33f,  5.0f, 2800.0f, 0.001f, 0.45f, 0.74f },
        { "glockenspiel",   0.30f, 0.30f,  5.0f, 3800.0f, 0.001f, 0.40f, 0.72f },
        { "organ",          0.50f, 0.50f,  4.0f, 2600.0f, 0.004f, 0.12f, 0.84f },
        { "accordion",      0.40f, 0.40f, 10.0f, 2600.0f, 0.012f, 0.18f, 0.80f },
        { "pipe organ",     0.50f, 0.50f,  3.0f, 3600.0f, 0.004f, 0.10f, 0.85f },
        { "oriental pipe",  0.33f, 0.33f,  6.0f, 2200.0f, 0.010f, 0.16f, 0.80f },
        { "brass",          0.35f, 0.20f,  7.0f, 3600.0f, 0.010f, 0.22f, 0.82f },
        { "cello",          0.125f,0.20f,  9.0f, 1600.0f, 0.045f, 0.40f, 0.74f },
        { "synth fuzz",     0.15f, 0.30f, 14.0f, 3200.0f, 0.006f, 0.30f, 0.72f },
        { "violin",         0.125f,0.125f, 8.0f, 3000.0f, 0.040f, 0.35f, 0.74f },
        { "trumpet",        0.35f, 0.20f,  7.0f, 4000.0f, 0.008f, 0.22f, 0.82f },
        { "funny fuzz",     0.12f, 0.35f, 16.0f, 2600.0f, 0.006f, 0.34f, 0.70f },
        { "st. ensemble",   0.125f,0.125f,12.0f, 2800.0f, 0.060f, 0.55f, 0.72f },
        { "clarinet",       0.50f, 0.50f,  5.0f, 2000.0f, 0.010f, 0.28f, 0.80f },
        { "flute",          0.50f, 0.50f,  4.0f, 1200.0f, 0.030f, 0.30f, 0.80f },
        { "recorder",       0.50f, 0.45f,  3.0f, 1600.0f, 0.020f, 0.24f, 0.78f },
        { "folk flute",     0.50f, 0.40f,  5.0f, 1400.0f, 0.030f, 0.32f, 0.76f },
    }};
    return presets;
}
