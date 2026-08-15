/*
 * Sideous - shared parameter definitions used by both the DSP plugin side
 * and the UI, so ranges/units/labels can't drift between the two.
 */

#pragma once

#include <cstdint>

namespace sideous {

enum Params : uint32_t {
    kParamWaveform = 0,
    kParamPulseWidth,
    kParamSubOctave,
    kParamSubLevel,
    kParamFilterMode,
    kParamFilterCutoff,
    kParamFilterResonance,
    kParamFilterDrive,
    kParamFilterEnvAmount,
    kParamAmpAttack,
    kParamAmpDecay,
    kParamAmpSustain,
    kParamAmpRelease,
    kParamAmpCurve,
    kParamVelocitySensitivity,
    kParamFilterAttack,
    kParamFilterDecay,
    kParamFilterSustain,
    kParamFilterRelease,
    kParamFilterEnvCurve,
    kParamMasterVolume,
    kParamLfoWaveform,
    kParamLfoRateHz,
    kParamLfoSync,
    kParamLfoDestination,
    kParamLfoAmount,
    kParamArpEnabled,
    kParamArpPattern,
    kParamArpOctaves,
    kParamArpRateHz,
    kParamArpSync,
    kParamArpGateLength,
    kParamVoiceMode,
    kParamPortamentoTime,
    kParamPitchBendRange,
    kParamModWheelDestination,
    kParamGlideMode,
    kParamModWheelAmount,

    // second, independent LFO - same waveform/destination/sync/amount shape
    // as LFO1, so e.g. LFO1->Pulse Width and LFO2->Pitch (vibrato) can run
    // at the same time instead of competing for the one destination slot.
    // Appended rather than inserted so existing presets/automation for the
    // params above keep their indices.
    kParamLfo2Waveform,
    kParamLfo2RateHz,
    kParamLfo2Sync,
    kParamLfo2Destination,
    kParamLfo2Amount,

    // step-sequencer data for LFO1/LFO2 (see dsp/LFO.hpp: LFO::setSteps(),
    // fillLfoStepPreset()). kParamLfoWaveform/kParamLfo2Waveform above are
    // no longer a live playback mode - they're a "last preset applied"
    // trigger that fills these step values. Appended at the end so the
    // existing indices above stay stable for old presets/automation.
    kParamLfoStepCount,
    kParamLfoStep1, kParamLfoStep2, kParamLfoStep3, kParamLfoStep4,
    kParamLfoStep5, kParamLfoStep6, kParamLfoStep7, kParamLfoStep8,
    kParamLfoStep9, kParamLfoStep10, kParamLfoStep11, kParamLfoStep12,
    kParamLfoStep13, kParamLfoStep14, kParamLfoStep15, kParamLfoStep16,

    kParamLfo2StepCount,
    kParamLfo2Step1, kParamLfo2Step2, kParamLfo2Step3, kParamLfo2Step4,
    kParamLfo2Step5, kParamLfo2Step6, kParamLfo2Step7, kParamLfo2Step8,
    kParamLfo2Step9, kParamLfo2Step10, kParamLfo2Step11, kParamLfo2Step12,
    kParamLfo2Step13, kParamLfo2Step14, kParamLfo2Step15, kParamLfo2Step16,

    kParamCount
};

enum class ParamShape { Linear, Logarithmic };

struct ParamInfo
{
    const char* name;
    const char* symbol;
    const char* unit;
    float min;
    float max;
    float def;
    ParamShape shape;
};

inline const ParamInfo& getParamInfo(uint32_t index) noexcept
{
    static constexpr ParamInfo table[kParamCount] = {
        { "Waveform",         "waveform",           "",   0.0f,     2.0f,     0.0f,   ParamShape::Linear },
        { "Pulse Width",      "pulse_width",         "%",  1.0f,    99.0f,    50.0f,   ParamShape::Linear },
        { "Sub Octave",       "sub_octave",          "",  -2.0f,     2.0f,    -1.0f,   ParamShape::Linear },
        { "Sub Level",        "sub_level",           "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Filter Mode",      "filter_mode",         "",   0.0f,     5.0f,     0.0f,   ParamShape::Linear },
        { "Filter Cutoff",    "filter_cutoff",       "Hz", 20.0f, 20000.0f, 2000.0f,   ParamShape::Logarithmic },
        { "Filter Resonance", "filter_resonance",    "",   0.0f,     1.0f,     0.2f,   ParamShape::Linear },
        { "Filter Drive",     "filter_drive",        "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Filter Env Amount","filter_env_amount",   "",  -1.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Amp Attack",       "amp_attack",          "s",  0.001f,   5.0f,     0.005f, ParamShape::Logarithmic },
        { "Amp Decay",        "amp_decay",           "s",  0.001f,   5.0f,     0.1f,   ParamShape::Logarithmic },
        { "Amp Sustain",      "amp_sustain",         "",   0.0f,     1.0f,     0.8f,   ParamShape::Linear },
        { "Amp Release",      "amp_release",         "s",  0.001f,   8.0f,     0.2f,   ParamShape::Logarithmic },
        { "Amp Env Curve",    "amp_curve",           "",   0.0f,     1.0f,     0.35f,  ParamShape::Linear },
        { "Velocity Sens",    "velocity_sens",       "",   0.0f,     1.0f,     1.0f,   ParamShape::Linear },
        { "Filter Attack",    "filter_attack",       "s",  0.001f,   5.0f,     0.005f, ParamShape::Logarithmic },
        { "Filter Decay",     "filter_decay",        "s",  0.001f,   5.0f,     0.2f,   ParamShape::Logarithmic },
        { "Filter Sustain",   "filter_sustain",      "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Filter Release",   "filter_release",      "s",  0.001f,   8.0f,     0.2f,   ParamShape::Logarithmic },
        { "Filter Env Curve", "filter_env_curve",    "",   0.0f,     1.0f,     0.35f,  ParamShape::Linear },
        { "Master Volume",    "master_volume",       "",   0.0f,     1.0f,     0.8f,   ParamShape::Linear },
        { "LFO Waveform",     "lfo_waveform",        "",   0.0f,     2.0f,     0.0f,   ParamShape::Linear },
        { "LFO Rate",         "lfo_rate_hz",         "Hz", 0.02f,   20.0f,     2.0f,   ParamShape::Logarithmic },
        { "LFO Sync",         "lfo_sync",            "",   0.0f,    16.0f,     0.0f,   ParamShape::Linear },
        { "LFO Destination",  "lfo_destination",     "",   0.0f,     3.0f,     0.0f,   ParamShape::Linear },
        { "LFO Amount",       "lfo_amount",          "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Arp Enabled",      "arp_enabled",         "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Arp Pattern",      "arp_pattern",         "",   0.0f,     3.0f,     0.0f,   ParamShape::Linear },
        { "Arp Octaves",      "arp_octaves",         "",   1.0f,     4.0f,     1.0f,   ParamShape::Linear },
        { "Arp Rate",         "arp_rate_hz",         "Hz", 0.5f,    20.0f,     4.0f,   ParamShape::Logarithmic },
        { "Arp Sync",         "arp_sync",            "",   0.0f,    16.0f,    12.0f,   ParamShape::Linear },
        { "Arp Gate Length",  "arp_gate_length",     "%",  10.0f,  100.0f,    70.0f,   ParamShape::Linear },
        { "Voice Mode",       "voice_mode",          "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Portamento",       "portamento",          "s",  0.0f,     3.0f,     0.0f,   ParamShape::Linear },
        { "Pitch Bend Range", "pitch_bend_range",    "st", 1.0f,    24.0f,     2.0f,   ParamShape::Linear },
        { "Mod Wheel Dest",   "mod_wheel_dest",      "",   0.0f,     3.0f,     0.0f,   ParamShape::Linear },
        { "Glide Mode",       "glide_mode",          "",   0.0f,     1.0f,     1.0f,   ParamShape::Linear },
        { "Mod Wheel Amount", "mod_wheel_amount",    "",   0.0f,     1.0f,     0.5f,   ParamShape::Linear },

        { "LFO2 Waveform",    "lfo2_waveform",       "",   0.0f,     2.0f,     0.0f,   ParamShape::Linear },
        { "LFO2 Rate",        "lfo2_rate_hz",        "Hz", 0.02f,   20.0f,     3.0f,   ParamShape::Logarithmic },
        { "LFO2 Sync",        "lfo2_sync",           "",   0.0f,    16.0f,     0.0f,   ParamShape::Linear },
        { "LFO2 Destination", "lfo2_destination",    "",   0.0f,     3.0f,     1.0f,   ParamShape::Linear },
        { "LFO2 Amount",      "lfo2_amount",         "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },

        // step-sequencer data - default step count 16, default shape a full
        // 16-point sine cycle, so the plugin ships with an obvious, fully
        // populated shape rather than silence. See kParamLfoStepCount's
        // comment in the enum above for why kParamLfoWaveform no longer
        // drives playback directly.
        { "LFO Step Count",   "lfo_step_count",      "",   1.0f,    16.0f,    16.0f,  ParamShape::Linear },
        { "LFO Step 1",       "lfo_step_1",          "",  -1.0f,     1.0f,     0.0000f, ParamShape::Linear },
        { "LFO Step 2",       "lfo_step_2",          "",  -1.0f,     1.0f,     0.3827f, ParamShape::Linear },
        { "LFO Step 3",       "lfo_step_3",          "",  -1.0f,     1.0f,     0.7071f, ParamShape::Linear },
        { "LFO Step 4",       "lfo_step_4",          "",  -1.0f,     1.0f,     0.9239f, ParamShape::Linear },
        { "LFO Step 5",       "lfo_step_5",          "",  -1.0f,     1.0f,     1.0000f, ParamShape::Linear },
        { "LFO Step 6",       "lfo_step_6",          "",  -1.0f,     1.0f,     0.9239f, ParamShape::Linear },
        { "LFO Step 7",       "lfo_step_7",          "",  -1.0f,     1.0f,     0.7071f, ParamShape::Linear },
        { "LFO Step 8",       "lfo_step_8",          "",  -1.0f,     1.0f,     0.3827f, ParamShape::Linear },
        { "LFO Step 9",       "lfo_step_9",          "",  -1.0f,     1.0f,     0.0000f, ParamShape::Linear },
        { "LFO Step 10",      "lfo_step_10",         "",  -1.0f,     1.0f,    -0.3827f, ParamShape::Linear },
        { "LFO Step 11",      "lfo_step_11",         "",  -1.0f,     1.0f,    -0.7071f, ParamShape::Linear },
        { "LFO Step 12",      "lfo_step_12",         "",  -1.0f,     1.0f,    -0.9239f, ParamShape::Linear },
        { "LFO Step 13",      "lfo_step_13",         "",  -1.0f,     1.0f,    -1.0000f, ParamShape::Linear },
        { "LFO Step 14",      "lfo_step_14",         "",  -1.0f,     1.0f,    -0.9239f, ParamShape::Linear },
        { "LFO Step 15",      "lfo_step_15",         "",  -1.0f,     1.0f,    -0.7071f, ParamShape::Linear },
        { "LFO Step 16",      "lfo_step_16",         "",  -1.0f,     1.0f,    -0.3827f, ParamShape::Linear },

        { "LFO2 Step Count",  "lfo2_step_count",     "",   1.0f,    16.0f,    16.0f,  ParamShape::Linear },
        { "LFO2 Step 1",      "lfo2_step_1",         "",  -1.0f,     1.0f,     0.0000f, ParamShape::Linear },
        { "LFO2 Step 2",      "lfo2_step_2",         "",  -1.0f,     1.0f,     0.3827f, ParamShape::Linear },
        { "LFO2 Step 3",      "lfo2_step_3",         "",  -1.0f,     1.0f,     0.7071f, ParamShape::Linear },
        { "LFO2 Step 4",      "lfo2_step_4",         "",  -1.0f,     1.0f,     0.9239f, ParamShape::Linear },
        { "LFO2 Step 5",      "lfo2_step_5",         "",  -1.0f,     1.0f,     1.0000f, ParamShape::Linear },
        { "LFO2 Step 6",      "lfo2_step_6",         "",  -1.0f,     1.0f,     0.9239f, ParamShape::Linear },
        { "LFO2 Step 7",      "lfo2_step_7",         "",  -1.0f,     1.0f,     0.7071f, ParamShape::Linear },
        { "LFO2 Step 8",      "lfo2_step_8",         "",  -1.0f,     1.0f,     0.3827f, ParamShape::Linear },
        { "LFO2 Step 9",      "lfo2_step_9",         "",  -1.0f,     1.0f,     0.0000f, ParamShape::Linear },
        { "LFO2 Step 10",     "lfo2_step_10",        "",  -1.0f,     1.0f,    -0.3827f, ParamShape::Linear },
        { "LFO2 Step 11",     "lfo2_step_11",        "",  -1.0f,     1.0f,    -0.7071f, ParamShape::Linear },
        { "LFO2 Step 12",     "lfo2_step_12",        "",  -1.0f,     1.0f,    -0.9239f, ParamShape::Linear },
        { "LFO2 Step 13",     "lfo2_step_13",        "",  -1.0f,     1.0f,    -1.0000f, ParamShape::Linear },
        { "LFO2 Step 14",     "lfo2_step_14",        "",  -1.0f,     1.0f,    -0.9239f, ParamShape::Linear },
        { "LFO2 Step 15",     "lfo2_step_15",        "",  -1.0f,     1.0f,    -0.7071f, ParamShape::Linear },
        { "LFO2 Step 16",     "lfo2_step_16",        "",  -1.0f,     1.0f,    -0.3827f, ParamShape::Linear },
    };
    return table[index];
}

} // namespace sideous
