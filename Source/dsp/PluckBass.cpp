#include "PluckBass.h"
#include <cmath>

void PluckBass::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    lpf.setLowPass (fs, 800.0, 0.707); // 12 dB/oct pluck tone
    env.prepare (fs);
    env.setDecay (0.22);               // med-fast pluck decay, zero sustain
    reset();
}

void PluckBass::reset() noexcept
{
    lpf.reset();
    env.hardReset();
    phase = 0.0;
    note = -1;
    attackGain = 0.0f;
}

void PluckBass::trigger (int midiNote) noexcept
{
    note = midiNote;
    const double freq = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
    inc = freq / fs;
    phase = 0.0;
    env.trigger();
    attackGain = 0.0f;
    attackInc = static_cast<float> (1.0 / (0.003 * fs)); // ~3 ms fast attack
}

void PluckBass::release() noexcept
{
    // Zero-sustain pluck: nothing to hold. Leave the envelope decaying.
    note = -1;
}

float PluckBass::process() noexcept
{
    if (! env.isActive()) return 0.0f;

    // Pure square wave.
    const float sq = (phase < 0.5) ? 1.0f : -1.0f;
    phase += inc; if (phase >= 1.0) phase -= 1.0;

    if (attackGain < 1.0f)
    {
        attackGain += attackInc;
        if (attackGain > 1.0f) attackGain = 1.0f;
    }

    const float filtered = lpf.process (sq);
    return filtered * env.getNextSample() * attackGain;
}
