#include "PluckBass.h"
#include <cmath>

void PluckBass::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    lpf.setLowPass (fs, 1300.0, 0.95); // deep, round "digital" bass body
    env.prepare (fs);
    env.setDecay (0.60);               // long-ish decay so eighth notes connect
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
    attackInc = static_cast<float> (1.0 / (0.004 * fs)); // ~4 ms fast attack
}

void PluckBass::release() noexcept
{
    // Zero-sustain pluck: nothing to hold. Leave the envelope decaying.
    note = -1;
}

float PluckBass::process() noexcept
{
    if (! env.isActive()) return 0.0f;

    // Pulse wave with a slight duty asymmetry for body.
    const float sq = (phase < 0.45) ? 1.0f : -1.0f;
    phase += inc; if (phase >= 1.0) phase -= 1.0;

    if (attackGain < 1.0f)
    {
        attackGain += attackInc;
        if (attackGain > 1.0f) attackGain = 1.0f;
    }

    // Soft drive for punch, then the pluck envelope and click-free attack ramp.
    const float filtered = std::tanh (1.4f * lpf.process (sq));
    return filtered * env.getNextSample() * attackGain * 1.15f;
}
