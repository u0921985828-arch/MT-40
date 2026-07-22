#pragma once

#include "Biquad.h"
#include "ExpDecay.h"
#include "LFSRNoise.h"

// ---------------------------------------------------------------------------
// SnareDrum — parallel tonal body + noise "wires" (§4.2).
//
//   * Tonal body : biquad BPF (fc = 220 Hz, Q = 6.0), impulse-excited, short
//                  exponential body decay.
//   * Snare wires: LFSR white noise -> 2nd-order HPF (fc = 2.5 kHz) ->
//                  exponential decay (~150 ms).
//   * Mix        : noise amplitude > tonal amplitude.
//
// The noise source is the shared global LFSR (passed by reference) so it stays
// coherent with the hi-hat generator.
// ---------------------------------------------------------------------------
class SnareDrum
{
public:
    explicit SnareDrum (LFSRNoise& sharedNoise) : noise (sharedNoise) {}

    void prepare (double sampleRate) noexcept;
    void trigger() noexcept;
    void reset() noexcept;
    bool isActive() const noexcept;
    float process() noexcept;

private:
    LFSRNoise& noise;
    Biquad bodyBpf;
    Biquad noiseHpf;      // 2nd-order high-pass
    ExpDecay bodyEnv;
    ExpDecay noiseEnv;
    double fs = 44100.0;
    int impulseSamplesLeft = 0;

    static constexpr float kTonalAmp = 0.4f;
    static constexpr float kNoiseAmp = 0.85f; // noise > tonal
};
