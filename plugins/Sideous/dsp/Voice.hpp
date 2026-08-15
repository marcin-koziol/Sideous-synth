/*
 * Sideous - a single synth voice: oscillator (+ suboscillator) -> amp ADSR,
 * in parallel with a filter whose cutoff is modulated by its own ADSR and two
 * independent LFOs (each can target pitch, cutoff, amplitude, or pulse
 * width) - e.g. LFO1 on Pulse Width and LFO2 on Pitch (vibrato) at once.
 */

#pragma once

#include <cmath>

#include "Oscillator.hpp"
#include "ADSR.hpp"
#include "Filter.hpp"
#include "LadderFilter.hpp"
#include "LFO.hpp"

namespace sideous {

// mod wheel (MIDI CC1) is a separate, independent modulation source from the
// LFO: it always targets whichever of these is picked, regardless of what
// the LFO's own Destination selector is set to.
enum class ModWheelDestination { Off = 0, Vibrato, Cutoff, Volume };

struct VoiceParams
{
    Waveform waveform = Waveform::Saw;
    float pulseWidth = 0.5f;

    float subOctave = -1.0f;        // -2, -1, +1, or +2
    float subLevel = 0.0f;          // 0..1, mixed in pre-filter

    FilterMode filterMode = FilterMode::LP12;
    float filterCutoff = 2000.0f;   // Hz, base cutoff before envelope/LFO
    float filterResonance = 0.2f;   // 0..1
    float filterDrive = 0.0f;       // 0..1
    float filterEnvAmount = 0.0f;   // -1..1: at +-1 the filter envelope alone
                                     // can sweep the full cutoff range

    float ampAttack = 0.005f, ampDecay = 0.1f, ampSustain = 0.8f, ampRelease = 0.2f;
    float ampCurve = 0.35f;         // 0..1, 0 = linear, 1 = analog-style
    float velocitySensitivity = 1.0f; // 0 = velocity ignored, 1 = full response

    float filterAttack = 0.005f, filterDecay = 0.2f, filterSustain = 0.0f, filterRelease = 0.2f;
    float filterCurve = 0.35f;      // 0..1, 0 = linear, 1 = analog-style

    // step-sequencer values the LFO holds through each block of its cycle
    // (no interpolation between steps); filled either by a one-click shape
    // preset (see LfoStepPreset/fillLfoStepPreset in LFO.hpp) or by hand.
    // Defaults mirror Params.hpp's default (a full 16-point sine cycle) so
    // this struct's own construction matches the declared parameter defaults,
    // same as every other field here.
    float lfoSteps[kLfoMaxSteps] = {
        0.0000f,  0.3827f,  0.7071f,  0.9239f,  1.0000f,  0.9239f,  0.7071f,  0.3827f,
        0.0000f, -0.3827f, -0.7071f, -0.9239f, -1.0000f, -0.9239f, -0.7071f, -0.3827f };
    int lfoStepCount = 16;
    LfoDestination lfoDestination = LfoDestination::Pitch;
    float lfoAmount = 0.0f;         // 0..1; LFO frequency is set separately
                                     // per-block from tempo, see setLfoFrequency()

    // independent second LFO - same shape as the first, so e.g. LFO1 can
    // target Pulse Width while LFO2 handles vibrato (Pitch) at the same time
    float lfo2Steps[kLfoMaxSteps] = {
        0.0000f,  0.3827f,  0.7071f,  0.9239f,  1.0000f,  0.9239f,  0.7071f,  0.3827f,
        0.0000f, -0.3827f, -0.7071f, -0.9239f, -1.0000f, -0.9239f, -0.7071f, -0.3827f };
    int lfo2StepCount = 16;
    LfoDestination lfo2Destination = LfoDestination::Cutoff;
    float lfo2Amount = 0.0f;

    float portamentoTime = 0.0f;    // seconds; 0 = off, snaps to the new pitch

    ModWheelDestination modWheelDestination = ModWheelDestination::Off;
    float modWheelAmount = 0.5f; // 0..1, scales whichever destination is
                                  // active against its own musical max (see
                                  // kVibratoMaxSemitones and the Cutoff/Volume
                                  // formulas in process())
    // pitch bend range and the live mod wheel/pitch bend values themselves
    // are NOT here - they're continuous controllers pushed every block via
    // setModWheel()/setPitchBend(), see below.
};

class Voice
{
public:
    // must match Params.hpp's kParamFilterCutoff range
    static constexpr float kCutoffMin = 20.0f;
    static constexpr float kCutoffMax = 20000.0f;
    // pitch swing at modWheelAmount = 1 (full wheel + full amount)
    static constexpr float kVibratoMaxSemitones = 2.0f;

    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        fOsc.setSampleRate(sampleRate);
        fSubOsc.setSampleRate(sampleRate);
        fAmpEnv.setSampleRate(sampleRate);
        fFilterEnv.setSampleRate(sampleRate);
        fFilterA.setSampleRate(sampleRate);
        fFilterB.setSampleRate(sampleRate);
        fLadder.setSampleRate(sampleRate);
        fLfo.setSampleRate(sampleRate);
        fLfo2.setSampleRate(sampleRate);
    }

    // retriggerEnvelope=false gives legato behavior: the pitch still moves
    // (gliding via portamento if set) but the amp/filter envelopes and LFO
    // phase are left running rather than restarting - used for mono-mode
    // note-stack fallback and overlapping ("legato") playing.
    //
    // glideFromFreq is a 3-way sentinel controlling where a glide starts from:
    //   < 0  "auto"           - glide only if this voice was already active
    //                           (uses its current pitch as the source); this
    //                           is the right choice when the caller has
    //                           already confirmed a genuine overlap
    //   == 0 "force no glide" - always snap straight to the target pitch,
    //                           even if the voice is still audibly ringing
    //                           out a release (e.g. a fresh, non-overlapping
    //                           staccato attack in Legato glide mode)
    //   > 0  "force glide"    - always glide, starting from this frequency,
    //                           regardless of whether the voice was active
    //                           (Always glide mode, or poly's "glide from
    //                           whatever note last played" behavior)
    void noteOn(int note, float velocity, bool retriggerEnvelope = true, float glideFromFreq = -1.0f) noexcept
    {
        fNote = note;
        fVelocity = velocity;
        const float targetFreq = noteToHz(note);
        const float sourceFreq = glideFromFreq > 0.0f ? glideFromFreq
                                : glideFromFreq == 0.0f ? -1.0f
                                : (fActive ? fCurrentFreq : -1.0f);

        if (sourceFreq > 0.0f && fPortamentoTime > 0.0005f)
        {
            fGlideFromLog = std::log2(sourceFreq);
            fGlideToLog = std::log2(targetFreq);
            fGlidePos = 0.0f;
            fGlideIncrement = (float)(1.0 / ((double)fPortamentoTime * fSampleRate));
        }
        else
        {
            fCurrentFreq = targetFreq;
            fGlidePos = 1.0f;
        }

        if (retriggerEnvelope)
        {
            fAmpEnv.noteOn();
            fFilterEnv.noteOn();
            fLfo.reset();
            fLfo2.reset();
        }
        fActive = true;
    }

    void noteOff() noexcept
    {
        fAmpEnv.noteOff();
        fFilterEnv.noteOff();
    }

    // hard cut, used for voice stealing
    void kill() noexcept
    {
        fActive = false;
    }

    bool isActive() const noexcept { return fActive && !fAmpEnv.isIdle(); }
    bool isReleasing() const noexcept { return fAmpEnv.getStage() == ADSR::Stage::Release; }
    int getNote() const noexcept { return fNote; }

    // set every audio block from the host tempo (or the free-rate knob);
    // kept separate from applyParams() since it needs to track tempo changes
    // without re-touching every other per-note setting each block
    void setLfoFrequency(float hz) noexcept { fLfo.setFrequency(hz); }
    void setLfo2Frequency(float hz) noexcept { fLfo2.setFrequency(hz); }

    // continuous MIDI controllers, also pushed every block regardless of
    // whether a note is currently sounding (so they're already in effect
    // the instant the next note starts)
    void setModWheel(float value01) noexcept { fModWheel = value01; }
    void setPitchBend(float semitones) noexcept { fPitchBendSemitones = semitones; }

    void applyParams(const VoiceParams& p) noexcept
    {
        fOsc.setWaveform(p.waveform);
        fSubOsc.setWaveform(p.waveform);
        fBasePulseWidth = p.pulseWidth; // combined with the LFO per-sample in process()
        fSubOctave = p.subOctave;
        fSubLevel = p.subLevel;

        fFilterMode = p.filterMode;
        const FilterType stageType = p.filterMode == FilterMode::HP12 || p.filterMode == FilterMode::HP24
                                    ? FilterType::Highpass
                                    : p.filterMode == FilterMode::BP12
                                    ? FilterType::Bandpass
                                    : FilterType::Lowpass;
        fFilterA.setType(stageType);
        fFilterB.setType(stageType);
        fFilterA.setResonance(p.filterResonance);
        // for the 24dB modes, only the first stage carries the resonant peak;
        // giving both stages the same high resonance compounds their gain
        // quadratically and blows well past sane levels (peaks of 400+ measured)
        fFilterB.setResonance(0.0f);
        fFilterA.setDrive(p.filterDrive);
        fLadder.setResonance(p.filterResonance);
        fLadder.setDrive(p.filterDrive);

        fAmpEnv.setAttack(p.ampAttack);
        fAmpEnv.setDecay(p.ampDecay);
        fAmpEnv.setSustain(p.ampSustain);
        fAmpEnv.setRelease(p.ampRelease);
        fAmpEnv.setCurve(p.ampCurve);
        fVelocitySensitivity = p.velocitySensitivity;

        fFilterEnv.setAttack(p.filterAttack);
        fFilterEnv.setDecay(p.filterDecay);
        fFilterEnv.setSustain(p.filterSustain);
        fFilterEnv.setRelease(p.filterRelease);
        fFilterEnv.setCurve(p.filterCurve);

        fBaseCutoff = p.filterCutoff;
        fEnvAmount = p.filterEnvAmount;

        fLfo.setSteps(p.lfoSteps, p.lfoStepCount);
        fLfoDestination = p.lfoDestination;
        fLfoAmount = p.lfoAmount;

        fLfo2.setSteps(p.lfo2Steps, p.lfo2StepCount);
        fLfo2Destination = p.lfo2Destination;
        fLfo2Amount = p.lfo2Amount;

        fPortamentoTime = p.portamentoTime;

        fModWheelDestination = p.modWheelDestination;
        fModWheelAmount = p.modWheelAmount;
    }

    float process() noexcept
    {
        if (!fActive)
            return 0.0f;

        if (fGlidePos < 1.0f)
        {
            fGlidePos += fGlideIncrement;
            if (fGlidePos >= 1.0f)
            {
                fGlidePos = 1.0f;
                fCurrentFreq = std::exp2(fGlideToLog);
            }
            else
            {
                fCurrentFreq = std::exp2(fGlideFromLog + (fGlideToLog - fGlideFromLog) * fGlidePos);
            }
        }

        const float lfo = fLfo.process();   // -1..1; always advance phase so it
        const float lfo2 = fLfo2.process(); // stays click-free if destination changes mid-note

        // mod wheel vibrato rides LFO1's oscillator/rate specifically (unchanged
        // from before LFO2 existed), but is an independent depth from LFO1's own
        // Amount knob - it always affects pitch when selected, regardless of what
        // either LFO's Destination is set to (so e.g. LFO1->Cutoff, LFO2->Pitch,
        // and ModWheel->Vibrato can all coexist). fModWheelAmount (0..1, shared
        // across all three destinations) scales each one against its own
        // musically-sane max, rather than every destination being a fixed,
        // non-adjustable depth.
        const float lfoPitchSemitones = (fLfoDestination == LfoDestination::Pitch ? fLfoAmount : 0.0f) * 12.0f;
        const float lfo2PitchSemitones = (fLfo2Destination == LfoDestination::Pitch ? fLfo2Amount : 0.0f) * 12.0f;
        const float vibratoSemitones = fModWheelDestination == ModWheelDestination::Vibrato
                                      ? fModWheel * fModWheelAmount * kVibratoMaxSemitones : 0.0f;
        const float pitchSemitones = lfo * (lfoPitchSemitones + vibratoSemitones)
                                    + lfo2 * lfo2PitchSemitones + fPitchBendSemitones;
        const float pitchRatio = std::exp2(pitchSemitones / 12.0f);
        fOsc.setFrequency(fCurrentFreq * pitchRatio);
        fSubOsc.setFrequency(fCurrentFreq * std::exp2(fSubOctave) * pitchRatio);

        // pulse width is set every sample (rather than once in applyParams())
        // so the LFO can modulate it at audio rate, same as cutoff below;
        // Oscillator::setPulseWidth() clamps to 0.01..0.99 for us
        float pulseWidth = fBasePulseWidth;
        if (fLfoDestination == LfoDestination::PulseWidth)
            pulseWidth += fLfoAmount * lfo * 0.48f;
        if (fLfo2Destination == LfoDestination::PulseWidth)
            pulseWidth += fLfo2Amount * lfo2 * 0.48f;
        fOsc.setPulseWidth(pulseWidth);
        fSubOsc.setPulseWidth(pulseWidth);

        const float osc = fOsc.process() + fSubOsc.process() * fSubLevel;
        const float ampLevel = fAmpEnv.process();
        const float filterEnvLevel = fFilterEnv.process();

        if (fAmpEnv.isIdle())
        {
            fActive = false;
            return 0.0f;
        }

        // base cutoff + filter envelope + (optional) LFO all combine in the
        // same normalized log-frequency space, so at envAmount/lfoAmount = 1
        // the modulation can sweep the *entire* cutoff range regardless of
        // where the base cutoff knob sits, not just a few fixed octaves
        float t = cutoffToNormalized(fBaseCutoff) + fEnvAmount * filterEnvLevel;
        if (fLfoDestination == LfoDestination::Cutoff)
            t += fLfoAmount * lfo;
        if (fLfo2Destination == LfoDestination::Cutoff)
            t += fLfo2Amount * lfo2;
        if (fModWheelDestination == ModWheelDestination::Cutoff)
            t += fModWheel * fModWheelAmount; // amount=1 can sweep the *entire* range, same convention as envAmount/lfoAmount above
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const float cutoff = normalizedToCutoff(t);

        float filtered;
        switch (fFilterMode)
        {
        case FilterMode::LP24:
        case FilterMode::HP24:
            filtered = fFilterB.process(fFilterA.process(osc, cutoff), cutoff);
            break;
        case FilterMode::Ladder24:
            filtered = fLadder.process(osc, cutoff);
            break;
        case FilterMode::LP12:
        case FilterMode::HP12:
        case FilterMode::BP12:
        default:
            filtered = fFilterA.process(osc, cutoff);
            break;
        }

        float ampMod = 1.0f;
        if (fLfoDestination == LfoDestination::Amplitude)
            ampMod *= 1.0f - fLfoAmount * (1.0f - (lfo * 0.5f + 0.5f));
        if (fLfo2Destination == LfoDestination::Amplitude)
            ampMod *= 1.0f - fLfo2Amount * (1.0f - (lfo2 * 0.5f + 0.5f));
        if (fModWheelDestination == ModWheelDestination::Volume)
            ampMod *= (1.0f + fModWheel * fModWheelAmount); // amount=1 -> up to +6dB-ish boost at full wheel

        const float velocityGain = 1.0f - fVelocitySensitivity * (1.0f - fVelocity);
        return filtered * ampLevel * velocityGain * ampMod;
    }

private:
    static float noteToHz(int note) noexcept
    {
        return 440.0f * std::exp2(((float)note - 69.0f) / 12.0f);
    }

    static float cutoffToNormalized(float hz) noexcept
    {
        return std::log(hz / kCutoffMin) / std::log(kCutoffMax / kCutoffMin);
    }

    static float normalizedToCutoff(float t) noexcept
    {
        return kCutoffMin * std::pow(kCutoffMax / kCutoffMin, t);
    }

    Oscillator fOsc;
    Oscillator fSubOsc;
    ADSR fAmpEnv;
    ADSR fFilterEnv;
    Filter fFilterA;
    Filter fFilterB;      // only used for the 24dB cascaded modes
    LadderFilter fLadder; // only used for the ladder mode
    LFO fLfo;
    LFO fLfo2;

    FilterMode fFilterMode = FilterMode::LP12;
    float fBaseCutoff = 2000.0f;
    float fEnvAmount = 0.0f;
    float fBasePulseWidth = 0.5f;
    float fSubOctave = -1.0f;
    float fSubLevel = 0.0f;
    float fVelocitySensitivity = 1.0f;

    double fSampleRate = 44100.0;
    float fCurrentFreq = 440.0f;   // instantaneous pitch, moves during a glide
    float fGlideFromLog = 0.0f;    // log2(Hz) glide endpoints
    float fGlideToLog = 0.0f;
    float fGlidePos = 1.0f;        // 0..1, 1 = glide complete/inactive
    float fGlideIncrement = 1.0f;
    float fPortamentoTime = 0.0f;

    LfoDestination fLfoDestination = LfoDestination::Pitch;
    float fLfoAmount = 0.0f;

    LfoDestination fLfo2Destination = LfoDestination::Cutoff;
    float fLfo2Amount = 0.0f;

    ModWheelDestination fModWheelDestination = ModWheelDestination::Off;
    float fModWheel = 0.0f;
    float fModWheelAmount = 0.5f;
    float fPitchBendSemitones = 0.0f;

    int fNote = -1;
    float fVelocity = 0.0f;
    bool fActive = false;
};

} // namespace sideous
