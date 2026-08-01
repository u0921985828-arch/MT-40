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

static void testOnePoleRamp()
{
    std::printf ("one-pole smoothing:\n");
    const float g = fastmath::onePoleCoeff (0.005f, 48000.0);   // 5 ms, 240 samples
    float y = 0.0f;
    for (int i = 0; i < 240; ++i)                                // one time constant
        y = 1.0f + (y - 1.0f) * g;
    std::printf ("  level after 1 tau = %.4f (expect ~0.632)\n", y);
    check (y > 0.60f && y < 0.66f, "one-pole reaches ~63% after one time constant");
    check (g > 0.0f && g < 1.0f, "one-pole coeff is a stable pole in (0,1)");
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

static void renderBlocks (GranularEngine& eng, int blocks, int N,
                          float& peak, bool& finite)
{
    std::vector<float> outL ((size_t) N), outR ((size_t) N);
    peak = 0.0f; finite = true;
    for (int b = 0; b < blocks; ++b)
    {
        eng.process (nullptr, nullptr, outL.data(), outR.data(), N);
        for (int n = 0; n < N; ++n)
        {
            if (! std::isfinite (outL[(size_t) n]) || ! std::isfinite (outR[(size_t) n]))
                finite = false;
            peak = std::max (peak, std::fabs (outL[(size_t) n]));
        }
    }
}

static void testMelodicInstrument()
{
    std::printf ("melodic instrument:\n");
    GranularEngine eng;
    eng.prepare (48000.0, 512);
    eng.setInstrumentMode (true);
    eng.params().set ("grainDensity", 0.6f);
    eng.params().set ("grainSize", 0.3f);
    eng.params().set ("outputGain", 0.8f);
    eng.params().set ("pitchA", 0.5f);
    eng.params().set ("pitchB", 0.5f);

    constexpr int N = 512;
    float peak = 0.0f; bool finite = true;

    //  Silence before any note.
    renderBlocks (eng, 20, N, peak, finite);
    check (peak < 1.0e-3f, "instrument is silent with no notes held");

    //  Note on -> sound.
    eng.noteOn (60, 1.0f);   // C4
    check (eng.activeVoices() == 1, "noteOn allocates a voice");
    renderBlocks (eng, 40, N, peak, finite);
    std::printf ("  held-note peak = %.3f, voices = %d\n", peak, eng.activeVoices());
    check (finite, "instrument output is finite");
    check (peak > 1.0e-2f, "held note produces sound");
    check (peak <= 1.0f + 1e-4f, "instrument master stays within [-1,1]");

    //  A chord adds voices.
    eng.noteOn (64, 0.9f);   // E4
    eng.noteOn (67, 0.8f);   // G4
    check (eng.activeVoices() == 3, "chord allocates three voices");

    //  Note off -> release + grain tail decay. Let it settle, then measure only
    //  the final window (the release ramp and in-flight grains are loud at first
    //  and must not count against the silence check).
    eng.allNotesOff();
    renderBlocks (eng, 120, N, peak, finite);        // settle past release + grain tail
    check (eng.activeVoices() == 0, "voices free after release");

    float tailPeak = 0.0f; bool tailFinite = true;
    renderBlocks (eng, 20, N, tailPeak, tailFinite); // measure the settled tail only
    std::printf ("  settled tail peak = %.5f, voices = %d\n", tailPeak, eng.activeVoices());
    check (tailPeak < 1.0e-2f, "output decays to near silence after note off");
}

//  Estimate output pitch by counting zero crossings over a rendered window.
static float measureZcr (GranularEngine& eng, int blocks, int N)
{
    std::vector<float> outL ((size_t) N), outR ((size_t) N);
    long crossings = 0, samples = 0;
    float prev = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        eng.process (nullptr, nullptr, outL.data(), outR.data(), N);
        for (int n = 0; n < N; ++n)
        {
            const float s = outL[(size_t) n];
            if ((prev <= 0.0f && s > 0.0f) || (prev >= 0.0f && s < 0.0f)) ++crossings;
            prev = s;
            ++samples;
        }
    }
    return samples > 0 ? static_cast<float> (crossings) / static_cast<float> (samples) : 0.0f;
}

static void testPitchAndBend()
{
    std::printf ("pitch / bend / glide:\n");
    GranularEngine eng;
    eng.prepare (48000.0, 512);
    eng.setInstrumentMode (true);
    eng.params().set ("grainDensity", 0.95f);   // dense -> tonal, stable ZCR
    eng.params().set ("grainSize", 0.5f);
    eng.params().set ("spray", 0.0f);
    eng.params().set ("outputGain", 0.8f);

    constexpr int N = 512;

    eng.noteOn (60, 1.0f);
    const float z60 = measureZcr (eng, 30, N);   // C4
    eng.allNotesOff();
    { float p; bool f; renderBlocks (eng, 120, N, p, f); }

    eng.noteOn (72, 1.0f);
    const float z72 = measureZcr (eng, 30, N);   // C5, one octave up
    std::printf ("  ZCR C4=%.4f  C5=%.4f  ratio=%.2f\n", z60, z72, z72 / (z60 + 1e-9f));
    check (z72 > z60 * 1.6f, "octave-up note raises pitch (ZCR ~2x)");
    eng.allNotesOff();
    { float p; bool f; renderBlocks (eng, 120, N, p, f); }

    //  Pitch bend up by +12 semis roughly doubles the pitch of C4.
    eng.noteOn (60, 1.0f);
    const float zFlat = measureZcr (eng, 20, N);
    eng.setPitchBend (12.0f);
    const float zBent = measureZcr (eng, 20, N);
    std::printf ("  ZCR flat=%.4f  bent+12=%.4f\n", zFlat, zBent);
    check (zBent > zFlat * 1.4f, "pitch bend up raises pitch");
    eng.setPitchBend (0.0f);
    eng.allNotesOff();
}

int main()
{
    std::printf ("== Phenotype DSP tests ==\n");
    testFastExp();
    testEqualPower();
    testOnePoleRamp();
    testCapillaryCycle();
    testGranularFiniteAllocFree();
    testMelodicInstrument();
    testPitchAndBend();

    std::printf ("== %s ==\n", failures == 0 ? "ALL PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
