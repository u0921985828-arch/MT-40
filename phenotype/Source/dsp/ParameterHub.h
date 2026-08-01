#pragma once

//==============================================================================
//  ParameterHub.h
//
//  The single lock-free bridge between the UI thread (WebView / IPC) and the
//  audio thread. Every control the DSP reads is a std::atomic<float> with
//  relaxed ordering — writers (UI) and the single reader (audio) never block,
//  never allocate. The audio thread snapshots the hub once per block into a
//  plain struct so the inner sample loop touches no atomics.
//==============================================================================

#include <atomic>

namespace phenotype::dsp
{
    //  Plain snapshot consumed inside processBlock (no atomics in the hot loop).
    struct ParameterSnapshot
    {
        float caudal        = 0.5f;   // capillary absorption rate
        float soilDensity   = 0.5f;   // capillary drainage rate
        float saturation    = 0.9f;   // capillary capacity threshold
        float grainDensity  = 0.4f;   // grains per second (normalised)
        float grainSize     = 0.3f;   // grain length (normalised)
        float position      = 0.5f;   // scrub position into source buffers
        float spray         = 0.2f;   // random position jitter
        float pitchA        = 0.5f;   // chromosome-A transpose
        float pitchB        = 0.5f;   // chromosome-B transpose
        float crossBlend    = 0.5f;   // A/B genotype mix
        float modDepth      = 0.5f;   // capillary -> position/blend depth
        float outputGain    = 0.8f;
        float arpOn         = 0.0f;   // arpeggiator enable (>0.5)
        float arpRate       = 0.4f;   // arpeggiator rate (normalised)
    };

    class ParameterHub
    {
    public:
        ParameterHub() = default;

        //  --- UI thread writers ------------------------------------------------
        void set (const char* id, float value) noexcept
        {
            //  Simple id dispatch. Kept branchy-but-flat; called at UI rate only.
            if      (match (id, "caudal"))       caudal.store       (value, rel);
            else if (match (id, "soilDensity"))  soilDensity.store  (value, rel);
            else if (match (id, "saturation"))   saturation.store   (value, rel);
            else if (match (id, "grainDensity")) grainDensity.store (value, rel);
            else if (match (id, "grainSize"))    grainSize.store    (value, rel);
            else if (match (id, "position"))     position.store     (value, rel);
            else if (match (id, "spray"))        spray.store        (value, rel);
            else if (match (id, "pitchA"))       pitchA.store       (value, rel);
            else if (match (id, "pitchB"))       pitchB.store       (value, rel);
            else if (match (id, "crossBlend"))   crossBlend.store   (value, rel);
            else if (match (id, "modDepth"))     modDepth.store     (value, rel);
            else if (match (id, "outputGain"))   outputGain.store   (value, rel);
            else if (match (id, "arpOn"))        arpOn.store        (value, rel);
            else if (match (id, "arpRate"))      arpRate.store      (value, rel);
        }

        //  --- audio thread reader ---------------------------------------------
        [[nodiscard]] ParameterSnapshot snapshot() const noexcept
        {
            ParameterSnapshot s;
            s.caudal       = caudal.load       (acq);
            s.soilDensity  = soilDensity.load  (acq);
            s.saturation   = saturation.load   (acq);
            s.grainDensity = grainDensity.load (acq);
            s.grainSize    = grainSize.load    (acq);
            s.position     = position.load     (acq);
            s.spray        = spray.load        (acq);
            s.pitchA       = pitchA.load       (acq);
            s.pitchB       = pitchB.load       (acq);
            s.crossBlend   = crossBlend.load   (acq);
            s.modDepth     = modDepth.load     (acq);
            s.outputGain   = outputGain.load   (acq);
            s.arpOn        = arpOn.load        (acq);
            s.arpRate      = arpRate.load      (acq);
            return s;
        }

    private:
        static constexpr std::memory_order rel = std::memory_order_relaxed;
        static constexpr std::memory_order acq = std::memory_order_relaxed;

        static bool match (const char* a, const char* b) noexcept
        {
            while (*a && *b) { if (*a++ != *b++) return false; }
            return *a == *b;
        }

        std::atomic<float> caudal       { 0.5f };
        std::atomic<float> soilDensity  { 0.5f };
        std::atomic<float> saturation   { 0.9f };
        std::atomic<float> grainDensity { 0.4f };
        std::atomic<float> grainSize    { 0.3f };
        std::atomic<float> position     { 0.5f };
        std::atomic<float> spray        { 0.2f };
        std::atomic<float> pitchA       { 0.5f };
        std::atomic<float> pitchB       { 0.5f };
        std::atomic<float> crossBlend   { 0.5f };
        std::atomic<float> modDepth     { 0.5f };
        std::atomic<float> outputGain   { 0.8f };
        std::atomic<float> arpOn        { 0.0f };
        std::atomic<float> arpRate      { 0.4f };
    };
}
