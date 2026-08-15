/*
 * Sideous - a basic SID/POKEY-vibe polyphonic synth.
 * Saw / pulse (with PWM) / triangle oscillator, amp ADSR, and a
 * lowpass/highpass filter with its own ADSR. No built-in effects.
 */

#include "DistrhoPlugin.hpp"
#include "Params.hpp"
#include "dsp/Voice.hpp"
#include "dsp/Sync.hpp"

#include <array>
#include <cmath>

START_NAMESPACE_DISTRHO

using namespace sideous;

// -----------------------------------------------------------------------------------------------------------

static constexpr const uint32_t kNumVoices = 16;
static constexpr const uint32_t kMaxHeldNotes = 16;
// grace window after the first note of a new arp phrase before step 0 actually
// triggers, so a chord played "simultaneously" (still separate MIDI events a
// few ms apart) is fully gathered before the sequence starts
static constexpr const float kArpChordGraceSeconds = 0.03f;

enum class ArpPattern { Up = 0, Down, UpDown, Random };

// LFO/Arp "Sync" dropdown: 0 = free-running (use the Rate Hz knob),
// 1..16 = SyncDivision::Whole..OneTwentyEighth
static ParameterEnumerationValue kSyncEnumValues[17] = {
    {  0.0f, "Free" },
    {  1.0f, "1/1" },
    {  2.0f, "1/2." },
    {  3.0f, "1/2" },
    {  4.0f, "1/2T" },
    {  5.0f, "1/4." },
    {  6.0f, "1/4" },
    {  7.0f, "1/4T" },
    {  8.0f, "1/8." },
    {  9.0f, "1/8" },
    { 10.0f, "1/8T" },
    { 11.0f, "1/16." },
    { 12.0f, "1/16" },
    { 13.0f, "1/16T" },
    { 14.0f, "1/32" },
    { 15.0f, "1/64" },
    { 16.0f, "1/128" },
};

static float syncedHz(float syncParam, float freeHz, double bpm) noexcept
{
    const int sync = (int)(syncParam + 0.5f);
    if (sync <= 0)
        return freeHz;
    return hzFromSyncDivision((SyncDivision)(sync - 1), bpm);
}

static float noteToHz(int note) noexcept
{
    return 440.0f * std::exp2(((float)note - 69.0f) / 12.0f);
}

class SideousPlugin : public Plugin
{
public:
    SideousPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        sampleRateChanged(getSampleRate());
    }

protected:
    // ---------------------------------------------------------------------
    // Information

    const char* getLabel() const override { return "Sideous"; }
    const char* getDescription() const override
    {
        return "Basic SID/POKEY-vibe polyphonic synth: saw/pulse(PWM)/triangle oscillator "
               "with suboscillator, amp ADSR with velocity sensitivity, a multi-mode filter "
               "(12/24dB LP/HP, bandpass, ladder) with its own ADSR, a tempo-syncable LFO, "
               "and a basic arpeggiator.";
    }
    const char* getMaker() const override { return "Sideous"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 2, 0); }

    // ---------------------------------------------------------------------
    // Init

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        const ParamInfo& info = getParamInfo(index);

        parameter.hints = kParameterIsAutomatable;
        parameter.name = info.name;
        parameter.symbol = info.symbol;
        parameter.unit = info.unit;
        parameter.ranges.min = info.min;
        parameter.ranges.max = info.max;
        parameter.ranges.def = info.def;

        if (info.shape == ParamShape::Logarithmic)
            parameter.hints |= kParameterIsLogarithmic;

        switch (index)
        {
        case kParamWaveform:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[3] = {
                    { 0.0f, "Saw" },
                    { 1.0f, "Pulse" },
                    { 2.0f, "Triangle" },
                };
                parameter.enumValues.count = 3;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamSubOctave:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[4] = {
                    { -2.0f, "-2" },
                    { -1.0f, "-1" },
                    {  1.0f, "+1" },
                    {  2.0f, "+2" },
                };
                parameter.enumValues.count = 4;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamFilterMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[6] = {
                    { 0.0f, "LP 12dB" },
                    { 1.0f, "LP 24dB" },
                    { 2.0f, "HP 12dB" },
                    { 3.0f, "HP 24dB" },
                    { 4.0f, "BP 12dB" },
                    { 5.0f, "Ladder 24dB" },
                };
                parameter.enumValues.count = 6;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        // no longer a live playback waveform - see the enum comment in
        // Params.hpp. Selecting/automating one of these regenerates the
        // step sequencer's values via fillLfoStepPreset() in setParameterValue().
        case kParamLfoWaveform:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[4] = {
                    { 0.0f, "Sine" },
                    { 1.0f, "Saw" },
                    { 2.0f, "Square" },
                    { 3.0f, "Gate" },
                };
                parameter.enumValues.count = 4;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamLfoDestination:
        case kParamLfo2Destination:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[4] = {
                    { 0.0f, "Pitch" },
                    { 1.0f, "Cutoff" },
                    { 2.0f, "Amplitude" },
                    { 3.0f, "Pulse Width" },
                };
                parameter.enumValues.count = 4;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamLfo2Waveform:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[4] = {
                    { 0.0f, "Sine" },
                    { 1.0f, "Saw" },
                    { 2.0f, "Square" },
                    { 3.0f, "Gate" },
                };
                parameter.enumValues.count = 4;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamLfoStepCount:
        case kParamLfo2StepCount:
            parameter.hints |= kParameterIsInteger;
            break;

        case kParamLfoSync:
        case kParamLfo2Sync:
        case kParamArpSync:
            parameter.hints |= kParameterIsInteger;
            parameter.enumValues.count = 17;
            parameter.enumValues.restrictedMode = true;
            parameter.enumValues.values = kSyncEnumValues;
            parameter.enumValues.deleteLater = false;
            break;

        case kParamArpEnabled:
            parameter.hints |= kParameterIsInteger | kParameterIsBoolean;
            break;

        case kParamArpPattern:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[4] = {
                    { 0.0f, "Up" },
                    { 1.0f, "Down" },
                    { 2.0f, "Up/Down" },
                    { 3.0f, "Random" },
                };
                parameter.enumValues.count = 4;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamArpOctaves:
            parameter.hints |= kParameterIsInteger;
            break;

        case kParamPitchBendRange:
            parameter.hints |= kParameterIsInteger;
            break;

        case kParamModWheelDestination:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[4] = {
                    { 0.0f, "Off" },
                    { 1.0f, "Vibrato" },
                    { 2.0f, "Cutoff" },
                    { 3.0f, "Volume" },
                };
                parameter.enumValues.count = 4;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamGlideMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[2] = {
                    { 0.0f, "Legato" },
                    { 1.0f, "Always" },
                };
                parameter.enumValues.count = 2;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamVoiceMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[2] = {
                    { 0.0f, "Poly" },
                    { 1.0f, "Mono" },
                };
                parameter.enumValues.count = 2;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        default:
            break;
        }
    }

    // ---------------------------------------------------------------------
    // Internal data

    float getParameterValue(uint32_t index) const override
    {
        if (index >= kParamLfoStep1 && index <= kParamLfoStep16)
            return fParams.lfoSteps[index - kParamLfoStep1];
        if (index >= kParamLfo2Step1 && index <= kParamLfo2Step16)
            return fParams.lfo2Steps[index - kParamLfo2Step1];

        switch (index)
        {
        case kParamWaveform:            return (float)fParams.waveform;
        case kParamPulseWidth:          return fParams.pulseWidth * 100.0f;
        case kParamSubOctave:           return fParams.subOctave;
        case kParamSubLevel:            return fParams.subLevel;
        case kParamFilterMode:          return (float)fParams.filterMode;
        case kParamFilterCutoff:        return fParams.filterCutoff;
        case kParamFilterResonance:     return fParams.filterResonance;
        case kParamFilterDrive:         return fParams.filterDrive;
        case kParamFilterEnvAmount:     return fParams.filterEnvAmount;
        case kParamAmpAttack:           return fParams.ampAttack;
        case kParamAmpDecay:            return fParams.ampDecay;
        case kParamAmpSustain:          return fParams.ampSustain;
        case kParamAmpRelease:          return fParams.ampRelease;
        case kParamAmpCurve:            return fParams.ampCurve;
        case kParamVelocitySensitivity: return fParams.velocitySensitivity;
        case kParamFilterAttack:        return fParams.filterAttack;
        case kParamFilterDecay:         return fParams.filterDecay;
        case kParamFilterSustain:       return fParams.filterSustain;
        case kParamFilterRelease:       return fParams.filterRelease;
        case kParamFilterEnvCurve:      return fParams.filterCurve;
        case kParamMasterVolume:        return fMasterVolume;
        case kParamLfoWaveform:         return fLfoPresetTag;
        case kParamLfoRateHz:           return fLfoRateHz;
        case kParamLfoSync:             return fLfoSync;
        case kParamLfoDestination:      return (float)fParams.lfoDestination;
        case kParamLfoAmount:           return fParams.lfoAmount;
        case kParamLfoStepCount:        return (float)fParams.lfoStepCount;
        case kParamArpEnabled:          return fArpEnabled ? 1.0f : 0.0f;
        case kParamArpPattern:          return (float)fArpPattern;
        case kParamArpOctaves:          return (float)fArpOctaves;
        case kParamArpRateHz:           return fArpRateHz;
        case kParamArpSync:             return fArpSync;
        case kParamArpGateLength:       return fArpGateLength * 100.0f;
        case kParamVoiceMode:           return fMonoMode ? 1.0f : 0.0f;
        case kParamPortamentoTime:      return fParams.portamentoTime;
        case kParamPitchBendRange:      return fPitchBendRangeSemitones;
        case kParamModWheelDestination: return (float)fParams.modWheelDestination;
        case kParamGlideMode:           return (float)fGlideMode;
        case kParamModWheelAmount:      return fParams.modWheelAmount;
        case kParamLfo2Waveform:        return fLfo2PresetTag;
        case kParamLfo2RateHz:          return fLfo2RateHz;
        case kParamLfo2Sync:            return fLfo2Sync;
        case kParamLfo2Destination:     return (float)fParams.lfo2Destination;
        case kParamLfo2Amount:          return fParams.lfo2Amount;
        case kParamLfo2StepCount:       return (float)fParams.lfo2StepCount;
        default:                        return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index >= kParamLfoStep1 && index <= kParamLfoStep16)
        {
            fParams.lfoSteps[index - kParamLfoStep1] = value;
            for (sideous::Voice& voice : fVoices)
                voice.applyParams(fParams);
            fMonoVoice.applyParams(fParams);
            return;
        }
        if (index >= kParamLfo2Step1 && index <= kParamLfo2Step16)
        {
            fParams.lfo2Steps[index - kParamLfo2Step1] = value;
            for (sideous::Voice& voice : fVoices)
                voice.applyParams(fParams);
            fMonoVoice.applyParams(fParams);
            return;
        }

        switch (index)
        {
        case kParamWaveform:
            fParams.waveform = value < 0.5f ? sideous::Waveform::Saw
                              : value < 1.5f ? sideous::Waveform::Pulse
                                              : sideous::Waveform::Triangle;
            break;
        case kParamPulseWidth:      fParams.pulseWidth = value * 0.01f; break;
        case kParamSubOctave:       fParams.subOctave = value; break;
        case kParamSubLevel:        fParams.subLevel = value; break;
        case kParamFilterMode:      fParams.filterMode = (sideous::FilterMode)(int)(value + 0.5f); break;
        case kParamFilterCutoff:    fParams.filterCutoff = value; break;
        case kParamFilterResonance: fParams.filterResonance = value; break;
        case kParamFilterDrive:     fParams.filterDrive = value; break;
        case kParamFilterEnvAmount: fParams.filterEnvAmount = value; break;
        case kParamAmpAttack:       fParams.ampAttack = value; break;
        case kParamAmpDecay:        fParams.ampDecay = value; break;
        case kParamAmpSustain:      fParams.ampSustain = value; break;
        case kParamAmpRelease:      fParams.ampRelease = value; break;
        case kParamAmpCurve:        fParams.ampCurve = value; break;
        case kParamVelocitySensitivity: fParams.velocitySensitivity = value; break;
        case kParamFilterAttack:    fParams.filterAttack = value; break;
        case kParamFilterDecay:     fParams.filterDecay = value; break;
        case kParamFilterSustain:   fParams.filterSustain = value; break;
        case kParamFilterRelease:   fParams.filterRelease = value; break;
        case kParamFilterEnvCurve:  fParams.filterCurve = value; break;
        case kParamMasterVolume:    fMasterVolume = value; break;
        case kParamLfoWaveform:
            fLfoPresetTag = value;
            sideous::fillLfoStepPreset((sideous::LfoStepPreset)(int)(value + 0.5f), fParams.lfoStepCount, fParams.lfoSteps);
            break;
        case kParamLfoRateHz:       fLfoRateHz = value; break;
        case kParamLfoSync:         fLfoSync = value; break;
        case kParamLfoDestination:  fParams.lfoDestination = (sideous::LfoDestination)(int)(value + 0.5f); break;
        case kParamLfoAmount:       fParams.lfoAmount = value; break;
        case kParamLfoStepCount:    fParams.lfoStepCount = (int)(value + 0.5f); break;
        case kParamArpEnabled:
            fArpEnabled = value >= 0.5f;
            if (!fArpEnabled)
            {
                // don't leave a note stuck on, or stale notes held, when
                // switching the arpeggiator off
                stopArpNote();
                fHeldCount = 0;
            }
            break;
        case kParamArpPattern:      fArpPattern = (int)(value + 0.5f); break;
        case kParamArpOctaves:      fArpOctaves = (int)(value + 0.5f); break;
        case kParamArpRateHz:       fArpRateHz = value; break;
        case kParamArpSync:         fArpSync = value; break;
        case kParamArpGateLength:   fArpGateLength = value * 0.01f; break;
        case kParamVoiceMode:
            {
                const bool newMono = value >= 0.5f;
                if (newMono != fMonoMode)
                {
                    // don't leave orphaned voices ringing when switching modes
                    for (sideous::Voice& voice : fVoices)
                        voice.kill();
                    fMonoVoice.kill();
                    fMonoStackCount = 0;
                }
                fMonoMode = newMono;
            }
            break;
        case kParamPortamentoTime:  fParams.portamentoTime = value; break;
        case kParamPitchBendRange:  fPitchBendRangeSemitones = value; break;
        case kParamModWheelDestination:
            fParams.modWheelDestination = (sideous::ModWheelDestination)(int)(value + 0.5f);
            break;
        case kParamGlideMode:       fGlideMode = (int)(value + 0.5f); break;
        case kParamModWheelAmount:  fParams.modWheelAmount = value; break;
        case kParamLfo2Waveform:
            fLfo2PresetTag = value;
            sideous::fillLfoStepPreset((sideous::LfoStepPreset)(int)(value + 0.5f), fParams.lfo2StepCount, fParams.lfo2Steps);
            break;
        case kParamLfo2RateHz:      fLfo2RateHz = value; break;
        case kParamLfo2Sync:        fLfo2Sync = value; break;
        case kParamLfo2Destination: fParams.lfo2Destination = (sideous::LfoDestination)(int)(value + 0.5f); break;
        case kParamLfo2Amount:      fParams.lfo2Amount = value; break;
        case kParamLfo2StepCount:   fParams.lfo2StepCount = (int)(value + 0.5f); break;
        default: return;
        }

        for (sideous::Voice& voice : fVoices)
            voice.applyParams(fParams);
        fMonoVoice.applyParams(fParams);
    }

    // ---------------------------------------------------------------------
    // Audio/MIDI Processing

    void activate() override
    {
        sampleRateChanged(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fSampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        for (sideous::Voice& voice : fVoices)
        {
            voice.setSampleRate(newSampleRate);
            voice.applyParams(fParams);
        }
        fMonoVoice.setSampleRate(newSampleRate);
        fMonoVoice.applyParams(fParams);
    }

    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        float* outL = outputs[0];
        float* outR = outputs[1];

        const TimePosition& timePos = getTimePosition();
        const double bpm = timePos.bbt.valid && timePos.bbt.beatsPerMinute > 0.0
                          ? timePos.bbt.beatsPerMinute : 120.0;

        const float lfoHz = syncedHz(fLfoSync, fLfoRateHz, bpm);
        const float lfo2Hz = syncedHz(fLfo2Sync, fLfo2RateHz, bpm);
        for (sideous::Voice& voice : fVoices)
        {
            voice.setLfoFrequency(lfoHz);
            voice.setLfo2Frequency(lfo2Hz);
        }
        fMonoVoice.setLfoFrequency(lfoHz);
        fMonoVoice.setLfo2Frequency(lfo2Hz);

        const float arpHz = syncedHz(fArpSync, fArpRateHz, bpm);
        const float arpStepDuration = arpHz > 0.0f ? 1.0f / arpHz : 0.25f;
        const float arpGateDuration = arpStepDuration * fArpGateLength;
        const float sampleDt = (float)(1.0 / fSampleRate);

        uint32_t nextEvent = 0;

        for (uint32_t frame = 0; frame < frames; ++frame)
        {
            bool controllersChanged = false;
            while (nextEvent < midiEventCount && midiEvents[nextEvent].frame == frame)
            {
                const uint8_t statusBefore = midiEvents[nextEvent].data[0] & 0xF0;
                if (statusBefore == 0xE0 || statusBefore == 0xB0)
                    controllersChanged = true;
                handleMidiEvent(midiEvents[nextEvent]);
                ++nextEvent;
            }

            if (controllersChanged)
            {
                const float pitchBendSemitones = fPitchBendNormalized * fPitchBendRangeSemitones;
                for (sideous::Voice& voice : fVoices)
                {
                    voice.setPitchBend(pitchBendSemitones);
                    voice.setModWheel(fModWheelValue);
                }
                fMonoVoice.setPitchBend(pitchBendSemitones);
                fMonoVoice.setModWheel(fModWheelValue);
            }

            if (fArpEnabled)
                advanceArp(sampleDt, arpStepDuration, arpGateDuration);

            float mix = 0.0f;
            for (sideous::Voice& voice : fVoices)
                mix += voice.process();
            mix += fMonoVoice.process();

            mix *= fMasterVolume * kVoiceHeadroom;

            // gentle safety saturation: a resonant filter near self-oscillation
            // (summed across many voices) can otherwise blow well past 0dBFS
            mix = std::tanh(mix);

            outL[frame] = mix;
            outR[frame] = mix;
        }

        // catch any remaining events stamped past the last frame (shouldn't happen, but be safe)
        while (nextEvent < midiEventCount)
            handleMidiEvent(midiEvents[nextEvent++]);
    }

private:
    static constexpr const float kVoiceHeadroom = 1.0f / 4.0f;

    void handleMidiEvent(const MidiEvent& event) noexcept
    {
        if (event.size < 2 || event.size > 3)
            return;

        const uint8_t status = event.data[0] & 0xF0;
        const uint8_t note = event.data[1];
        const uint8_t velocity = event.size > 2 ? event.data[2] : 0;

        if (status == 0x90 && velocity > 0)
        {
            if (fArpEnabled)
                addHeldNote(note, velocity);
            else if (fMonoMode)
                monoNoteOn(note, velocity);
            else
                triggerVoiceNoteOn(note, velocity);
        }
        else if (status == 0x80 || (status == 0x90 && velocity == 0))
        {
            if (fArpEnabled)
                removeHeldNote(note);
            else if (fMonoMode)
                monoNoteOff(note);
            else
                triggerVoiceNoteOff(note);
        }
        else if (status == 0xE0 && event.size >= 3) // pitch bend, 14-bit, center = 8192
        {
            const int raw = (int)event.data[1] | ((int)event.data[2] << 7);
            float normalized = ((float)raw - 8192.0f) / 8192.0f;
            fPitchBendNormalized = normalized < -1.0f ? -1.0f : (normalized > 1.0f ? 1.0f : normalized);
        }
        else if (status == 0xB0 && event.size >= 3 && event.data[1] == 1) // CC1 = mod wheel
        {
            fModWheelValue = (float)event.data[2] / 127.0f;
        }
    }

    // ---------------------------------------------------------------------
    // voice allocation (used directly when the arp is off, and by the arp
    // engine itself to sound its generated notes). Transparently routes to
    // the single mono voice when mono mode is active: retriggerEnvelope
    // becomes false whenever that voice is already sounding, which is
    // correct for the arp's own note stream (short gate = re-attack,
    // legato-length gate = smooth glide between steps). Direct MIDI input in
    // mono mode does NOT come through here for note-on - see monoNoteOn(),
    // which needs the real held-key stack to get retriggering right.

    // computes the glideFromFreq sentinel for Voice::noteOn() from the
    // current Glide Mode: Always glides from whatever note last played,
    // Legato leaves it to Voice's own "only if already active" default.
    float glideSourceForNextNote() const noexcept
    {
        if (fGlideMode == 1) // Always
            return fHasLastNoteFreq ? fLastNoteFreq : 0.0f;
        return -1.0f; // Legato: auto
    }

    void triggerVoiceNoteOn(int note, int velocity) noexcept
    {
        if (fMonoMode)
        {
            const bool retrigger = !fMonoVoice.isActive();
            fMonoVoice.applyParams(fParams);
            fMonoVoice.noteOn(note, (float)velocity / 127.0f, retrigger, glideSourceForNextNote());
            fLastNoteFreq = noteToHz(note);
            fHasLastNoteFreq = true;
            return;
        }

        sideous::Voice* target = nullptr;

        for (sideous::Voice& voice : fVoices)
        {
            if (!voice.isActive())
            {
                target = &voice;
                break;
            }
        }

        if (target == nullptr)
        {
            for (sideous::Voice& voice : fVoices)
            {
                if (voice.isReleasing())
                {
                    target = &voice;
                    break;
                }
            }
        }

        if (target == nullptr)
            target = &fVoices[fStealCursor++ % kNumVoices];

        target->applyParams(fParams);
        target->noteOn(note, (float)velocity / 127.0f, true, glideSourceForNextNote());

        fLastNoteFreq = noteToHz(note);
        fHasLastNoteFreq = true;
    }

    void triggerVoiceNoteOff(int note) noexcept
    {
        if (fMonoMode)
        {
            if (fMonoVoice.getNote() == note)
                fMonoVoice.noteOff();
            return;
        }

        for (sideous::Voice& voice : fVoices)
        {
            if (voice.isActive() && voice.getNote() == note && !voice.isReleasing())
                voice.noteOff();
        }
    }

    // ---------------------------------------------------------------------
    // mono mode note stack: tracks which real keys are physically held so
    // releasing the current note can fall back to the previous one, like a
    // classic analog mono synth (only used for direct MIDI input, i.e. when
    // the arpeggiator isn't the one driving notes).
    //
    // Retriggering is decided from the stack transition (0 held -> 1 held),
    // NOT from whether the voice is still audible. A voice can still be
    // ringing out a long release well after its key was let go; using
    // isActive() there would treat a fresh staccato attack as a legato
    // continuation and just keep fading out the old envelope instead of
    // re-attacking, which is silent/broken for anything but very short
    // release times.

    void monoNoteOn(int note, int velocity) noexcept
    {
        for (uint32_t i = 0; i < fMonoStackCount; ++i)
        {
            if (fMonoStack[i].note == note)
            {
                removeMonoStackIndex(i);
                break;
            }
        }

        const bool wasEmpty = fMonoStackCount == 0;
        if (fMonoStackCount < kMaxHeldNotes)
            fMonoStack[fMonoStackCount++] = { note, velocity };

        const bool retrigger = wasEmpty;
        float glideFrom;
        if (fGlideMode == 1) // Always
            glideFrom = fHasLastNoteFreq ? fLastNoteFreq : 0.0f;
        else // Legato: glide only on genuine overlap (stack wasn't empty)
            glideFrom = wasEmpty ? 0.0f : -1.0f;

        fMonoVoice.applyParams(fParams);
        fMonoVoice.noteOn(note, (float)velocity / 127.0f, retrigger, glideFrom);

        fLastNoteFreq = noteToHz(note);
        fHasLastNoteFreq = true;
    }

    void monoNoteOff(int note) noexcept
    {
        for (uint32_t i = 0; i < fMonoStackCount; ++i)
        {
            if (fMonoStack[i].note == note)
            {
                removeMonoStackIndex(i);
                break;
            }
        }

        if (fMonoStackCount == 0)
            fMonoVoice.noteOff();
        else
            triggerVoiceNoteOn(fMonoStack[fMonoStackCount - 1].note, fMonoStack[fMonoStackCount - 1].velocity);
    }

    void removeMonoStackIndex(uint32_t index) noexcept
    {
        for (uint32_t j = index; j < fMonoStackCount - 1; ++j)
            fMonoStack[j] = fMonoStack[j + 1];
        --fMonoStackCount;
    }

    // ---------------------------------------------------------------------
    // arpeggiator

    struct HeldNote { int note; int velocity; };

    void addHeldNote(int note, int velocity) noexcept
    {
        for (uint32_t i = 0; i < fHeldCount; ++i)
        {
            if (fHeldNotes[i].note == note)
            {
                fHeldNotes[i].velocity = velocity;
                return;
            }
        }

        if (fHeldCount >= kMaxHeldNotes)
            return;

        if (fHeldCount == 0)
        {
            // Start the arp shortly after the first note of a new phrase,
            // instead of literally on the same sample it arrives: a chord
            // played "simultaneously" still lands as separate MIDI events a
            // few ms apart, and triggering step 0 immediately would lock in
            // whichever note happened to arrive first (with totalSteps==1)
            // before the rest of the chord is in fHeldNotes, scrambling the
            // Up/Down/UpDown sequence for the whole held phrase afterwards.
            // A short grace window lets the rest of the chord register first.
            fArpForceRestart = true;
            fArpForceRestartDelay = kArpChordGraceSeconds;
        }

        // keep the list sorted by pitch (ascending), not MIDI arrival order,
        // so Up/Down/etc. reflect the actual chord shape regardless of which
        // order the keys were physically pressed in
        uint32_t insertPos = fHeldCount;
        for (uint32_t i = 0; i < fHeldCount; ++i)
        {
            if (fHeldNotes[i].note > note)
            {
                insertPos = i;
                break;
            }
        }
        for (uint32_t j = fHeldCount; j > insertPos; --j)
            fHeldNotes[j] = fHeldNotes[j - 1];
        fHeldNotes[insertPos] = { note, velocity };
        ++fHeldCount;
    }

    void removeHeldNote(int note) noexcept
    {
        for (uint32_t i = 0; i < fHeldCount; ++i)
        {
            if (fHeldNotes[i].note == note)
            {
                for (uint32_t j = i; j < fHeldCount - 1; ++j)
                    fHeldNotes[j] = fHeldNotes[j + 1];
                --fHeldCount;
                return;
            }
        }
    }

    static uint32_t xorshift32(uint32_t& state) noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    void advanceArp(float dt, float stepDuration, float gateDuration) noexcept
    {
        if (fHeldCount == 0)
        {
            stopArpNote();
            fArpPhase = 0.0f;
            fArpForceRestart = false;
            return;
        }

        if (fArpForceRestart)
        {
            fArpForceRestartDelay -= dt;
            if (fArpForceRestartDelay > 0.0f)
                return; // still gathering a possibly-simultaneous chord

            fArpForceRestart = false;
            fArpPhase = stepDuration;
            fArpStepCounter = 0;
        }
        else
        {
            fArpPhase += dt;
        }

        if (fArpNoteOn && fArpPhase >= gateDuration)
            stopArpNote();

        if (fArpPhase >= stepDuration)
        {
            fArpPhase -= stepDuration;
            startNextArpNote();
        }
    }

    void startNextArpNote() noexcept
    {
        stopArpNote();

        const int totalSteps = (int)fHeldCount * fArpOctaves;
        if (totalSteps <= 0)
            return;

        int seqPos;
        switch ((ArpPattern)fArpPattern)
        {
        case ArpPattern::Down:
            seqPos = (totalSteps - 1) - (int)(fArpStepCounter % (uint32_t)totalSteps);
            break;

        case ArpPattern::UpDown:
        {
            const int pingLen = totalSteps <= 1 ? totalSteps : (2 * totalSteps - 2);
            const int pos = pingLen > 0 ? (int)(fArpStepCounter % (uint32_t)pingLen) : 0;
            seqPos = pos < totalSteps ? pos : pingLen - pos;
            break;
        }

        case ArpPattern::Random:
            seqPos = (int)(xorshift32(fArpRngState) % (uint32_t)totalSteps);
            break;

        case ArpPattern::Up:
        default:
            seqPos = (int)(fArpStepCounter % (uint32_t)totalSteps);
            break;
        }

        const int noteIndex = seqPos % (int)fHeldCount;
        const int octave = seqPos / (int)fHeldCount;
        int midiNote = fHeldNotes[noteIndex].note + octave * 12;
        midiNote = midiNote < 0 ? 0 : (midiNote > 127 ? 127 : midiNote);

        fArpCurrentNote = midiNote;
        triggerVoiceNoteOn(midiNote, fHeldNotes[noteIndex].velocity);
        fArpNoteOn = true;

        ++fArpStepCounter;
    }

    void stopArpNote() noexcept
    {
        if (fArpNoteOn)
        {
            triggerVoiceNoteOff(fArpCurrentNote);
            fArpNoteOn = false;
        }
    }

    std::array<sideous::Voice, kNumVoices> fVoices;
    sideous::VoiceParams fParams;
    float fMasterVolume = 0.8f;
    uint32_t fStealCursor = 0;
    double fSampleRate = 44100.0;
    float fLastNoteFreq = 0.0f;    // poly portamento glide source, see triggerVoiceNoteOn()
    bool fHasLastNoteFreq = false;
    int fGlideMode = 1;             // 0 = Legato, 1 = Always (see kSyncEnumValues-style table)

    float fPitchBendNormalized = 0.0f; // -1..1, from MIDI pitch bend (0xE0)
    float fPitchBendRangeSemitones = 2.0f;
    float fModWheelValue = 0.0f;       // 0..1, from MIDI CC1

    float fLfoRateHz = 2.0f;
    float fLfoSync = 0.0f; // 0 = free, see kSyncEnumValues
    float fLfo2RateHz = 3.0f;
    float fLfo2Sync = 0.0f; // 0 = free, see kSyncEnumValues

    // "last preset applied" echo for kParamLfoWaveform/kParamLfo2Waveform -
    // no longer read by the DSP directly, see the enum comment in Params.hpp
    float fLfoPresetTag = 0.0f;
    float fLfo2PresetTag = 0.0f;

    bool fArpEnabled = false;
    int fArpPattern = 0;
    int fArpOctaves = 1;
    float fArpRateHz = 4.0f;
    float fArpSync = 12.0f; // default 1/16, see kSyncEnumValues
    float fArpGateLength = 0.7f;

    std::array<HeldNote, kMaxHeldNotes> fHeldNotes;
    uint32_t fHeldCount = 0;
    uint32_t fArpStepCounter = 0;
    uint32_t fArpRngState = 0x9e3779b9u;
    float fArpPhase = 0.0f;
    int fArpCurrentNote = -1;
    bool fArpNoteOn = false;
    bool fArpForceRestart = false;
    float fArpForceRestartDelay = 0.0f;

    bool fMonoMode = false;
    sideous::Voice fMonoVoice;
    std::array<HeldNote, kMaxHeldNotes> fMonoStack;
    uint32_t fMonoStackCount = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideousPlugin)
};

// -----------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new SideousPlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
