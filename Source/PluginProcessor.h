#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/VoiceAllocator.h"
#include "dsp/RhythmEngine.h"
#include "dsp/AutoChord.h"
#include "dsp/Presets.h"
#include "dsp/DCBlocker.h"
#include <vector>

// ---------------------------------------------------------------------------
// ST40AudioProcessor — top-level plugin (§1-§7).
//
// Wires the APVTS parameters to the DSP engine, handles MIDI (including the
// split-keyboard / auto-chord logic and the CC map from §6), runs the global
// vibrato LFO, and mixes the melodic pool with the analog rhythm bus.
// ---------------------------------------------------------------------------
class ST40AudioProcessor : public juce::AudioProcessor
{
public:
    ST40AudioProcessor();
    ~ST40AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Artifacts ST-40"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Split-keyboard boundaries (§2). Lowest key = C2 (MIDI 36).
    static constexpr int kLowestKey = 36;
    static constexpr int kSplitKeyIndex = 15;                 // keys 0-14 vs 15-36
    static constexpr int kSplitNote = kLowestKey + kSplitKeyIndex; // MIDI 51

    RhythmEngine::Transport getTransportState() const noexcept { return rhythm.getTransport(); }

    // --- UI (WebView) -> engine bridges (thread-safe) --------------------
    // Called from the message thread by the on-screen keyboard / panel.
    void injectNoteOn (int note, float velocity);
    void injectNoteOff (int note);
    void uiPressSynchro() noexcept { synchroPending.store (true); }
    void uiSetFillHeld (bool held) noexcept { fillHeldUI.store (held); }
    void uiStartStop() noexcept { startStopPending.store (true); }

private:
    void handleMidiMessage (const juce::MidiMessage& m);
    void handleNoteOn (int note, float velocity);
    void handleNoteOff (int note);
    void handleControlChange (int cc, int value);
    void updateFromParameters();
    void recomputeChord();
    float renderNextSample();

    bool isChordZone (int note) const noexcept
    {
        return note < kSplitNote;
    }

    VoiceAllocator melodic;      // right-hand / melody voices
    VoiceAllocator chordVoices;  // auto-chord accompaniment (kept separate so
                                 // releasing a chord never steals a melody note)
    RhythmEngine rhythm;

    // On-screen keyboard notes are queued here and merged into the block's
    // MIDI stream on the audio thread (§ WebView keyboard).
    juce::MidiMessageCollector keyboardCollector;
    std::atomic<bool> synchroPending  { false };
    std::atomic<bool> fillHeldUI      { false };
    std::atomic<bool> startStopPending { false };

    // auto-chord accompaniment state.
    std::vector<int> heldChordZoneNotes;
    std::vector<int> activeChordTones;

    double sampleRate = 44100.0;
    double lfoPhase = 0.0;
    int currentPatch = -1;
    float masterGainS = 0.85f;   // smoothed master gain (anti-zipper)
    DCBlocker dcBlocker;         // master DC removal

    // Cached parameter pointers.
    std::atomic<float>* pMaster = nullptr;
    std::atomic<float>* pRhythm = nullptr;
    std::atomic<float>* pBass   = nullptr;
    std::atomic<float>* pTempo  = nullptr;
    std::atomic<float>* pVibrato = nullptr;
    std::atomic<float>* pSustain = nullptr;
    std::atomic<float>* pMode    = nullptr;
    std::atomic<float>* pRhythmIdx = nullptr;
    std::atomic<float>* pPatch   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ST40AudioProcessor)
};
