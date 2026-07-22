#pragma once

#include "DCOVoice.h"
#include "Presets.h"
#include <array>

// ---------------------------------------------------------------------------
// VoiceAllocator — melodic polyphony management (§2).
//
//   * 8-voice melodic pool, Oldest-Note-Stealing when full.
//   * (The single dedicated bass voice is handled separately by the Sleng
//     Teng / Casio-Chord bass path, which is monophonic Highest-Note
//     priority — see SlengTengBass / RhythmEngine.)
// ---------------------------------------------------------------------------
class VoiceAllocator
{
public:
    static constexpr int kMelodicVoices = 8;

    void prepare (double sampleRate) noexcept
    {
        for (auto& v : voices) v.prepare (sampleRate);
    }

    void setPreset (const MelodicPreset& p) noexcept
    {
        for (auto& v : voices) v.setPreset (p);
    }

    void setReleaseMultiplier (double m) noexcept
    {
        for (auto& v : voices) v.setReleaseMultiplier (m);
    }

    void noteOn (int note, float velocity) noexcept
    {
        // Re-trigger if the same note is already sounding.
        for (auto& v : voices)
            if (v.isActive() && v.getNote() == note)
            {
                v.noteOn (note, velocity);
                return;
            }

        // Find a free voice.
        for (auto& v : voices)
            if (! v.isActive())
            {
                v.noteOn (note, velocity);
                return;
            }

        // All busy -> steal the OLDEST note (largest age).
        DCOVoice* oldest = &voices[0];
        for (auto& v : voices)
            if (v.getAge() > oldest->getAge())
                oldest = &v;
        oldest->noteOn (note, velocity);
    }

    void noteOff (int note) noexcept
    {
        for (auto& v : voices)
            if (v.isActive() && v.getNote() == note)
                v.noteOff();
    }

    void allNotesOff() noexcept
    {
        for (auto& v : voices) v.noteOff();
    }

    void reset() noexcept
    {
        for (auto& v : voices) v.reset();
    }

    // Sum all active voices for one sample and advance their ages.
    inline float render (float lfoValue, float vibratoDepthSemis) noexcept
    {
        float out = 0.0f;
        for (auto& v : voices)
        {
            if (v.isActive())
            {
                out += v.render (lfoValue, vibratoDepthSemis);
                v.tickAge();
            }
        }
        return out;
    }

private:
    std::array<DCOVoice, kMelodicVoices> voices;
};
