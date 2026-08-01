#pragma once

//==============================================================================
//  CapillaryModulator.h
//
//  Non-linear modulation source replacing conventional LFO / ADSR. Models a
//  capillary substrate that fills (absorption) until it saturates, then drains
//  under simulated gravity. Two user controls:
//
//      Caudal            -> absorption time constant  (flow rate into substrate)
//      Densidad del Suelo-> drainage time constant     (how fast it filters out)
//
//  Phases (see spec §3):
//      1. Absorption (log):  y(t) = 1 - e^{-t/tauA}     charging toward capacity
//      2. Saturation:        instantaneous state flip at the capacity threshold
//      3. Drainage  (exp):   y(t) = e^{-t/tauD}         free fall to the floor
//
//  Implemented as first-order recursions (the closed-form solutions of the RC
//  charge / discharge equations), so each sample costs one multiply-add. No
//  std::sin, no heap allocation, no locks. Coefficients are recomputed only
//  when a control changes, driven by the lock-free atomic parameter hub.
//==============================================================================

#include "FastMath.h"
#include <atomic>

namespace phenotype::dsp
{
    class CapillaryModulator
    {
    public:
        CapillaryModulator() = default;

        void prepare (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate;
            recomputeCoeffs();
            level = 0.0f;
            phase = Phase::Absorption;
        }

        void reset() noexcept
        {
            level = 0.0f;
            phase = Phase::Absorption;
        }

        //  Controls are normalised 0..1. Mapping is exponential so the knob feel
        //  is musical across the 5 ms .. 5 s range.
        void setCaudal (float caudal01) noexcept
        {
            caudal01 = clamp01 (caudal01);
            if (caudal01 == lastCaudal) return;   // skip per-block re-solve of tau
            lastCaudal = caudal01;
            //  High flow -> short fill time. Invert so 1.0 = fastest.
            tauAbsorb = mapTau (1.0f - caudal01);
            dirty = true;
        }

        void setDensidadSuelo (float density01) noexcept
        {
            density01 = clamp01 (density01);
            if (density01 == lastDensity) return;
            lastDensity = density01;
            //  Dense soil -> slow drainage (long tau). 1.0 = slowest.
            tauDrain = mapTau (density01);
            dirty = true;
        }

        //  Capacity of the substrate before it tips into drainage (0.5 .. 1.0).
        void setSaturation (float saturation01) noexcept
        {
            saturation01 = clamp01 (saturation01);
            if (saturation01 == lastSaturation) return;
            lastSaturation = saturation01;
            capacity = 0.5f + 0.5f * saturation01;
            dirty = true;
        }

        //  Advance one sample. Returns modulation value in [0, 1].
        [[nodiscard]] float processSample() noexcept
        {
            if (dirty)
                recomputeCoeffs();

            switch (phase)
            {
                case Phase::Absorption:
                    //  Charge toward 1.0; the visible band is capped by capacity.
                    level += (1.0f - level) * (1.0f - absorbCoeff);
                    if (level >= capacity)
                    {
                        level = capacity;         // saturation: hard state flip
                        phase = Phase::Drainage;
                    }
                    break;

                case Phase::Drainage:
                    //  Free fall toward 0.
                    level *= drainCoeff;
                    if (level <= floorLevel)
                    {
                        level = floorLevel;
                        phase = Phase::Absorption;
                    }
                    break;
            }

            return level;
        }

        //  Block helper — fills a pre-allocated buffer, no allocation.
        void processBlock (float* dest, int numSamples) noexcept
        {
            for (int n = 0; n < numSamples; ++n)
                dest[n] = processSample();
        }

        [[nodiscard]] float getCurrentLevel() const noexcept { return level; }

    private:
        enum class Phase { Absorption, Drainage };

        static float clamp01 (float v) noexcept
        {
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        }

        //  0..1 -> 0.005 s .. 5 s, exponentially.
        static float mapTau (float x) noexcept
        {
            constexpr float lo = 0.005f, hi = 5.0f;
            return lo * fastmath::fastExp (x * std::log (hi / lo));
        }

        void recomputeCoeffs() noexcept
        {
            absorbCoeff = fastmath::onePoleCoeff (tauAbsorb, sampleRate);
            drainCoeff  = fastmath::onePoleCoeff (tauDrain,  sampleRate);
            dirty = false;
        }

        double sampleRate  = 44100.0;
        float  tauAbsorb   = 0.25f;
        float  tauDrain    = 0.75f;
        float  absorbCoeff = 0.0f;
        float  drainCoeff  = 0.0f;
        float  capacity    = 0.98f;
        float  floorLevel  = 0.001f;
        float  level       = 0.0f;
        bool   dirty       = true;
        Phase  phase       = Phase::Absorption;

        //  Change-detection sentinels (-1 forces the first setter to apply).
        float  lastCaudal     = -1.0f;
        float  lastDensity    = -1.0f;
        float  lastSaturation = -1.0f;
    };
}
