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
        float arpRate       = 0.4f;   // arpeggiator rate / sync division (normalised)
        float arpMode       = 0.0f;   // 0..1 -> up/down/updown/random
        float arpSync       = 0.0f;   // tempo sync enable (>0.5)
        float scaleType     = 0.0f;   // 0..1 -> chromatic/major/minor/pent/dorian
        float filterCutoff  = 1.0f;   // SVF cutoff (0..1 -> ~20 Hz .. ~20 kHz log)
        float filterReso    = 0.12f;  // SVF resonance (0..1 -> Q)
        float filterType    = 0.0f;   // 0..1 -> LP .. BP .. HP
        float filterMod     = 0.0f;   // capillary -> cutoff amount (octaves)
        float drive         = 0.1f;   // analog-style saturation amount
        float unison        = 0.0f;   // 0..1 -> 1..7 stacked detuned grains
        float unisonDetune  = 0.25f;  // unison spread (cents)
        float stereoWidth   = 0.5f;   // 0..1 -> 0 (mono) .. 1 (2x wide)
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
            else if (match (id, "arpMode"))      arpMode.store      (value, rel);
            else if (match (id, "arpSync"))      arpSync.store      (value, rel);
            else if (match (id, "scaleType"))    scaleType.store    (value, rel);
            else if (match (id, "filterCutoff")) filterCutoff.store (value, rel);
            else if (match (id, "filterReso"))   filterReso.store   (value, rel);
            else if (match (id, "filterType"))   filterType.store   (value, rel);
            else if (match (id, "filterMod"))    filterMod.store    (value, rel);
            else if (match (id, "drive"))        drive.store        (value, rel);
            else if (match (id, "unison"))       unison.store       (value, rel);
            else if (match (id, "unisonDetune")) unisonDetune.store (value, rel);
            else if (match (id, "stereoWidth"))  stereoWidth.store  (value, rel);
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
            s.arpMode      = arpMode.load      (acq);
            s.arpSync      = arpSync.load      (acq);
            s.scaleType    = scaleType.load    (acq);
            s.filterCutoff = filterCutoff.load (acq);
            s.filterReso   = filterReso.load   (acq);
            s.filterType   = filterType.load   (acq);
            s.filterMod    = filterMod.load    (acq);
            s.drive        = drive.load        (acq);
            s.unison       = unison.load       (acq);
            s.unisonDetune = unisonDetune.load (acq);
            s.stereoWidth  = stereoWidth.load  (acq);
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
        std::atomic<float> arpMode      { 0.0f };
        std::atomic<float> arpSync      { 0.0f };
        std::atomic<float> scaleType    { 0.0f };
        std::atomic<float> filterCutoff { 1.0f };
        std::atomic<float> filterReso   { 0.12f };
        std::atomic<float> filterType   { 0.0f };
        std::atomic<float> filterMod    { 0.0f };
        std::atomic<float> drive        { 0.1f };
        std::atomic<float> unison       { 0.0f };
        std::atomic<float> unisonDetune { 0.25f };
        std::atomic<float> stereoWidth  { 0.5f };
    };
}
