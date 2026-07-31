#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

/**
    Centralised APVTS parameter identifiers, aligned with the Minimoog Model D
    control layout: Controllers, Oscillator Bank, Mixer, Modifiers (filter +
    filter envelope), Output (loudness envelope). Plus a few "analog character"
    controls (drift / drive / bass-thinning) that are switchable per preset.
*/
namespace ParamID
{
    // ---- Controllers ---------------------------------------------------------
    static constexpr auto masterVolume   = "MASTER_VOLUME";
    static constexpr auto masterTune     = "MASTER_TUNE";      // semitones
    static constexpr auto glideOn        = "GLIDE_ON";
    static constexpr auto glideTime      = "GLIDE_TIME";       // seconds
    static constexpr auto pitchBendRange = "PITCH_BEND_RANGE"; // semitones
    static constexpr auto modWheel       = "MOD_WHEEL";        // 0..1 (also MIDI CC1)
    static constexpr auto modMix         = "MOD_MIX";          // 0 = Osc3, 1 = Noise
    static constexpr auto modOscOn       = "MOD_OSC_ON";       // route mod -> oscillator pitch
    static constexpr auto modFilterOn    = "MOD_FILTER_ON";    // route mod -> filter cutoff
    static constexpr auto polyOn         = "POLY_ON";          // false = mono (authentic), true = polyphonic

    // ---- Oscillator Bank -----------------------------------------------------
    static constexpr auto osc1Wave  = "OSC1_WAVE";
    static constexpr auto osc1Range = "OSC1_RANGE";

    static constexpr auto osc2Wave   = "OSC2_WAVE";
    static constexpr auto osc2Range  = "OSC2_RANGE";
    static constexpr auto osc2Detune = "OSC2_DETUNE";  // semitones (-7..+7)

    static constexpr auto osc3Wave      = "OSC3_WAVE";
    static constexpr auto osc3Range     = "OSC3_RANGE";
    static constexpr auto osc3Detune    = "OSC3_DETUNE";
    static constexpr auto osc3KbControl = "OSC3_KB_CONTROL"; // follow keyboard (off = free/LFO)

    // ---- Mixer ---------------------------------------------------------------
    static constexpr auto mixOsc1Vol  = "MIX_OSC1_VOL";
    static constexpr auto mixOsc2Vol  = "MIX_OSC2_VOL";
    static constexpr auto mixOsc3Vol  = "MIX_OSC3_VOL";
    static constexpr auto mixNoiseVol = "MIX_NOISE_VOL";
    static constexpr auto mixExtVol   = "MIX_EXT_VOL";   // external input / feedback level

    static constexpr auto mixOsc1On  = "MIX_OSC1_ON";
    static constexpr auto mixOsc2On  = "MIX_OSC2_ON";
    static constexpr auto mixOsc3On  = "MIX_OSC3_ON";
    static constexpr auto mixNoiseOn = "MIX_NOISE_ON";
    static constexpr auto mixExtOn   = "MIX_EXT_ON";
    static constexpr auto noiseType  = "NOISE_TYPE";     // 0 = white, 1 = pink

    // ---- Modifiers: Filter ---------------------------------------------------
    static constexpr auto filterCutoff   = "FILTER_CUTOFF";
    static constexpr auto filterReso     = "FILTER_RESO";      // "emphasis"
    static constexpr auto filterEnv      = "FILTER_ENV";       // amount of contour
    static constexpr auto filterKeyTrack = "FILTER_KEYTRACK";  // 0 / 33 / 66 / 100 %

    static constexpr auto filterAttack  = "FILTER_ATTACK";
    static constexpr auto filterDecay   = "FILTER_DECAY";
    static constexpr auto filterSustain = "FILTER_SUSTAIN";
    static constexpr auto filterRelease = "FILTER_RELEASE";

    // ---- Output: Loudness envelope ------------------------------------------
    static constexpr auto ampAttack  = "AMP_ATTACK";
    static constexpr auto ampDecay   = "AMP_DECAY";
    static constexpr auto ampSustain = "AMP_SUSTAIN";
    static constexpr auto ampRelease = "AMP_RELEASE";

    // ---- Analog character (switchable per preset) ---------------------------
    static constexpr auto driftAmount = "DRIFT_AMOUNT"; // oscillator analog drift
    static constexpr auto filterDrive = "FILTER_DRIVE"; // mixer -> filter overdrive (growl)
    static constexpr auto bassThin    = "BASS_THIN";    // resonance bass-thinning

    // ---- Sample / wavetable layer (blends with the oscillators) --------------
    static constexpr auto sampleOn  = "SAMPLE_ON";
    static constexpr auto sampleSel = "SAMPLE_SEL";  // 0..7 built-in timbre
    static constexpr auto sampleVol = "SAMPLE_VOL";

    // ---- Perform: chord + arpeggiator (global performance mode) --------------
    static constexpr auto chordType = "CHORD_TYPE"; // 0=Off..9=Add9
    static constexpr auto arpOn     = "ARP_ON";
    static constexpr auto arpRate   = "ARP_RATE";   // 0=1/4..5=1/32
    static constexpr auto arpMode   = "ARP_MODE";   // Up/Down/Up-Down/Random/As-Played
    static constexpr auto arpOct    = "ARP_OCT";    // 0=1..3=4 octaves
    static constexpr auto arpGate   = "ARP_GATE";   // 0..1
    static constexpr auto arpBpm    = "ARP_BPM";    // 40..240 (used when host tempo absent)
}

namespace ParamChoices
{
    // Six Minimoog waveshapes shared by all three oscillators.
    inline juce::StringArray waveforms()
    {
        return { "Triangle", "Tri-Saw", "Saw", "Square", "Wide Pulse", "Narrow Pulse" };
    }

    inline juce::StringArray ranges()
    {
        return { "LO", "32'", "16'", "8'", "4'", "2'" };
    }

    // Minimoog keyboard-control switches give four tracking amounts.
    inline juce::StringArray keyTracking()
    {
        return { "0%", "33%", "66%", "100%" };
    }

    inline juce::StringArray noiseTypes()
    {
        return { "White", "Pink" };
    }

    // ---- Sample / wavetable timbres -----------------------------------------
    inline juce::StringArray sampleTables()
    {
        return { "Digital", "Organ", "Voice", "Reed", "Bell", "Pluck", "Glass", "Saw2" };
    }

    // ---- Perform ------------------------------------------------------------
    inline juce::StringArray chordTypes()
    {
        return { "Off", "Oct", "5th", "Power", "Maj", "Min", "Sus4", "Maj7", "Min7", "Add9" };
    }

    // Semitone interval sets for each chord type (index matches chordTypes()).
    inline const std::array<std::vector<int>, 10>& chordIntervals()
    {
        static const std::array<std::vector<int>, 10> iv = {{
            {0}, {0,12}, {0,7}, {0,7,12}, {0,4,7}, {0,3,7},
            {0,5,7}, {0,4,7,11}, {0,3,7,10}, {0,4,7,14}
        }};
        return iv;
    }

    inline juce::StringArray arpRates()
    {
        return { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    }

    // Steps per beat for each arp rate.
    inline int arpRateSteps (int i)
    {
        static const int s[6] = { 1, 2, 3, 4, 6, 8 };
        return s[juce::jlimit (0, 5, i)];
    }

    inline juce::StringArray arpModes()
    {
        return { "Up", "Down", "Up-Down", "Random", "As-Played" };
    }

    inline juce::StringArray arpOctaves()
    {
        return { "1", "2", "3", "4" };
    }

    inline float rangeMultiplier (int rangeIndex)
    {
        switch (rangeIndex)
        {
            case 0: return 1.0f / 64.0f; // LO / LFO
            case 1: return 0.25f;        // 32'
            case 2: return 0.5f;         // 16'
            case 3: return 1.0f;         // 8'
            case 4: return 2.0f;         // 4'
            case 5: return 4.0f;         // 2'
            default: return 1.0f;
        }
    }

    inline float keyTrackAmount (int index)
    {
        switch (index)
        {
            case 0: return 0.0f;
            case 1: return 0.33f;
            case 2: return 0.66f;
            case 3: return 1.0f;
            default: return 0.0f;
        }
    }
}
