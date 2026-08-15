#pragma once
#include <cmath>
namespace sideous {

enum class LfoDestination { Pitch = 0, Cutoff, Amplitude, PulseWidth };

// one-shot shapes used to *fill* the step sequencer (see fillLfoStepPreset
// below) - not a runtime playback mode. Once applied, the steps are just
// data the user can freely overwrite by drawing.
enum class LfoStepPreset { Sine = 0, Saw, Square, Gate };

static constexpr int kLfoMaxSteps = 16;

// samples one full cycle of the named shape into out[0..stepCount). Shared
// verbatim by the DSP (regenerating steps when the preset param is set,
// including via host automation) and the UI (so a freshly-clicked preset
// button can push the right values immediately without waiting on a
// round-trip through the audio thread).
inline void fillLfoStepPreset(LfoStepPreset preset, int stepCount, float* out) noexcept
{
    if (stepCount <= 0)
        return;
    const float n = (float)stepCount;
    for (int i = 0; i < stepCount; ++i)
    {
        switch (preset)
        {
        case LfoStepPreset::Saw:
            out[i] = -1.0f + 2.0f * (float)i / n;
            break;
        case LfoStepPreset::Square:
            out[i] = (float)i < n * 0.5f ? 1.0f : -1.0f;
            break;
        case LfoStepPreset::Gate:
        {
            const float phase = (float)i / n;
            out[i] = phase < 0.25f ? 0.0f : phase < 0.5f ? 1.0f : phase < 0.75f ? 0.0f : -1.0f;
            break;
        }
        case LfoStepPreset::Sine:
        default:
            out[i] = std::sin(2.0f * (float)M_PI * (float)i / n);
            break;
        }
    }
}

class LFO
{
public:
    void setSampleRate(double sampleRate) noexcept { fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0; }
    void setFrequency(float hz) noexcept { fIncrement = (double)hz / fSampleRate; }
    void reset() noexcept { fPhase = 0.0; }

    // steps are held (no interpolation between them), classic step-sequencer
    // behavior; count is clamped to the fixed storage size.
    void setSteps(const float* steps, int count) noexcept
    {
        fStepCount = count < 1 ? 1 : (count > kLfoMaxSteps ? kLfoMaxSteps : count);
        for (int i = 0; i < fStepCount; ++i)
            fSteps[i] = steps[i];
    }

    float process() noexcept
    {
        int stepIdx = (int)(fPhase * (double)fStepCount);
        if (stepIdx >= fStepCount)
            stepIdx = fStepCount - 1;
        const float out = fSteps[stepIdx];

        fPhase += fIncrement;
        if (fPhase >= 1.0)
            fPhase -= 1.0;
        return out;
    }

private:
    double fSampleRate = 44100.0;
    double fPhase = 0.0;
    double fIncrement = 0.0;
    float fSteps[kLfoMaxSteps] = {};
    int fStepCount = 1;
};

}
