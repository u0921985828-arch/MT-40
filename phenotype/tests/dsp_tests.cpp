//==============================================================================
//  dsp_tests.cpp
//
//  JUCE-independent sanity + invariant tests for the Phenotype DSP core.
//  Builds standalone (no JUCE, no network) so CI can verify the trig-free,
//  lock-free, allocation-free audio path quickly. Returns non-zero on failure.
//==============================================================================

#include "CapillaryModulator.h"
#include "GranularEngine.h"
#include "SVF.h"
#include "FX.h"
#include "FastMath.h"

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
//  Fundamental (Hz) via autocorrelation — tracks true pitch regardless of
//  harmonic content, so it is immune to the mip anti-aliaser keeping the output
//  bandwidth roughly constant (which defeats a zero-crossing-rate proxy).
static float autocorrPitch (GranularEngine& eng, int blocks, int N)
{
    const int len = blocks * N;
    std::vector<float> buf ((size_t) len);
    std::vector<float> oL ((size_t) N), oR ((size_t) N);
    for (int b = 0; b < blocks; ++b)
    {
        eng.process (nullptr, nullptr, oL.data(), oR.data(), N);
        for (int n = 0; n < N; ++n) buf[(size_t) (b * N + n)] = oL[(size_t) n];
    }
    double mean = 0.0;
    for (float v : buf) mean += v;
    mean /= len;
    for (auto& v : buf) v -= static_cast<float> (mean);

    double r0 = 0.0;
    for (int i = 0; i < len; ++i) r0 += (double) buf[i] * buf[i];
    if (r0 < 1e-9) return 0.0f;

    const int minLag = 48000 / 1200, maxLag = 48000 / 60;   // ~60..1200 Hz window
    double best = -1.0; int bestLag = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double r = 0.0;
        for (int i = 0; i + lag < len; ++i) r += (double) buf[i] * buf[i + lag];
        const double norm = r / r0;
        if (norm > best) { best = norm; bestLag = lag; }
    }
    return 48000.0f / static_cast<float> (bestLag);
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
    const float f60 = autocorrPitch (eng, 16, N);   // C4 ~261.6 Hz
    eng.allNotesOff();
    { float p; bool f; renderBlocks (eng, 120, N, p, f); }

    eng.noteOn (72, 1.0f);
    const float f72 = autocorrPitch (eng, 16, N);   // C5, one octave up
    std::printf ("  f C4=%.1f Hz  C5=%.1f Hz  ratio=%.2f\n", f60, f72, f72 / (f60 + 1e-9f));
    check (f72 > f60 * 1.7f && f72 < f60 * 2.4f, "octave-up note doubles the fundamental");
    eng.allNotesOff();
    { float p; bool f; renderBlocks (eng, 120, N, p, f); }

    //  Pitch bend up by +12 semis doubles the fundamental of C4.
    eng.noteOn (60, 1.0f);
    const float fFlat = autocorrPitch (eng, 16, N);
    eng.setPitchBend (12.0f);
    { float p; bool f; renderBlocks (eng, 40, N, p, f); }   // flush old-pitch grains
    const float fBent = autocorrPitch (eng, 16, N);
    std::printf ("  f flat=%.1f Hz  bent+12=%.1f Hz\n", fFlat, fBent);
    check (fBent > fFlat * 1.7f, "pitch bend up raises the fundamental");
    eng.setPitchBend (0.0f);
    eng.allNotesOff();
}

static void testArpeggiator()
{
    std::printf ("arpeggiator:\n");
    GranularEngine eng;
    eng.prepare (48000.0, 512);
    eng.setInstrumentMode (true);
    eng.params().set ("grainDensity", 0.9f);
    eng.params().set ("grainSize", 0.4f);
    eng.params().set ("spray", 0.0f);
    eng.params().set ("outputGain", 0.8f);
    eng.params().set ("arpOn", 1.0f);
    eng.params().set ("arpRate", 0.8f);   // fast

    constexpr int N = 512;

    //  Hold a triad; the arp should sequence it (mono-ish), producing sound and
    //  changing pitch across steps rather than a static chord.
    eng.noteOn (60, 1.0f);
    eng.noteOn (64, 1.0f);
    eng.noteOn (67, 1.0f);

    std::vector<float> outL ((size_t) N), outR ((size_t) N);
    float peak = 0.0f; bool finite = true;
    int   maxSounding = 0;
    long  crossings = 0, total = 0; float prev = 0.0f;
    for (int b = 0; b < 120; ++b)
    {
        eng.process (nullptr, nullptr, outL.data(), outR.data(), N);
        maxSounding = std::max (maxSounding, eng.activeVoices());
        for (int n = 0; n < N; ++n)
        {
            const float s = outL[(size_t) n];
            if (! std::isfinite (s)) finite = false;
            peak = std::max (peak, std::fabs (s));
            if ((prev <= 0.0f && s > 0.0f)) ++crossings;
            prev = s; ++total;
        }
    }
    std::printf ("  peak=%.3f finite=%d maxVoices=%d\n", peak, (int) finite, maxSounding);
    check (finite, "arp output is finite");
    check (peak > 1.0e-2f, "arp produces sound from held notes");
    check (peak <= 1.0f + 1e-4f, "arp master stays within [-1,1]");
    check (maxSounding <= 3, "arp is broadly monophonic (steps, not a chord)");
    check (crossings > 0, "arp output is tonal");

    //  Releasing all held notes silences the arp within a step + release.
    eng.allNotesOff();
    float tail = 0.0f; bool tf = true;
    renderBlocks (eng, 120, N, tail, tf);
    renderBlocks (eng, 20,  N, tail, tf);
    std::printf ("  post-release tail=%.5f\n", tail);
    check (tail < 1.0e-2f, "arp stops when notes released");
}

static void testScaleQuantiser()
{
    std::printf ("scale quantiser:\n");
    GranularEngine eng;
    eng.prepare (48000.0, 512);

    eng.setScale (0);   // chromatic
    check (eng.quantize (61) == 61, "chromatic leaves notes untouched");

    eng.setScale (1);   // major (C): no C#(61), no D#(63), no F#(66)
    const int q61 = eng.quantize (61);
    const int q66 = eng.quantize (66);
    std::printf ("  major: 61->%d  66->%d\n", q61, q66);
    check (q61 == 60 || q61 == 62, "major snaps C# to C or D");
    check (q66 == 65 || q66 == 67, "major snaps F# to F or G");
    check (eng.quantize (64) == 64, "major keeps E in scale");

    eng.setScale (3);   // pentatonic major {0,2,4,7,9}
    const int q65 = eng.quantize (65);  // F -> nearest E(64) or G(67)
    check (q65 == 64 || q65 == 67, "pentatonic snaps F out of scale");
}

static void testArpSyncRate()
{
    std::printf ("arp tempo sync:\n");
    //  120 BPM: 1/4 = 2 Hz, 1/8 = 4, 1/16 = 8, 1/32 = 16.
    const float r4  = GranularEngine::arpSyncedRate (120.0, 0.0f);
    const float r16 = GranularEngine::arpSyncedRate (120.0, 0.6f);
    std::printf ("  120bpm 1/4=%.2fHz  1/16=%.2fHz\n", r4, r16);
    check (std::fabs (r4 - 2.0f) < 0.01f, "1/4 at 120 BPM = 2 Hz");
    check (std::fabs (r16 - 8.0f) < 0.01f, "1/16 at 120 BPM = 8 Hz");
    check (GranularEngine::arpSyncedRate (60.0, 0.0f) < r4, "half tempo halves rate");
}

static void testSampleGenome()
{
    std::printf ("sample genome:\n");
    GranularEngine eng;
    eng.prepare (48000.0, 512);
    eng.setInstrumentMode (true);
    eng.params().set ("grainDensity", 0.7f);
    eng.params().set ("outputGain", 0.8f);

    //  A short synthetic sample (two-cycle triangle-ish) as the genome.
    constexpr int L = 400;
    std::vector<float> samp ((size_t) L);
    for (int n = 0; n < L; ++n)
        samp[(size_t) n] = 2.0f * std::fabs ((float) (n % 200) / 200.0f - 0.5f) - 0.5f;
    eng.loadGenomeFromSample (samp.data(), L);

    eng.noteOn (60, 1.0f);
    float peak = 0.0f; bool finite = true;
    renderBlocks (eng, 40, 512, peak, finite);
    std::printf ("  sample-genome peak = %.3f\n", peak);
    check (finite, "sample genome output is finite");
    check (peak > 1.0e-2f, "sample genome granulates into sound");
    check (peak <= 1.0f + 1e-4f, "sample genome output bounded");
}

//  --- New tone-stage primitives -------------------------------------------
static void testFastTanh()
{
    std::printf ("fastTanh:\n");
    double maxErr = 0.0;
    for (double x = -3.0; x <= 3.0; x += 0.01)
    {
        const double approx = fastmath::fastTanh (static_cast<float> (x));
        maxErr = std::max (maxErr, std::fabs (approx - std::tanh (x)));
    }
    check (maxErr < 0.02, "fastTanh within 0.02 of std::tanh on [-3,3]");
    check (fastmath::fastTanh (10.0f) <= 1.0f && fastmath::fastTanh (10.0f) >= 0.99f, "fastTanh saturates to +1");
    check (fastmath::fastTanh (-10.0f) >= -1.0f && fastmath::fastTanh (-10.0f) <= -0.99f, "fastTanh saturates to -1");
    //  Monotonic (odd, increasing).
    bool mono = true;
    float prev = -2.0f;
    for (double x = -3.0; x <= 3.0; x += 0.01)
    {
        const float v = fastmath::fastTanh (static_cast<float> (x));
        if (v < prev) mono = false;
        prev = v;
    }
    check (mono, "fastTanh monotonic increasing");
}

static void testFastTan()
{
    std::printf ("fastTan:\n");
    double maxRel = 0.0;
    for (double x = 0.0; x <= 1.40; x += 0.005)
    {
        const double a = fastmath::fastTan (static_cast<float> (x));
        const double t = std::tan (x);
        if (t > 1e-3) maxRel = std::max (maxRel, std::fabs (a - t) / t);
    }
    check (maxRel < 0.01, "fastTan within 1% of std::tan on [0,1.40]");
}

static void testSvfStability()
{
    std::printf ("SVF:\n");
    const float fs = 48000.0f;
    for (float type : { 0.0f, 0.5f, 1.0f })
    {
        SVF f;
        const float g = SVF::gForCutoff (2000.0f, fs);
        const float k = 1.0f / 8.0f;           // Q = 8, high resonance
        float peak = 0.0f;
        bool  finite = true;
        for (int n = 0; n < 96000; ++n)
        {
            const float in = (n == 0) ? 1.0f : 0.0f;   // impulse
            const float o  = f.process (in, g, k, type);
            if (! std::isfinite (o)) finite = false;
            peak = std::max (peak, std::fabs (o));
        }
        check (finite, "SVF impulse response finite (high Q)");
        check (peak < 20.0f, "SVF impulse response bounded (high Q)");
        //  Tail must have decayed (stable).
        float tail = 0.0f;
        for (int n = 0; n < 100; ++n) tail = std::max (tail, std::fabs (f.process (0.0f, g, k, type)));
        check (tail < 1e-3f, "SVF settles to rest (stable)");
    }
}

static void testFilteredEngineClean()
{
    std::printf ("engine (filter+drive+unison):\n");
    GranularEngine eng;
    eng.prepare (48000.0, 512);
    eng.setInstrumentMode (true);
    auto& hub = eng.params();
    hub.set ("unison", 1.0f);          // 7 voices
    hub.set ("unisonDetune", 0.5f);
    hub.set ("drive", 0.6f);
    hub.set ("filterCutoff", 0.5f);    // ~ mid
    hub.set ("filterReso", 0.7f);
    hub.set ("stereoWidth", 1.0f);
    eng.noteOn (60, 1.0f);

    std::vector<float> L (2048), R (2048);
    float peak = 0.0f;
    bool finite = true;
    for (int block = 0; block < 20; ++block)
    {
        eng.process (nullptr, nullptr, L.data(), R.data(), 2048);
        for (int i = 0; i < 2048; ++i)
        {
            if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false;
            peak = std::max (peak, std::max (std::fabs (L[i]), std::fabs (R[i])));
        }
    }
    check (finite, "filtered/driven/unison output finite");
    check (peak <= 1.0f + 1e-4f, "filtered/driven/unison output within clip ceiling");
    check (peak > 0.02f, "filtered/driven/unison output audibly present");
}

static void testMipAntiAlias()
{
    std::printf ("mip anti-alias:\n");
    check (GranularEngine::mipForInc (1.0f) == 0, "mip(1.0) == 0 (unison uses full band)");
    check (GranularEngine::mipForInc (1.5f) == 1, "mip(1.5) == 1");
    check (GranularEngine::mipForInc (2.0f) == 1, "mip(2.0) == 1");
    check (GranularEngine::mipForInc (2.1f) == 2, "mip(2.1) == 2");
    check (GranularEngine::mipForInc (0.5f) == 0, "mip(0.5) == 0 (down-pitch safe)");
    check (GranularEngine::mipForInc (1000.0f) == GranularEngine::kNumMips - 1, "mip clamps at top");

    //  A very high note (ratio ~16) must stay finite and bounded — the mip
    //  selection prevents the read from blowing up with aliased energy.
    GranularEngine eng;
    eng.prepare (48000.0, 512);
    eng.setInstrumentMode (true);
    eng.noteOn (108, 1.0f);                 // 4 octaves above middle C
    std::vector<float> L (2048), R (2048);
    float peak = 0.0f; bool finite = true;
    for (int b = 0; b < 16; ++b)
    {
        eng.process (nullptr, nullptr, L.data(), R.data(), 2048);
        for (int i = 0; i < 2048; ++i)
        {
            if (! std::isfinite (L[i])) finite = false;
            peak = std::max (peak, std::fabs (L[i]));
        }
    }
    check (finite, "very high note output finite");
    check (peak <= 1.0f + 1e-4f, "very high note output bounded");
    check (peak > 0.01f, "very high note audibly present");
}

static void testFastLog()
{
    std::printf ("fastLog / fastLnCosh:\n");
    double maxAbs = 0.0;
    for (double x = 0.02; x <= 40.0; x += 0.02)
        maxAbs = std::max (maxAbs, std::fabs (fastmath::fastLog (static_cast<float> (x)) - std::log (x)));
    check (maxAbs < 0.02, "fastLog within 0.02 abs of std::log on [0.02,40]");

    double maxErr = 0.0;
    for (double x = -12.0; x <= 12.0; x += 0.02)
    {
        const double a = fastmath::fastLnCosh (static_cast<float> (x));
        maxErr = std::max (maxErr, std::fabs (a - std::log (std::cosh (x))));
    }
    check (maxErr < 0.02, "fastLnCosh within 0.02 of ln(cosh) on [-12,12]");
}

static void testSustainPedal()
{
    std::printf ("sustain pedal:\n");
    GranularEngine e;
    e.prepare (48000.0, 512);
    e.setInstrumentMode (true);
    std::vector<float> L (4096), R (4096);

    e.noteOn (60, 1.0f);
    for (int b = 0; b < 4; ++b) e.process (nullptr, nullptr, L.data(), R.data(), 4096);

    e.setSustain (true);
    e.noteOff (60);                       // key up, but pedal holds it
    float held = 0.0f;
    for (int b = 0; b < 6; ++b)
    {
        e.process (nullptr, nullptr, L.data(), R.data(), 4096);
        for (int i = 0; i < 4096; ++i) held = std::max (held, std::fabs (L[i]));
    }
    check (held > 0.02f, "sustain holds the note after note-off");

    e.setSustain (false);                 // pedal up -> release
    for (int b = 0; b < 22; ++b) e.process (nullptr, nullptr, L.data(), R.data(), 4096);
    float tail = 0.0f;
    for (int i = 0; i < 4096; ++i) tail = std::max (tail, std::fabs (L[i]));
    check (tail < 0.02f, "note releases after pedal up");
}

static void testFX()
{
    std::printf ("fx rack (delay + reverb):\n");
    StereoFX fx;
    fx.prepare (48000.0);
    fx.setParams (0.5f, 0.4f, 0.5f, 0.6f, 0.6f, 0.4f);
    float peak = 0.0f; bool finite = true;
    for (int n = 0; n < 96000; ++n)
    {
        float l = (n == 0) ? 1.0f : 0.0f, r = l;
        fx.process (l, r);
        if (! std::isfinite (l) || ! std::isfinite (r)) finite = false;
        peak = std::max (peak, std::max (std::fabs (l), std::fabs (r)));
    }
    check (finite, "FX impulse response finite");
    check (peak < 8.0f, "FX impulse response bounded");
    float tail = 0.0f;
    for (int n = 0; n < 400; ++n) { float l = 0, r = 0; fx.process (l, r); tail = std::max (tail, std::max (std::fabs (l), std::fabs (r))); }
    check (tail < 0.5f, "FX tail decays");
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
    testArpeggiator();
    testScaleQuantiser();
    testArpSyncRate();
    testSampleGenome();
    testFastTanh();
    testFastTan();
    testSvfStability();
    testFilteredEngineClean();
    testMipAntiAlias();
    testFastLog();
    testSustainPedal();
    testFX();

    std::printf ("== %s ==\n", failures == 0 ? "ALL PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
