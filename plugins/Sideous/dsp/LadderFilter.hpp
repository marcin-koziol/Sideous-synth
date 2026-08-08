/*
 * Sideous - simplified 4-pole digital ladder filter (Moog-style), for the
 * "LADDER 24dB" filter mode. Not a precise analog model, just four one-pole
 * lowpass stages in series with global feedback and a saturating input,
 * enough to get the characteristic squelchy self-oscillating 24dB slope.
 */

#pragma once

#include <cmath>
#include <algorithm>

namespace sideous {

class LadderFilter
{
public:
    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    // resonance 0..1; ~1 sits near self-oscillation
    void setResonance(float res01) noexcept
    {
        fRes = std::clamp(res01, 0.0f, 1.0f);
    }

    // 0 = clean, 1 = hard input saturation for a fatter, more aggressive tone
    void setDrive(float drive01) noexcept
    {
        fDrive = std::clamp(drive01, 0.0f, 1.0f);
    }

    void reset() noexcept
    {
        fS1 = fS2 = fS3 = fS4 = 0.0f;
    }

    float process(float in, float cutoffHz) noexcept
    {
        cutoffHz = std::clamp(cutoffHz, 20.0f, (float)fSampleRate * 0.49f);

        if (fDrive > 0.0f)
        {
            const float driveGain = 1.0f + fDrive * 7.0f;
            const float saturated = std::tanh(in * driveGain);
            in = in + fDrive * (saturated - in);
        }

        // simple always-stable one-pole coefficient per stage
        const float g = 1.0f - std::exp(-2.0f * (float)M_PI * cutoffHz / (float)fSampleRate);

        // feedback amount; ~4.2 sits near self-oscillation for this topology
        const float k = fRes * 4.2f;

        const float feedback = k * fS4;
        const float x = std::tanh(in - feedback);

        fS1 += g * (x   - fS1);
        fS2 += g * (fS1 - fS2);
        fS3 += g * (fS2 - fS3);
        fS4 += g * (fS3 - fS4);

        return fS4;
    }

private:
    double fSampleRate = 44100.0;
    float fRes = 0.2f;
    float fDrive = 0.0f;
    float fS1 = 0.0f, fS2 = 0.0f, fS3 = 0.0f, fS4 = 0.0f;
};

} // namespace sideous
