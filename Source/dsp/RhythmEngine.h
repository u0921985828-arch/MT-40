#pragma once

#include "TwinTResonator.h"
#include "SnareDrum.h"
#include "HiHat.h"
#include "SlengTengBass.h"
#include "LFSRNoise.h"
#include <array>

// ---------------------------------------------------------------------------
// RhythmEngine — analog rhythm section sequencer + Sleng Teng bass line, with
// the Synchro / Fill-in state machine (§4, §5).
//
//   * 6 rhythm patterns (§6, active_rhythm_idx 0-5). Index 0 is the "Sleng
//     Teng" Rock rhythm — a hardcoded 2-bar / 16-step-per-bar loop (§5.1).
//   * Free-running clock at the current BPM (default ~90), 1/16-note grid.
//   * Transport state machine: STOPPED -> ARMED -> PLAYING (§5.2).
//   * Fill-in logic: hold = 1/4 kicks; double-tap & hold = 1/8 snares (§5.2).
//
// The dedicated monophonic bass voice (§2) plays the Sleng Teng bass line,
// transposed to follow the current auto-chord root.
// ---------------------------------------------------------------------------
class RhythmEngine
{
public:
    static constexpr int kStepsPerBar = 16;
    static constexpr int kBars        = 2;
    static constexpr int kTotalSteps  = kStepsPerBar * kBars; // 32
    static constexpr int kNumRhythms  = 6;
    static constexpr int kRest        = -128;

    enum class Transport { Stopped, Armed, Playing };

    RhythmEngine() : snare (noise), hat (noise) {}

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setTempo (double bpm) noexcept;
    void setRhythm (int idx) noexcept { activeRhythm = juceClampRhythm (idx); }
    void setRhythmGain (float g) noexcept { rhythmGain = g; }
    void setBassGain (float g) noexcept { bassGain = g; }

    // The bass follows this root (MIDI note). -1 = no chord held.
    void setChordRoot (int midiNote) noexcept { chordRoot = midiNote; }

    // --- Transport / state machine inputs (§5.2) -------------------------
    void pressSynchro() noexcept;                 // STOPPED -> ARMED
    void noteOnInSplitZone() noexcept;            // STOPPED/ARMED -> PLAYING
    void start() noexcept;                        // force PLAYING from the top
    void stop() noexcept;                         // -> STOPPED (tails ring out)
    void allSplitNotesReleased() noexcept;        // may return to STOPPED
    void setFillHeld (bool held) noexcept;        // Fill-in button

    Transport getTransport() const noexcept { return transport; }

    // Render one sample of the full rhythm+bass bus (pre master gain).
    float process() noexcept;

private:
    struct Pattern
    {
        const char* name;
        std::array<uint8_t, kTotalSteps> kick;
        std::array<uint8_t, kTotalSteps> snare;
        std::array<uint8_t, kTotalSteps> hatClosed;
        std::array<uint8_t, kTotalSteps> hatOpen;
        std::array<int,     kTotalSteps> bass; // semitone offset from root, or kRest
    };

    static int juceClampRhythm (int idx) noexcept
    {
        return idx < 0 ? 0 : (idx >= kNumRhythms ? kNumRhythms - 1 : idx);
    }

    void advanceStep() noexcept;
    void fireStep (int step) noexcept;

    // Fill-in modifiers (§5.2).
    bool kickOverrideActive() const noexcept { return fillMode == FillMode::KickQuarter; }
    bool snareOverrideActive() const noexcept { return fillMode == FillMode::SnareEighth; }

    enum class FillMode { None, KickQuarter, SnareEighth };

    LFSRNoise noise;
    TwinT_Resonator kick;
    SnareDrum snare;
    HiHat hat;
    SlengTengBass bass;

    std::array<Pattern, kNumRhythms> patterns;

    double fs = 44100.0;
    double bpm = 90.0;
    double samplesPerStep = 0.0;
    double stepPhase = 0.0;   // sample counter within the current step
    int currentStep = 0;

    Transport transport = Transport::Stopped;
    FillMode fillMode = FillMode::None;
    bool fillHeld = false;
    bool fillWasHeld = false;
    bool pendingNormalOnBarLine = false;

    int activeRhythm = 0;
    int chordRoot = -1;
    float rhythmGain = 0.8f;
    float bassGain = 0.8f;
};
