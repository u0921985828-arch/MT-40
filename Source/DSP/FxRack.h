#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
    Master FX rack: Drive -> Chorus -> Ping-pong Delay -> Reverb.
    Each effect has an on/off and an amount; bypassed effects are skipped.
    Mirrors the web app's FX chain.
*/
class FxRack
{
public:
    struct Params
    {
        bool  driveOn = false;  float drive = 0.35f;
        bool  chorusOn = false; float chorus = 0.4f;
        bool  phaserOn = false; float phaser = 0.4f;
        bool  crushOn = false;  float crush = 0.35f;
        bool  toneOn = false;   float tone = 0.5f;
        bool  delayOn = false;  float delayMix = 0.3f, delayTime = 0.28f;
        bool  reverbOn = false; float reverbMix = 0.3f;
    };

    void prepare (double sr, int blockSize, int numChannels)
    {
        sampleRate = sr;
        const int ch = juce::jmax (1, numChannels);
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, blockSize), (juce::uint32) ch };
        chorus.prepare (spec);
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.0f);
        chorus.setRate (0.8f);

        reverb.setSampleRate (sr);

        dmax = (int) (sr * 1.5) + 8;
        for (auto& b : dbuf) b.assign ((size_t) dmax, 0.0f);
        dpos = 0;
        lpL = lpR = 0.0f;
        lpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 3200.0f / (float) sr);
        lfoInc = (float) (0.4 / sr);   // ~0.4 Hz phaser sweep
        reset();
    }

    void reset()
    {
        chorus.reset();
        reverb.reset();
        for (auto& b : dbuf) std::fill (b.begin(), b.end(), 0.0f);
        dpos = 0; lpL = lpR = 0.0f;
        for (auto& c : apZ) for (auto& z : c) z = 0.0f;
        toneZ[0] = toneZ[1] = 0.0f; lfoPhase = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int n  = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        if (ch < 1 || n < 1) return;

        // ---- Drive (soft tanh, wet/dry) ------------------------------------
        if (p.driveOn)
        {
            const float k = 1.0f + p.drive * 8.0f;
            const float norm = 1.0f / std::tanh (k);
            for (int c = 0; c < ch; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = d[i] * (1.0f - p.drive) + std::tanh (d[i] * k) * norm * p.drive;
            }
        }

        // ---- Chorus (JUCE dsp) ---------------------------------------------
        if (p.chorusOn)
        {
            chorus.setDepth (juce::jlimit (0.0f, 1.0f, 0.15f + p.chorus * 0.6f));
            chorus.setMix   (juce::jlimit (0.0f, 1.0f, p.chorus));
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            chorus.process (ctx);
        }

        // ---- Phaser (4 modulated allpass stages, wet/dry) ------------------
        if (p.phaserOn)
        {
            const float depth = p.phaser;
            for (int i = 0; i < n; ++i)
            {
                lfoPhase += lfoInc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                const float a = 0.4f + 0.45f * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * lfoPhase));
                for (int c = 0; c < ch; ++c)
                {
                    float* d = buffer.getWritePointer (c);
                    float x = d[i], y = x;
                    for (int s = 0; s < 4; ++s)
                    {
                        const float in = y;
                        y = -a * in + apZ[c][s];
                        apZ[c][s] = in + a * y;
                    }
                    d[i] = x + (y - x) * depth;
                }
            }
        }

        // ---- Bitcrush (bit-depth quantise) ---------------------------------
        if (p.crushOn)
        {
            const float q = std::pow (2.0f, juce::jmax (1.0f, 8.0f - p.crush * 6.0f));
            for (int c = 0; c < ch; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i) d[i] = std::round (d[i] * q) / q;
            }
        }

        // ---- Tone (one-pole low-pass tilt) ---------------------------------
        if (p.toneOn)
        {
            const float fc = 400.0f * std::pow (2.0f, p.tone * 5.5f);
            const float a  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * juce::jmin ((float) sampleRate * 0.49f, fc) / (float) sampleRate);
            for (int c = 0; c < ch; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i) { toneZ[c] += a * (d[i] - toneZ[c]); d[i] = toneZ[c]; }
            }
        }

        // ---- Ping-pong delay (manual stereo, low-passed feedback) ----------
        if (p.delayOn && ch >= 1)
        {
            const int dSamp = juce::jlimit (1, dmax - 1, (int) (p.delayTime * (float) sampleRate));
            const float fb  = 0.15f + p.delayMix * 0.5f;
            float* L = buffer.getWritePointer (0);
            float* R = ch > 1 ? buffer.getWritePointer (1) : nullptr;
            for (int i = 0; i < n; ++i)
            {
                const int rp = (dpos - dSamp + dmax) % dmax;
                float dl = dbuf[0][(size_t) rp];
                float dr = dbuf[1][(size_t) rp];
                lpL += lpCoef * (dl - lpL); dl = lpL;
                lpR += lpCoef * (dr - lpR); dr = lpR;

                const float inL = L[i];
                const float inR = R ? R[i] : L[i];
                dbuf[0][(size_t) dpos] = inL + dr * fb;   // cross-feed -> bounce
                dbuf[1][(size_t) dpos] = inR + dl * fb;
                dpos = (dpos + 1) % dmax;

                L[i] = inL + dl * p.delayMix;
                if (R) R[i] = inR + dr * p.delayMix;
            }
        }

        // ---- Reverb (JUCE freeverb) ----------------------------------------
        if (p.reverbOn)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.82f;
            rp.damping  = 0.35f;
            rp.wetLevel = p.reverbMix * 0.9f;
            rp.dryLevel = 1.0f;
            rp.width    = 1.0f;
            reverb.setParameters (rp);
            if (ch > 1) reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
            else        reverb.processMono   (buffer.getWritePointer (0), n);
        }
    }

private:
    double sampleRate { 44100.0 };
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;

    std::vector<float> dbuf[2];
    int dmax { 0 }, dpos { 0 };
    float lpL { 0.0f }, lpR { 0.0f }, lpCoef { 0.5f };

    float apZ[2][4] { { 0 }, { 0 } };   // phaser allpass state (per channel)
    float toneZ[2] { 0.0f, 0.0f };      // tone one-pole state
    float lfoPhase { 0.0f }, lfoInc { 0.0f };
};
