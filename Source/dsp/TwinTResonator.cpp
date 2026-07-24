#include "TwinTResonator.h"
#include <cmath>
#include <algorithm>

void TwinT_Resonator::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    bpf.setBandPass (fs, 55.0, 18.5); // fc = 55 Hz, Q = 18.5
    reset();
}

void TwinT_Resonator::reset() noexcept
{
    bpf.reset();
    impulseSamplesLeft = 0;
    active = false;
    lastMag = 0.0f;
}

void TwinT_Resonator::trigger() noexcept
{
    bpf.reset();
    impulseSamplesLeft = std::max (1, static_cast<int> (0.001 * fs)); // 1 ms
    active = true;
}

float TwinT_Resonator::process() noexcept
{
    if (! active) return 0.0f;

    float x = 0.0f;
    if (impulseSamplesLeft > 0)
    {
        x = 1.0f; // unit impulse for the 1 ms excitation window
        --impulseSamplesLeft;
    }

    float y = bpf.process (x);

    // Transistor saturation at output stage.
    y = std::tanh (drive * y);

    // Track magnitude to know when the resonance has rung out.
    lastMag = 0.999f * lastMag + 0.001f * std::fabs (y);
    if (impulseSamplesLeft == 0 && lastMag < envFloor)
        active = false;

    return y;
}
