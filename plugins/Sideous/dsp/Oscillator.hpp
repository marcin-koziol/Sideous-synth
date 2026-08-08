/*
 * Sideous - basic SID/POKEY-vibe oscillator (saw, pulse w/ PWM, triangle)
 * Uses PolyBLEP band-limiting on saw/pulse edges to keep aliasing sane.
 */

#pragma once

#include <cmath>

namespace sideous {

enum class Waveform {
    Saw = 0,
    Pulse,
    Triangle
};

class Oscillator
{
public:
    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void setFrequency(float hz) noexcept
    {
        fIncrement = (double)hz / fSampleRate;
    }

    void setWaveform(Waveform w) noexcept { fWaveform = w; }

    // 0.01 .. 0.99, only used by Pulse waveform
    void setPulseWidth(float pw) noexcept
    {
        fPulseWidth = pw < 0.01f ? 0.01f : (pw > 0.99f ? 0.99f : pw);
    }

    void resetPhase(float phase = 0.0f) noexcept { fPhase = phase; }

    float process() noexcept
    {
        float out;

        switch (fWaveform)
        {
        case Waveform::Saw:
            out = 2.0f * (float)fPhase - 1.0f;
            out -= polyBlep(fPhase, fIncrement);
            break;

        case Waveform::Pulse:
        {
            out = fPhase < fPulseWidth ? 1.0f : -1.0f;
            out += polyBlep(fPhase, fIncrement);

            double phase2 = fPhase + (1.0 - fPulseWidth);
            if (phase2 >= 1.0)
                phase2 -= 1.0;
            out -= polyBlep(phase2, fIncrement);
            break;
        }

        case Waveform::Triangle:
        default:
        {
            // integrate a PolyBLEP square wave into a triangle (cheap, click-free)
            float sq = fPhase < 0.5f ? 1.0f : -1.0f;
            sq += polyBlep(fPhase, fIncrement);
            double phase2 = fPhase + 0.5;
            if (phase2 >= 1.0)
                phase2 -= 1.0;
            sq -= polyBlep(phase2, fIncrement);

            // integrate the (normalized-amplitude) square into a triangle, with
            // a slow leak to bleed off DC drift from the discrete integration
            fTriIntegrator += (float)(4.0 * fIncrement) * sq;
            fTriIntegrator -= fTriIntegrator * 0.0005f;
            out = fTriIntegrator;
            break;
        }
        }

        fPhase += fIncrement;
        if (fPhase >= 1.0)
            fPhase -= 1.0;
        else if (fPhase < 0.0)
            fPhase += 1.0;

        return out;
    }

private:
    static float polyBlep(double t, double dt) noexcept
    {
        if (dt <= 0.0)
            return 0.0f;

        if (t < dt)
        {
            const double x = t / dt;
            return (float)(x + x - x * x - 1.0);
        }
        else if (t > 1.0 - dt)
        {
            const double x = (t - 1.0) / dt;
            return (float)(x * x + x + x + 1.0);
        }
        return 0.0f;
    }

    double fSampleRate = 44100.0;
    double fPhase = 0.0;
    double fIncrement = 0.0;
    float fPulseWidth = 0.5f;
    float fTriIntegrator = 0.0f;
    Waveform fWaveform = Waveform::Saw;
};

} // namespace sideous
