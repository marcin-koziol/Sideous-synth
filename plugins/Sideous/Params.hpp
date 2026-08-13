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
        { "LFO Sync",         "lfo_sync",            "",   0.0f,    14.0f,     0.0f,   ParamShape::Linear },
        { "LFO Destination",  "lfo_destination",     "",   0.0f,     3.0f,     0.0f,   ParamShape::Linear },
        { "LFO Amount",       "lfo_amount",          "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Arp Enabled",      "arp_enabled",         "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Arp Pattern",      "arp_pattern",         "",   0.0f,     3.0f,     0.0f,   ParamShape::Linear },
        { "Arp Octaves",      "arp_octaves",         "",   1.0f,     4.0f,     1.0f,   ParamShape::Linear },
        { "Arp Rate",         "arp_rate_hz",         "Hz", 0.5f,    20.0f,     4.0f,   ParamShape::Logarithmic },
        { "Arp Sync",         "arp_sync",            "",   0.0f,    14.0f,    12.0f,   ParamShape::Linear },
        { "Arp Gate Length",  "arp_gate_length",     "%",  10.0f,  100.0f,    70.0f,   ParamShape::Linear },
        { "Voice Mode",       "voice_mode",          "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Portamento",       "portamento",          "s",  0.0f,     3.0f,     0.0f,   ParamShape::Linear },
        { "Pitch Bend Range", "pitch_bend_range",    "st", 1.0f,    24.0f,     2.0f,   ParamShape::Linear },
        { "Mod Wheel Dest",   "mod_wheel_dest",      "",   0.0f,     3.0f,     0.0f,   ParamShape::Linear },
        { "Glide Mode",       "glide_mode",          "",   0.0f,     1.0f,     1.0f,   ParamShape::Linear },
        { "Mod Wheel Amount", "mod_wheel_amount",    "",   0.0f,     1.0f,     0.5f,   ParamShape::Linear },
    };
    return table[index];
}

} // namespace sideous
