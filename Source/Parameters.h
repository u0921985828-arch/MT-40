#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// Parameters — APVTS layout for the Artifacts ST-40 emulator (§6, §7 task 1).
//
// Parameter IDs, ranges and default values mirror the proposed VST MIDI CC
// mapping table. The MIDI CC -> parameter routing (CC7, CC12, CC13, CC1,
// CC64, CC85, CC86 + Program Change) is applied in the processor.
// ---------------------------------------------------------------------------
namespace ParamIDs
{
    constexpr auto masterVolume = "master_vca_gain";   // CC7
    constexpr auto rhythmVolume = "rhythm_bus_gain";   // CC12
    constexpr auto bassVolume   = "bass_osc_gain";     // CC13
    constexpr auto tempo        = "host_bpm";          // Tempo
    constexpr auto vibrato      = "global_lfo_on";     // CC1
    constexpr auto sustain      = "env_release_mult";  // CC64
    constexpr auto kbdMode      = "kbd_mode";          // CC85
    constexpr auto rhythmIdx    = "active_rhythm_idx"; // CC86
    constexpr auto patchIdx     = "active_patch_idx";  // Program Change
}

// Suggested MIDI CC numbers (§6).
namespace MidiCC
{
    constexpr int vibrato      = 1;
    constexpr int masterVolume = 7;
    constexpr int rhythmVolume = 12;
    constexpr int bassVolume   = 13;
    constexpr int sustain      = 64;
    constexpr int kbdMode      = 85;
    constexpr int rhythmSelect = 86;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
