#pragma once

#include <array>

// ---------------------------------------------------------------------------
// MelodicPreset — per-patch metadata for the consonant-vowel melodic engine.
//
// A patch is defined entirely by DSP metadata (no samples): a harmonic family
// (the additive band-limited "vowel" wavetable, see WaveBank), a chorus detune
// + mix, a warmth low-pass, an ADSR envelope, and a short "consonant" attack
// transient (bright click for plucked/mallet tones, airy chiff for winds) that
// is the defining Casiotone characteristic.
// ---------------------------------------------------------------------------
struct MelodicPreset
{
    const char* name;
    const char* family;      // WaveBank harmonic family key
    float detuneCents;       // chorus detune between the two oscillators
    float chorus;            // chorus mix 0..1 (0 = single oscillator)
    float lpfCutoff;         // warmth low-pass cutoff, Hz
    float attack;            // ADSR, seconds
    float decay;
    float sustain;           // 0 = percussive (rings out while held)
    float release;
    float gain;              // per-patch output trim
    int   consType;          // 0 none, 1 bright click, 2 airy chiff
    float consLevel;         // transient level
    float consDecay;         // transient decay, seconds
};

// The 22 ST-40 tones, in panel order (numbers 1-22 printed above the
// keyboard). Percussive tones use sustain 0 (they ring out and die while
// held); sustained tones hold at their sustain level until note-off.
inline const std::array<MelodicPreset, 22>& getMelodicPresets()
{
    static const std::array<MelodicPreset, 22> presets = {{
        //  name             family      detune chorus  lpf    atk    dec   sus   rel   gain  cons clv   cdec
        { "electric piano", "epiano",    6.0f, 0.15f, 4200.f, 0.002f,1.60f,0.00f,0.35f,0.70f, 1, 0.25f,0.030f },
        { "banjo",          "pluck",     7.0f, 0.00f, 5000.f, 0.001f,0.35f,0.00f,0.14f,0.62f, 1, 0.40f,0.020f },
        { "guitar",         "pluck",     6.0f, 0.10f, 3600.f, 0.001f,0.90f,0.00f,0.22f,0.66f, 1, 0.30f,0.025f },
        { "harpsichord",    "harpsi",    6.0f, 0.12f, 5200.f, 0.001f,0.60f,0.00f,0.18f,0.60f, 1, 0.35f,0.020f },
        { "xylophone",      "mallet",    4.0f, 0.00f, 6000.f, 0.001f,0.45f,0.00f,0.15f,0.60f, 1, 0.30f,0.015f },
        { "celesta",        "mallet",    5.0f, 0.10f, 5000.f, 0.001f,0.90f,0.00f,0.20f,0.60f, 1, 0.25f,0.020f },
        { "glockenspiel",   "mallet",    5.0f, 0.00f, 7000.f, 0.001f,1.40f,0.00f,0.25f,0.58f, 1, 0.30f,0.012f },
        { "organ",          "organ",     4.0f, 0.08f, 4500.f, 0.004f,0.05f,1.00f,0.10f,0.60f, 0, 0.00f,0.020f },
        { "accordion",      "reed",     12.0f, 0.50f, 4000.f, 0.012f,0.08f,0.95f,0.16f,0.56f, 0, 0.00f,0.020f },
        { "pipe organ",     "pipe",      3.0f, 0.10f, 5000.f, 0.004f,0.05f,1.00f,0.09f,0.60f, 2, 0.06f,0.050f },
        { "oriental pipe",  "reed",      6.0f, 0.20f, 3200.f, 0.010f,0.10f,0.90f,0.16f,0.58f, 0, 0.00f,0.020f },
        { "brass",          "brass",     7.0f, 0.20f, 4200.f, 0.030f,0.12f,0.90f,0.20f,0.58f, 1, 0.08f,0.030f },
        { "cello",          "string",    9.0f, 0.40f, 3000.f, 0.060f,0.15f,0.90f,0.30f,0.55f, 2, 0.05f,0.050f },
        { "synth fuzz",     "fuzz",     14.0f, 0.20f, 5000.f, 0.006f,0.10f,0.95f,0.22f,0.50f, 0, 0.00f,0.020f },
        { "violin",         "string",    8.0f, 0.35f, 4200.f, 0.070f,0.15f,0.90f,0.28f,0.54f, 2, 0.05f,0.050f },
        { "trumpet",        "brass",     7.0f, 0.15f, 5000.f, 0.010f,0.10f,0.92f,0.20f,0.56f, 1, 0.10f,0.030f },
        { "funny fuzz",     "fuzz",     16.0f, 0.25f, 3800.f, 0.006f,0.10f,0.90f,0.30f,0.50f, 0, 0.00f,0.020f },
        { "st. ensemble",   "string",   12.0f, 0.80f, 4000.f, 0.090f,0.20f,0.90f,0.40f,0.52f, 0, 0.00f,0.020f },
        { "clarinet",       "clarinet",  5.0f, 0.15f, 3600.f, 0.030f,0.08f,0.95f,0.24f,0.60f, 2, 0.05f,0.040f },
        { "flute",          "flute",     4.0f, 0.15f, 4000.f, 0.050f,0.10f,0.90f,0.26f,0.62f, 2, 0.12f,0.050f },
        { "recorder",       "flute",     3.0f, 0.10f, 4600.f, 0.035f,0.08f,0.92f,0.22f,0.62f, 2, 0.14f,0.045f },
        { "folk flute",     "flute",     5.0f, 0.15f, 4200.f, 0.050f,0.10f,0.90f,0.28f,0.60f, 2, 0.12f,0.050f },
    }};
    return presets;
}
