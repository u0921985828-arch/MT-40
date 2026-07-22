#pragma once

#include "Biquad.h"
#include "ExpDecay.h"

// ---------------------------------------------------------------------------
// SlengTengBass — dedicated monophonic bass voice (§2 bass pool, §5.1 timbre).
//
//   Pure square wave -> 12 dB/oct (2-pole) LPF (fc ~= 800 Hz) -> pluck
//   envelope (fast attack, med-fast decay, ZERO sustain).
//
// Monophonic with Highest-Note priority: a new note always replaces the
// current one, and the played pitch tracks the highest note in the held set.
// ---------------------------------------------------------------------------
class SlengTengBass
{
public:
    void prepare (double sampleRate) noexcept;

    // Trigger the bass at a given MIDI note (retriggers the pluck envelope).
    void trigger (int midiNote) noexcept;

    // Release the current note (envelope decays; zero sustain means it is
    // already decaying, this just marks it free).
    void release() noexcept;

    void reset() noexcept;
    bool isActive() const noexcept { return env.isActive(); }
    int currentNote() const noexcept { return note; }

    float process() noexcept;

private:
    Biquad lpf;          // 12 dB/oct = single 2-pole section
    ExpDecay env;        // pluck: fast attack handled by short ramp-in
    double fs = 44100.0;
    double phase = 0.0;
    double inc = 0.0;
    int note = -1;

    // Very short linear attack to avoid a click, then exponential pluck decay.
    float attackGain = 0.0f;
    float attackInc = 0.0f;
};
