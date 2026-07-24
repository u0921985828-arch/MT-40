#pragma once

// ---------------------------------------------------------------------------
// DCBlocker — one-pole DC-removal high-pass on the master output.
//
//   y[n] = x[n] - x[n-1] + R * y[n-1]
//
// R = 0.9995 places the corner near 3.5 Hz at 44.1 kHz, so it nulls any DC
// build-up from asymmetric waveforms without touching the audible band
// (the lowest bass note, E2 ~82 Hz, is unaffected).
// ---------------------------------------------------------------------------
class DCBlocker
{
public:
    void reset() noexcept { x1 = 0.0f; y1 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    static constexpr float R = 0.9995f;
    float x1 = 0.0f, y1 = 0.0f;
};
