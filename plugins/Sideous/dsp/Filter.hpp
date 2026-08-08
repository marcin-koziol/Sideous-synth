/*
 * Sideous - state-variable filter (TPT/Zavalishin topology), switchable
 * lowpass/highpass, stable under fast cutoff modulation (used for the
 * filter envelope).
 */

#pragma once

#include <cmath>
#include <algorithm>

namespace sideous {

enum class FilterType { Lowpass = 0, Highpass, Bandpass };

// the selectable filter modes exposed to the host/UI; Filter itself only
// knows how to be a single 12dB LP/HP/BP stage, Voice combines stages to
// realize the 24dB and ladder modes.
enum class FilterMode { LP12 = 0, LP24, HP12, HP24, BP12, Ladder24 };

class Filter
{
public:
    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void setType(FilterType t) noexcept { fType = t; }

    // resonance 0..1, mapped internally to a sane Q range
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
        fIc1eq = fIc2eq = 0.0f;
    }

    // cutoffHz should already include any envelope/keytrack modulation.
    // Zero-delay-feedback SVF (Andrew Simper / Zavalishin TPT form).
    float process(float in, float cutoffHz) noexcept
    {
        cutoffHz = std::clamp(cutoffHz, 20.0f, (float)fSampleRate * 0.49f);

        if (fDrive > 0.0f)
        {
            const float driveGain = 1.0f + fDrive * 7.0f;
            const float saturated = std::tanh(in * driveGain);
            in = in + fDrive * (saturated - in);
        }

        const float g = std::tan((float)M_PI * cutoffHz / (float)fSampleRate);
        // resonance curved so the upper half of the knob opens up faster;
        // fRes 0 -> heavily damped, fRes 1 -> self-oscillation
        const float resCurved = std::pow(fRes, 0.7f);
        const float k = 2.0f - 1.995f * resCurved;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float v3 = in - fIc2eq;
        const float v1 = a1 * fIc1eq + a2 * v3;
        const float v2 = fIc2eq + a2 * fIc1eq + a3 * v3;

        fIc1eq = 2.0f * v1 - fIc1eq;
        fIc2eq = 2.0f * v2 - fIc2eq;

        const float lowpass = v2;
        const float highpass = in - k * v1 - v2;
        const float bandpass = v1;

        switch (fType)
        {
        case FilterType::Highpass: return highpass;
        case FilterType::Bandpass: return bandpass;
        case FilterType::Lowpass:
        default:                   return lowpass;
        }
    }

private:
    double fSampleRate = 44100.0;
    float fRes = 0.2f;
    float fDrive = 0.0f;
    float fIc1eq = 0.0f;
    float fIc2eq = 0.0f;
    FilterType fType = FilterType::Lowpass;
};

} // namespace sideous
