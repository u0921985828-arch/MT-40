#pragma once

#include <juce_core/juce_core.h>
#include <array>

/**
    Centralised definition of every APVTS parameter identifier used across the
    plugin.  Keeping the IDs in one place avoids stringly-typed mismatches
    between the DSP (which reads the values) and the UI (which attaches to them).

    The layout itself is built in PluginProcessor::createParameterLayout().
*/
namespace ParamID
{
    // ---- Global / Controllers ------------------------------------------------
    static constexpr auto masterVolume   = "MASTER_VOLUME";
    static constexpr auto masterTune     = "MASTER_TUNE";     // Osc 1 master pitch, in semitones
    static constexpr auto glideOn        = "GLIDE_ON";
    static constexpr auto glideTime      = "GLIDE_TIME";      // seconds
    static constexpr auto pitchBendRange = "PITCH_BEND_RANGE"; // semitones

    // ---- Oscillator 1 --------------------------------------------------------
    static constexpr auto osc1Wave  = "OSC1_WAVE";
    static constexpr auto osc1Range = "OSC1_RANGE";

    // ---- Oscillator 2 --------------------------------------------------------
    static constexpr auto osc2Wave   = "OSC2_WAVE";
    static constexpr auto osc2Range  = "OSC2_RANGE";
    static constexpr auto osc2Detune = "OSC2_DETUNE";  // fine tune, semitones (-7..+7)
    static constexpr auto osc2Coarse = "OSC2_COARSE";  // coarse tune, semitones (-24..+24)

    // ---- Oscillator 3 --------------------------------------------------------
    static constexpr auto osc3Wave    = "OSC3_WAVE";
    static constexpr auto osc3Range   = "OSC3_RANGE";
    static constexpr auto osc3Detune  = "OSC3_DETUNE";
    static constexpr auto osc3Coarse  = "OSC3_COARSE";
    static constexpr auto osc3LfoMode = "OSC3_LFO_MODE";   // Osc 3 acts as a modulation LFO
    static constexpr auto osc3ModDepth = "OSC3_MOD_DEPTH"; // amount routed to filter/pitch

    // ---- Mixer ---------------------------------------------------------------
    static constexpr auto mixOsc1Vol  = "MIX_OSC1_VOL";
    static constexpr auto mixOsc2Vol  = "MIX_OSC2_VOL";
    static constexpr auto mixOsc3Vol  = "MIX_OSC3_VOL";
    static constexpr auto mixNoiseVol = "MIX_NOISE_VOL";
    static constexpr auto mixFeedback = "MIX_FEEDBACK";   // feedback drive / overdrive

    static constexpr auto mixOsc1On     = "MIX_OSC1_ON";
    static constexpr auto mixOsc2On     = "MIX_OSC2_ON";
    static constexpr auto mixOsc3On     = "MIX_OSC3_ON";
    static constexpr auto mixNoiseOn    = "MIX_NOISE_ON";
    static constexpr auto mixFeedbackOn = "MIX_FEEDBACK_ON";
    static constexpr auto noiseType     = "NOISE_TYPE";   // 0 = white, 1 = pink

    // ---- Filter --------------------------------------------------------------
    static constexpr auto filterCutoff   = "FILTER_CUTOFF";
    static constexpr auto filterReso     = "FILTER_RESO";
    static constexpr auto filterEnv      = "FILTER_ENV";       // contour amount
    static constexpr auto filterKeyTrack = "FILTER_KEYTRACK";  // 0%, 50%, 100%

    // ---- Filter Envelope (ENV 1) --------------------------------------------
    static constexpr auto filterAttack  = "FILTER_ATTACK";
    static constexpr auto filterDecay   = "FILTER_DECAY";
    static constexpr auto filterSustain = "FILTER_SUSTAIN";
    static constexpr auto filterRelease = "FILTER_RELEASE";

    // ---- Amp / Loudness Envelope (ENV 2) ------------------------------------
    static constexpr auto ampAttack  = "AMP_ATTACK";
    static constexpr auto ampDecay    = "AMP_DECAY";
    static constexpr auto ampSustain = "AMP_SUSTAIN";
    static constexpr auto ampRelease = "AMP_RELEASE";
}

namespace ParamChoices
{
    // Waveform selector shared by all three oscillators.
    inline juce::StringArray waveforms()
    {
        return { "Triangle", "Saw", "Square", "Wide Pulse", "Narrow Pulse" };
    }

    // Foot ranges for the oscillator bank.  LO is only meaningful for Osc 3
    // (used as a sub-audio LFO) but is exposed everywhere for consistency.
    inline juce::StringArray ranges()
    {
        return { "LO", "32'", "16'", "8'", "4'", "2'" };
    }

    // Keyboard tracking amounts for the ladder filter.
    inline juce::StringArray keyTracking()
    {
        return { "0%", "50%", "100%" };
    }

    inline juce::StringArray noiseTypes()
    {
        return { "White", "Pink" };
    }

    // Maps a range choice index to a frequency multiplier relative to 8'.
    // LO drops several octaves so Osc 3 can act as an LFO.
    inline float rangeMultiplier (int rangeIndex)
    {
        switch (rangeIndex)
        {
            case 0: return 1.0f / 64.0f; // LO / LFO
            case 1: return 0.25f;        // 32'
            case 2: return 0.5f;         // 16'
            case 3: return 1.0f;         // 8'  (reference)
            case 4: return 2.0f;         // 4'
            case 5: return 4.0f;         // 2'
            default: return 1.0f;
        }
    }
}
