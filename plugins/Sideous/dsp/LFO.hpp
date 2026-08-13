/*
 * Sideous - a simple free-running LFO (sine/saw/square) for pitch, filter
 * cutoff, amplitude, or pulse-width modulation. Not band-limited: LFO rates
 * are always far below audio range so aliasing isn't a concern.
 */

#pragma once

#include <cmath>

namespace sideous {

enum class LfoWaveform { Sine = 0, Saw, Square };
// PulseWidth is appended rather than inserted, so its numeric value (3)
// doesn't shift Cutoff/Amplitude and break existing presets/automation.
enum class LfoDestination { Pitch = 0, Cutoff, Amplitude, PulseWidth };

class LFO
{
public:
    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void setWaveform(LfoWaveform w) noexcept { fWaveform = w; }
    void setFrequency(float hz) noexcept { fIncrement = (double)hz / fSampleRate; }

    // retrigger to a consistent starting phase, called on note-on
    void reset() noexcept { fPhase = 0.0; }

    // returns -1..1
    float process() noexcept
    {
        float out;
        switch (fWaveform)
        {
        case LfoWaveform::Saw:
            out = 2.0f * (float)fPhase - 1.0f;
            break;
        case LfoWaveform::Square:
            out = fPhase < 0.5 ? 1.0f : -1.0f;
            break;
        case LfoWaveform::Sine:
        default:
            out = std::sin(2.0f * (float)M_PI * (float)fPhase);
            break;
        }

        fPhase += fIncrement;
        if (fPhase >= 1.0)
            fPhase -= 1.0;

        return out;
    }

private:
    double fSampleRate = 44100.0;
    double fPhase = 0.0;
    double fIncrement = 0.0;
    LfoWaveform fWaveform = LfoWaveform::Sine;
};

} // namespace sideous
