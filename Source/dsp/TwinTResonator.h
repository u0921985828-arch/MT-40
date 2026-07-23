#pragma once

#include "Biquad.h"

// ---------------------------------------------------------------------------
// TwinT_Resonator — analog kick drum model (§4.1).
//
// The original ST-40 kick is a Twin-T oscillator/resonator circuit. We model
// it as a 2-pole band-pass biquad pushed to a very high Q (near self-
// oscillation), excited by a 1 ms unit impulse. The high Q gives the ~250 ms
// resonant decay; a tanh soft-clip at the output stage emulates transistor
// saturation.
//
//   fc = 55 Hz, Q = 18.5, drive via tanh.
// ---------------------------------------------------------------------------
class TwinT_Resonator
{
public:
    void prepare (double sampleRate) noexcept;

    // Fire the drum: schedules a 1 ms unit impulse.
    void trigger() noexcept;

    void reset() noexcept;

    bool isActive() const noexcept { return impulseSamplesLeft > 0 || active; }

    // Produce one output sample.
    float process() noexcept;

    void setDrive (float d) noexcept { drive = d; }

private:
    Biquad bpf;
    double fs = 44100.0;
    int impulseSamplesLeft = 0;   // remaining samples of the 1 ms excitation
    float drive = 4.0f;
    bool active = false;
    float envFloor = 1.0e-4f;
    float lastMag = 0.0f;
};
