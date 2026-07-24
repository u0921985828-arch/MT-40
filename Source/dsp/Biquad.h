#pragma once

#include <cmath>

// ---------------------------------------------------------------------------
// Biquad — Direct Form I second-order IIR section.
//
//   y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]
//
// Coefficients are stored already normalised by a0 (RBJ cookbook convention).
// This matches the transfer-function reference in §4.1 of the architecture doc.
// ---------------------------------------------------------------------------
class Biquad
{
public:
    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }

    inline float process (float x) noexcept
    {
        const double in = static_cast<double> (x);
        const double out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = in;
        y2 = y1; y1 = out;
        return static_cast<float> (out);
    }

    // --- RBJ cookbook designers ------------------------------------------
    void setBandPass (double fs, double fc, double q) noexcept
    {
        // Constant skirt gain, peak gain = Q (BPF, "0 dB peak" variant uses Q norm).
        const double w0 = 2.0 * M_PI * fc / fs;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double cw = std::cos (w0);

        const double a0 = 1.0 + alpha;
        b0 = (q * alpha) / a0;   // peak gain proportional to Q -> resonant "ping"
        b1 = 0.0;
        b2 = (-q * alpha) / a0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setHighPass (double fs, double fc, double q) noexcept
    {
        const double w0 = 2.0 * M_PI * fc / fs;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double cw = std::cos (w0);

        const double a0 = 1.0 + alpha;
        b0 = ((1.0 + cw) * 0.5) / a0;
        b1 = (-(1.0 + cw)) / a0;
        b2 = ((1.0 + cw) * 0.5) / a0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setLowPass (double fs, double fc, double q) noexcept
    {
        const double w0 = 2.0 * M_PI * fc / fs;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double cw = std::cos (w0);

        const double a0 = 1.0 + alpha;
        b0 = ((1.0 - cw) * 0.5) / a0;
        b1 = (1.0 - cw) / a0;
        b2 = ((1.0 - cw) * 0.5) / a0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

private:
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
};
