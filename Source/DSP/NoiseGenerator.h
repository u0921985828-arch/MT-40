#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/**
    White / pink noise source.

    White noise comes straight from JUCE's Random.  Pink noise (-3 dB/oct) is
    produced with Paul Kellet's economical filtering approximation, which is
    accurate to within a fraction of a dB across the audio band.
*/
class NoiseGenerator
{
public:
    enum class Type { white = 0, pink };

    NoiseGenerator() : random (juce::Random::getSystemRandom().nextInt64()) {}

    void setType (Type t) noexcept { type = t; }

    void reset() noexcept
    {
        b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    }

    inline float getNextSample() noexcept
    {
        const float white = random.nextFloat() * 2.0f - 1.0f;

        if (type == Type::white)
            return white;

        // Paul Kellet's refined pink-noise filter.
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;

        const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;

        return pink * 0.11f; // scale into roughly [-1, 1]
    }

private:
    juce::Random random;
    Type type { Type::white };
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
};
