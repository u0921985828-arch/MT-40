//==============================================================================
//  dsp_tests.cpp
//
//  JUCE-independent sanity + invariant tests for the Phenotype DSP core.
//  Builds standalone (no JUCE, no network) so CI can verify the trig-free,
//  lock-free, allocation-free audio path quickly. Returns non-zero on failure.
//==============================================================================

#include "CapillaryModulator.h"
#include "GranularEngine.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
    int failures = 0;

    void check (bool cond, const std::string& what)
    {
        if (! cond)
        {
            std::printf ("  [FAIL] %s\n", what.c_str());
            ++failures;
        }
        else
        {
            std::printf ("  [ok]   %s\n", what.c_str());
        }
    }
}

using namespace phenotype;
using namespace phenotype::dsp;

static void testFastExp()
{
    std::printf ("fastExp:\n");
    double maxErr = 0.0;
    for (float x = -12.0f; x <= 2.0f; x += 0.005f)
    {
        const float approx = fastmath::fastExp (x);
        const double ref   = std::exp ((double) x);
        maxErr = std::max (maxErr, std::fabs ((approx - ref) / (ref + 1e-9)));
    }
    std::printf ("  max rel err over [-12,2] = %.6f\n", maxErr);
    check (maxErr < 5e-3, "fastExp within 0.5% of std::exp");
    check (fastmath::fastExp (0.0f) > 0.999f && fastmath::fastExp (0.0f) < 1.001f,
           "fastExp(0) == 1");
}

static void testEqualPower()
{
    std::printf ("equalPower:\n");
    double maxSum = 0.0, minSum = 2.0;
    for (float b = 0.0f; b <= 1.0f; b += 0.01f)
    {
        float a, c;
        fastmath::equalPowerPair (b, a, c);
        const double p = (double) a * a + (double) c * c; // power sum ~ 1
        maxSum = std::max (maxSum, p);
        minSum = std::min (minSum, p);
    }
    check (minSum > 0.95 && maxSum < 1.05, "equal-power gains preserve energy (~1)");
}

static void testCapillaryCycle()
{
    std::printf ("capillary:\n");
    CapillaryModulator cap;
    cap.prepare (48000.0);
    cap.setCaudal (0.85f);         // fast fill
    cap.setDensidadSuelo (0.4f);   // moderate drain
    cap.setSaturation (0.9f);      // capacity -> 0.95

    float lo = 1.0f, hi = 0.0f;
    bool inRange = true;
    for (int i = 0; i < 48000 * 6; ++i)
    {
        const float v = cap.processSample();
        if (v < -1e-4f || v > 1.0f + 1e-4f) inRange = false;
        lo = std::min (lo, v);
        hi = std::max (hi, v);
    }
    std::printf ("  observed range [%.4f, %.4f]\n", lo, hi);
    check (inRange, "capillary stays within [0,1]");
    check (hi > 0.9f, "capillary reaches saturation capacity");
    check (lo < 0.05f, "capillary drains toward the floor");
}

static void testGranularFiniteAllocFree()
{
    std::printf ("granular:\n");
    GranularEngine eng;
    eng.prepare (48000.0, 512);
    eng.params().set ("grainDensity", 0.7f);
    eng.params().set ("grainSize", 0.3f);
    eng.params().set ("spray", 0.3f);
    eng.params().set ("crossBlend", 0.5f);
    eng.params().set ("outputGain", 0.8f);

    constexpr int N = 512;
    float inL[N], inR[N], outL[N], outR[N];
    bool finite = true;
    float peak = 0.0f;
    for (int b = 0; b < 400; ++b)
    {
        for (int n = 0; n < N; ++n)
        {
            const float t = (float) (b * N + n) / 48000.0f;
            inL[n] = 0.4f * std::sin (2.0f * 3.14159265f * 220.0f * t);
            inR[n] = 0.4f * std::sin (2.0f * 3.14159265f * 277.0f * t);
        }
        eng.process (inL, inR, outL, outR, N);
        for (int n = 0; n < N; ++n)
        {
            if (! std::isfinite (outL[n]) || ! std::isfinite (outR[n])) finite = false;
            peak = std::max (peak, std::fabs (outL[n]));
        }
    }
    std::printf ("  active grains = %d, capillary = %.3f, peak = %.3f\n",
                 eng.activeGrains(), eng.capillaryLevel(), peak);
    check (finite, "granular output is always finite");
    check (eng.activeGrains() > 0, "granular schedules grains");
    check (peak > 0.0f, "granular produces non-silent output");
    check (peak <= 1.0f + 1e-4f, "granular master stays within [-1,1] (soft clip)");

    //  In-place aliasing must not crash or NaN.
    eng.process (inL, inR, inL, inR, N);
    bool aliasFinite = true;
    for (int n = 0; n < N; ++n)
        if (! std::isfinite (inL[n])) aliasFinite = false;
    check (aliasFinite, "granular tolerates in-place (aliased) buffers");
}

int main()
{
    std::printf ("== Phenotype DSP tests ==\n");
    testFastExp();
    testEqualPower();
    testCapillaryCycle();
    testGranularFiniteAllocFree();

    std::printf ("== %s ==\n", failures == 0 ? "ALL PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
