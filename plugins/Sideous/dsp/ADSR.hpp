/*
 * Sideous - ADSR envelope generator with a shapeable curve.
 *
 * Each stage interpolates from its start level to its target level over
 * a normalized progress fraction t in [0,1], bent through t^exponent.
 * exponent < 1 gives the classic analog RC feel (fast initial move,
 * slow settle into the target) on attack, decay, and release alike.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace sideous {

class ADSR
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    // all times in seconds, sustain is a 0..1 level
    void setAttack(float seconds) noexcept  { fAttack  = std::max(0.0005f, seconds); }
    void setDecay(float seconds) noexcept   { fDecay   = std::max(0.0005f, seconds); }
    void setSustain(float level) noexcept   { fSustain = std::clamp(level, 0.0f, 1.0f); }
    void setRelease(float seconds) noexcept { fRelease = std::max(0.0005f, seconds); }

    // 0 = linear, 1 = strongly analog-style (fast move, slow settle)
    void setCurve(float amount) noexcept
    {
        const float curve = std::clamp(amount, 0.0f, 1.0f);
        fExponent = std::pow(4.0f, -curve); // 1 .. 0.25
    }

    void noteOn() noexcept
    {
        beginStage(Stage::Attack, 1.0f, fAttack);
    }

    void noteOff() noexcept
    {
        if (fStage != Stage::Idle)
            beginStage(Stage::Release, 0.0f, fRelease);
    }

    bool isIdle() const noexcept { return fStage == Stage::Idle; }

    float process() noexcept
    {
        if (fStage == Stage::Idle)
        {
            fLevel = 0.0f;
            return fLevel;
        }

        if (fStage == Stage::Sustain)
        {
            fLevel = fSustain;
            return fLevel;
        }

        const float t = fStageLength > 0 ? (float)fStageSample / (float)fStageLength : 1.0f;
        const float shaped = std::pow(std::clamp(t, 0.0f, 1.0f), fExponent);
        fLevel = fStartLevel + (fEndLevel - fStartLevel) * shaped;

        ++fStageSample;
        if (fStageSample >= fStageLength)
        {
            switch (fStage)
            {
            case Stage::Attack:  beginStage(Stage::Decay, fSustain, fDecay); break;
            case Stage::Decay:   fLevel = fSustain; fStage = Stage::Sustain; break;
            case Stage::Release: fLevel = 0.0f; fStage = Stage::Idle; break;
            default: break;
            }
        }

        return fLevel;
    }

    Stage getStage() const noexcept { return fStage; }
    float getLevel() const noexcept { return fLevel; }

private:
    void beginStage(Stage stage, float targetLevel, float seconds) noexcept
    {
        fStage = stage;
        fStartLevel = fLevel;
        fEndLevel = targetLevel;
        fStageLength = std::max(1u, (uint32_t)(seconds * (float)fSampleRate));
        fStageSample = 0;
    }

    double fSampleRate = 44100.0;
    float fAttack = 0.01f;
    float fDecay = 0.1f;
    float fSustain = 0.8f;
    float fRelease = 0.2f;
    float fExponent = 1.0f;

    float fLevel = 0.0f;
    float fStartLevel = 0.0f;
    float fEndLevel = 0.0f;
    uint32_t fStageLength = 1;
    uint32_t fStageSample = 0;
    Stage fStage = Stage::Idle;
};

} // namespace sideous
