#include "RhythmEngine.h"
#include <algorithm>

namespace
{
    // Compact helpers for authoring 32-step patterns.
    constexpr uint8_t _ = 0; // no hit
    constexpr uint8_t X = 1; // hit
}

void RhythmEngine::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    kick.prepare (fs);
    snare.prepare (fs);
    hat.prepare (fs);
    bass.prepare (fs);
    setTempo (bpm);

    // -----------------------------------------------------------------
    // Rhythm 5: "Rock" — the ST-40 pattern that became the "Sleng Teng"
    // riddim (rightmost slider position). Hardcoded 2-bar loop (§5.1). The
    // bass row holds semitone offsets from the auto-chord root; kRest = rest.
    // -----------------------------------------------------------------
    patterns[5] = {
        "Rock",
        //         1 . . . 2 . . . 3 . . . 4 . . . | bar 2 ...
        { X,_,_,_, _,_,X,_, X,_,_,_, _,_,_,_,   X,_,_,_, _,_,X,_, X,_,_,_, _,_,X,_ }, // kick
        { _,_,_,_, X,_,_,_, _,_,_,_, X,_,_,_,   _,_,_,_, X,_,_,_, _,_,_,_, X,_,_,_ }, // snare
        { X,_,X,_, X,_,X,_, X,_,X,_, X,_,X,_,   X,_,X,_, X,_,X,_, X,_,X,_, X,_,X,_ }, // closed hat
        { _,_,_,_, _,_,_,_, _,_,_,_, _,_,X,_,   _,_,_,_, _,_,_,_, _,_,_,_, _,_,X,_ }, // open hat
        // bass: the Sleng Teng riff (offsets from root, kRest = tie/rest)
        { 0,kRest,0,kRest, 7,kRest,0,kRest, 0,kRest,3,kRest, 7,kRest,5,kRest,
          0,kRest,0,kRest, 7,kRest,0,kRest, 0,kRest,3,kRest, 5,kRest,3,kRest }
    };

    // Rhythm 3: "Slow Rock" (unchanged position)
    patterns[3] = {
        "Slow Rock",
        { X,_,_,_, _,_,_,_, X,_,_,_, _,_,_,_,   X,_,_,_, _,_,_,_, X,_,_,_, _,_,_,_ },
        { _,_,_,_, X,_,_,_, _,_,_,_, X,_,_,_,   _,_,_,_, X,_,_,_, _,_,_,_, X,_,_,_ },
        { X,_,X,_, X,_,X,_, X,_,X,_, X,_,X,_,   X,_,X,_, X,_,X,_, X,_,X,_, X,_,X,_ },
        { _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,   _,_,_,_, _,_,_,_, _,_,_,_, _,_,X,_ },
        { 0,kRest,kRest,kRest, 0,kRest,kRest,kRest, 7,kRest,kRest,kRest, 5,kRest,kRest,kRest,
          0,kRest,kRest,kRest, 0,kRest,kRest,kRest, 7,kRest,kRest,kRest, 3,kRest,kRest,kRest }
    };

    // Rhythm 4: "Pops"
    patterns[4] = {
        "Pops",
        { X,_,_,_, _,_,X,_, _,_,X,_, _,_,_,_,   X,_,_,_, _,_,X,_, _,_,X,_, _,_,_,_ },
        { _,_,_,_, X,_,_,_, _,_,_,_, X,_,_,_,   _,_,_,_, X,_,_,_, _,_,_,_, X,_,_,X },
        { X,X,X,X, X,X,X,X, X,X,X,X, X,X,X,X,   X,X,X,X, X,X,X,X, X,X,X,X, X,X,X,X },
        { _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,   _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_ },
        { 0,kRest,kRest,kRest, kRest,kRest,7,kRest, 0,kRest,kRest,kRest, 5,kRest,kRest,kRest,
          0,kRest,kRest,kRest, kRest,kRest,7,kRest, 0,kRest,kRest,kRest, 5,kRest,3,kRest }
    };

    // Rhythm 2: "Swing"
    patterns[2] = {
        "Swing",
        { X,_,_,_, _,_,_,_, X,_,_,X, _,_,_,_,   X,_,_,_, _,_,_,_, X,_,_,X, _,_,_,_ },
        { _,_,_,_, X,_,_,_, _,_,_,_, X,_,_,_,   _,_,_,_, X,_,_,_, _,_,_,_, X,_,_,_ },
        { X,_,_,X, _,_,X,_, _,X,_,_, X,_,_,X,   X,_,_,X, _,_,X,_, _,X,_,_, X,_,_,X },
        { _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,   _,_,_,_, _,_,_,_, _,_,_,_, _,_,X,_ },
        { 0,kRest,kRest,kRest, 3,kRest,kRest,kRest, 5,kRest,kRest,kRest, 7,kRest,kRest,kRest,
          0,kRest,kRest,kRest, 3,kRest,kRest,kRest, 5,kRest,kRest,kRest, 7,kRest,kRest,kRest }
    };

    // Rhythm 0: "Samba" (leftmost slider position)
    patterns[0] = {
        "Samba",
        { X,_,_,_, _,_,X,_, _,_,_,_, X,_,_,_,   _,_,X,_, _,_,_,_, X,_,_,_, _,_,X,_ },
        { _,_,X,_, _,_,_,_, X,_,_,_, _,_,X,_,   _,_,_,_, X,_,_,_, _,_,X,_, _,_,_,_ },
        { X,_,X,_, X,_,X,_, X,_,X,_, X,_,X,_,   X,_,X,_, X,_,X,_, X,_,X,_, X,_,X,_ },
        { _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,   _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_ },
        { 0,kRest,kRest,kRest, kRest,kRest,7,kRest, kRest,kRest,kRest,kRest, 5,kRest,kRest,kRest,
          kRest,kRest,3,kRest, kRest,kRest,kRest,kRest, 0,kRest,kRest,kRest, kRest,kRest,7,kRest }
    };

    // Rhythm 1: "Waltz" (3/4 feel over the 16-step grid, first 12 steps used)
    patterns[1] = {
        "Waltz",
        { X,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,   X,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_ },
        { _,_,_,_, X,_,_,_, X,_,_,_, _,_,_,_,   _,_,_,_, X,_,_,_, X,_,_,_, _,_,_,_ },
        { X,_,_,_, X,_,_,_, X,_,_,_, _,_,_,_,   X,_,_,_, X,_,_,_, X,_,_,_, _,_,_,_ },
        { _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_,   _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_ },
        { 0,kRest,kRest,kRest, 7,kRest,kRest,kRest, 7,kRest,kRest,kRest, kRest,kRest,kRest,kRest,
          0,kRest,kRest,kRest, 7,kRest,kRest,kRest, 7,kRest,kRest,kRest, kRest,kRest,kRest,kRest }
    };

    reset();
}

void RhythmEngine::reset() noexcept
{
    kick.reset();
    snare.reset();
    hat.reset();
    bass.reset();
    stepPhase = 0.0;
    currentStep = 0;
    transport = Transport::Stopped;
    fillMode = FillMode::None;
    fillHeld = fillWasHeld = false;
    pendingNormalOnBarLine = false;
}

void RhythmEngine::setTempo (double newBpm) noexcept
{
    bpm = std::clamp (newBpm, 40.0, 240.0);
    // 16th-note step duration in samples.
    const double stepsPerSecond = (bpm / 60.0) * 4.0;
    samplesPerStep = fs / stepsPerSecond;
}

void RhythmEngine::pressSynchro() noexcept
{
    if (transport == Transport::Stopped)
        transport = Transport::Armed;
}

void RhythmEngine::noteOnInSplitZone() noexcept
{
    // From STOPPED or ARMED, a note in the split zone starts playback.
    if (transport != Transport::Playing)
    {
        transport = Transport::Playing;
        currentStep = 0;
        stepPhase = 0.0;
        fireStep (0); // play the downbeat immediately
    }
}

void RhythmEngine::start() noexcept
{
    transport = Transport::Playing;
    currentStep = 0;
    stepPhase = 0.0;
    fireStep (0);
}

void RhythmEngine::stop() noexcept
{
    transport = Transport::Stopped;
    // Percussion/bass tails are left to decay naturally.
}

void RhythmEngine::allSplitNotesReleased() noexcept
{
    // Returning to stopped is left to host/UI policy; the free-running clock
    // keeps going while notes ring. We stop only if we were merely armed.
    if (transport == Transport::Armed)
        transport = Transport::Stopped;
}

void RhythmEngine::setFillHeld (bool held) noexcept
{
    if (held && ! fillWasHeld)
    {
        // Rising edge. First hold from normal -> 1/4 kicks; a second
        // consecutive hold (double-tap) -> 1/8 snares; a further tap cycles
        // back to 1/4 kicks. Deriving from the current mode (instead of a
        // latching counter) means a fresh single hold always starts at 1/4.
        fillMode = (fillMode == FillMode::KickQuarter) ? FillMode::SnareEighth
                                                       : FillMode::KickQuarter;
        pendingNormalOnBarLine = false;
    }
    else if (! held && fillWasHeld)
    {
        // Released: return to the normal pattern on the next bar line.
        pendingNormalOnBarLine = true;
    }

    fillHeld = held;
    fillWasHeld = held;
}

void RhythmEngine::advanceStep() noexcept
{
    currentStep = (currentStep + 1) % kTotalSteps;

    // Bar line handling (§5.2): resolve a pending "return to normal".
    if (currentStep % kStepsPerBar == 0)
    {
        if (pendingNormalOnBarLine && ! fillHeld)
        {
            fillMode = FillMode::None;
            pendingNormalOnBarLine = false;
        }
    }

    fireStep (currentStep);
}

void RhythmEngine::fireStep (int step) noexcept
{
    const Pattern& p = patterns[activeRhythm];

    // --- Kick ---------------------------------------------------------
    bool kickHit = p.kick[step] != 0;
    if (kickOverrideActive())
        kickHit = (step % 4 == 0); // constant 1/4-note kicks
    if (kickHit)
        kick.trigger();

    // --- Snare --------------------------------------------------------
    bool snareHit = p.snare[step] != 0;
    if (snareOverrideActive())
        snareHit = (step % 2 == 0); // 1/8-note snares
    if (snareHit)
        snare.trigger();

    // --- Hats ---------------------------------------------------------
    if (p.hatOpen[step] != 0)
        hat.triggerOpen();
    else if (p.hatClosed[step] != 0)
        hat.triggerClosed();

    // --- Bass ---------------------------------------------------------
    const int off = p.bass[step];
    if (off != kRest)
    {
        const int root = (chordRoot >= 0) ? chordRoot : 45; // default A2
        bass.trigger (root + off);
    }
}

float RhythmEngine::process() noexcept
{
    // Advance the free-running clock only while playing.
    if (transport == Transport::Playing)
    {
        stepPhase += 1.0;
        if (stepPhase >= samplesPerStep)
        {
            stepPhase -= samplesPerStep;
            advanceStep();
        }
    }

    // Percussion tails and the bass always render so notes ring out cleanly.
    const float drums = kick.process() + snare.process() + hat.process();
    const float bassOut = bass.process();

    return drums * rhythmGain + bassOut * bassGain;
}
