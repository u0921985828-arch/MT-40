#pragma once

//==============================================================================
//  FX.h
//
//  Post-engine stereo effects rack: a ping-pong delay followed by an algorithmic
//  reverb (classic public-domain comb + allpass topology — 8 parallel damped
//  comb filters into 4 series allpasses per channel). JUCE-independent so it
//  builds and is verified inside the standalone DSP tests. All buffers are
//  pre-allocated in prepare(); process() is allocation-free, lock-free, noexcept.
//==============================================================================

#include <vector>
#include <cstddef>

namespace phenotype::dsp
{
    class StereoFX
    {
    public:
        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            const int maxDelay = static_cast<int> (sr * 1.05);   // up to ~1 s
            dbufL.assign ((size_t) maxDelay, 0.0f);
            dbufR.assign ((size_t) maxDelay, 0.0f);
            maxDelaySamps = maxDelay;

            const double k = sr / 44100.0;
            for (int i = 0; i < kNumCombs; ++i)
            {
                combL[i].init (scale (kComb[i], k));
                combR[i].init (scale (kComb[i] + kStereoSpread, k));
            }
            for (int i = 0; i < kNumAll; ++i)
            {
                allL[i].init (scale (kAll[i], k));
                allR[i].init (scale (kAll[i] + kStereoSpread, k));
            }
            reset();
        }

        void reset() noexcept
        {
            for (auto& v : dbufL) v = 0.0f;
            for (auto& v : dbufR) v = 0.0f;
            writeIdx = 0;
            for (int i = 0; i < kNumCombs; ++i) { combL[i].clear(); combR[i].clear(); }
            for (int i = 0; i < kNumAll; ++i)   { allL[i].clear();  allR[i].clear();  }
        }

        //  Per-block control update (all 0..1 normalised).
        void setParams (float delayMix01, float delayTime01, float delayFb01,
                        float reverbMix01, float reverbSize01, float reverbDamp01) noexcept
        {
            delayMix  = clamp01 (delayMix01);
            delayFb   = clamp01 (delayFb01) * 0.9f;
            int t = static_cast<int> ((0.02f + delayTime01 * 0.73f) * (float) sampleRate); // 20..750 ms
            delaySamps = t < 1 ? 1 : (t >= maxDelaySamps ? maxDelaySamps - 1 : t);

            reverbMix = clamp01 (reverbMix01);
            const float fb = 0.7f + clamp01 (reverbSize01) * 0.28f;   // room size -> comb feedback
            const float d1 = clamp01 (reverbDamp01) * 0.4f;
            for (int i = 0; i < kNumCombs; ++i)
            {
                combL[i].feedback = fb; combR[i].feedback = fb;
                combL[i].damp1 = d1;    combR[i].damp1 = d1;
                combL[i].damp2 = 1.0f - d1; combR[i].damp2 = 1.0f - d1;
            }
        }

        //  Process one stereo sample in place; wet is mixed onto the dry signal.
        void process (float& l, float& r) noexcept
        {
            //  --- Ping-pong delay -------------------------------------------
            int rd = writeIdx - delaySamps;
            if (rd < 0) rd += maxDelaySamps;
            const float dl = dbufL[(size_t) rd];
            const float dr = dbufR[(size_t) rd];
            dbufL[(size_t) writeIdx] = l + dr * delayFb;   // cross feedback = ping-pong
            dbufR[(size_t) writeIdx] = r + dl * delayFb;
            if (++writeIdx >= maxDelaySamps) writeIdx = 0;

            float wl = l + dl * delayMix;
            float wr = r + dr * delayMix;

            //  --- Reverb -----------------------------------------------------
            const float in = (wl + wr) * 0.5f * 0.015f;    // input gain (freeverb)
            float rl = 0.0f, rr = 0.0f;
            for (int i = 0; i < kNumCombs; ++i) { rl += combL[i].process (in); rr += combR[i].process (in); }
            for (int i = 0; i < kNumAll; ++i)   { rl = allL[i].process (rl);   rr = allR[i].process (rr); }

            l = wl + rl * reverbMix * 3.0f;
            r = wr + rr * reverbMix * 3.0f;
        }

    private:
        static float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
        static int   scale (int n, double k) noexcept { int s = static_cast<int> (n * k); return s < 1 ? 1 : s; }

        struct Comb
        {
            std::vector<float> buf; int idx = 0; float store = 0.0f;
            float feedback = 0.8f, damp1 = 0.2f, damp2 = 0.8f;
            void init (int n) { buf.assign ((size_t) n, 0.0f); idx = 0; store = 0.0f; }
            void clear() { for (auto& v : buf) v = 0.0f; idx = 0; store = 0.0f; }
            float process (float input) noexcept
            {
                const float out = buf[(size_t) idx];
                store = out * damp2 + store * damp1;
                buf[(size_t) idx] = input + store * feedback;
                if (++idx >= (int) buf.size()) idx = 0;
                return out;
            }
        };
        struct Allpass
        {
            std::vector<float> buf; int idx = 0;
            void init (int n) { buf.assign ((size_t) n, 0.0f); idx = 0; }
            void clear() { for (auto& v : buf) v = 0.0f; idx = 0; }
            float process (float input) noexcept
            {
                const float bufout = buf[(size_t) idx];
                const float out = -input + bufout;
                buf[(size_t) idx] = input + bufout * 0.5f;
                if (++idx >= (int) buf.size()) idx = 0;
                return out;
            }
        };

        static constexpr int kNumCombs = 8;
        static constexpr int kNumAll   = 4;
        static constexpr int kStereoSpread = 23;
        static constexpr int kComb[kNumCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static constexpr int kAll[kNumAll]     = { 556, 441, 341, 225 };

        double sampleRate = 44100.0;
        std::vector<float> dbufL, dbufR;
        int   maxDelaySamps = 1, writeIdx = 0, delaySamps = 1;
        float delayMix = 0.0f, delayFb = 0.0f;
        float reverbMix = 0.0f;

        Comb    combL[kNumCombs], combR[kNumCombs];
        Allpass allL[kNumAll],    allR[kNumAll];
    };
}
