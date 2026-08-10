/*
 * Sideous - a handful of factory example presets, seeded into the user's
 * preset directory (see ui/PresetStore.hpp) the first time the UI ever runs
 * with an empty preset library. Compiled in (rather than shipped as separate
 * data files) so they exist identically across every build/install of the
 * plugin regardless of platform or plugin format - no bundle-path discovery
 * needed, they just get written out as ordinary, user-editable ".sidpreset"
 * files on first run and behave exactly like any preset the user saves
 * themselves from that point on.
 */

#pragma once

#include "PresetStore.hpp"

#include <cstring>
#include <utility>
#include <vector>

namespace sideous {
namespace ui {

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> overrides; // symbol -> value; everything else stays at that param's Params.hpp default
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        // Plain saw lead, filter opened up with a bit of env punch - the
        // "plain" starting point
        { "Classic Saw Lead", {
            { "filter_mode", 0.0f }, { "filter_cutoff", 4000.0f }, { "filter_resonance", 0.25f },
            { "filter_env_amount", 0.3f },
            { "filter_attack", 0.005f }, { "filter_decay", 0.2f }, { "filter_sustain", 0.3f }, { "filter_release", 0.15f },
            { "amp_attack", 0.005f }, { "amp_decay", 0.15f }, { "amp_sustain", 0.7f }, { "amp_release", 0.15f },
        }},
        // Narrow pulse width, slow lush envelope - about as close to a pad
        // as a single chiptune oscillator gets
        { "Pulse Strings", {
            { "waveform", 1.0f }, // Pulse
            { "pulse_width", 30.0f },
            { "filter_mode", 0.0f }, { "filter_cutoff", 3000.0f }, { "filter_resonance", 0.15f },
            { "amp_attack", 0.4f }, { "amp_decay", 0.3f }, { "amp_sustain", 0.85f }, { "amp_release", 0.6f },
            { "filter_env_amount", 0.15f }, { "filter_attack", 0.4f }, { "filter_decay", 0.3f }, { "filter_sustain", 0.6f }, { "filter_release", 0.4f },
        }},
        // Sub-octave heavy, ladder filter, short punchy env - a one-note-
        // bassline patch
        { "Sub Bass Stab", {
            { "sub_octave", -2.0f }, { "sub_level", 0.7f },
            { "filter_mode", 5.0f }, { "filter_cutoff", 600.0f }, { "filter_resonance", 0.3f },
            { "filter_env_amount", 0.4f },
            { "filter_attack", 0.002f }, { "filter_decay", 0.15f }, { "filter_sustain", 0.1f }, { "filter_release", 0.1f },
            { "amp_attack", 0.002f }, { "amp_decay", 0.12f }, { "amp_sustain", 0.5f }, { "amp_release", 0.1f },
        }},
        // Arp on, Up over 2 octaves, short pulse blips - the arpeggiator
        // showcase patch
        { "Arp Sequence", {
            { "arp_enabled", 1.0f }, { "arp_pattern", 0.0f }, { "arp_octaves", 2.0f },
            { "arp_sync", 12.0f }, { "arp_gate_length", 60.0f }, // 1/16
            { "waveform", 1.0f }, { "pulse_width", 40.0f },
            { "filter_mode", 0.0f }, { "filter_cutoff", 3500.0f },
            { "amp_attack", 0.002f }, { "amp_decay", 0.08f }, { "amp_sustain", 0.3f }, { "amp_release", 0.06f },
        }},
        // LFO tempo-synced to Cutoff on a resonant ladder filter - a
        // rhythmic wobble pad
        { "Wobble Pad", {
            { "lfo_destination", 1.0f }, { "lfo_amount", 0.5f }, { "lfo_sync", 9.0f }, // 1/8 straight
            { "filter_mode", 5.0f }, { "filter_cutoff", 800.0f }, { "filter_resonance", 0.5f },
            { "amp_attack", 0.3f }, { "amp_decay", 0.2f }, { "amp_sustain", 0.9f }, { "amp_release", 0.5f },
        }},
        // Mono with portamento always-on - classic monophonic glide lead
        { "Mono Glide Lead", {
            { "voice_mode", 1.0f }, // Mono
            { "portamento", 0.08f }, { "glide_mode", 1.0f }, // Always
            { "filter_mode", 0.0f }, { "filter_cutoff", 5000.0f }, { "filter_resonance", 0.2f },
            { "amp_attack", 0.01f }, { "amp_decay", 0.1f }, { "amp_sustain", 0.75f }, { "amp_release", 0.15f },
        }},
        // Ladder filter pushed hot with drive and resonance, slow filter-env
        // sweep - the aggressive/growly end of the range
        { "Ladder Growl", {
            { "filter_mode", 5.0f }, { "filter_cutoff", 400.0f }, { "filter_resonance", 0.75f }, { "filter_drive", 0.5f },
            { "filter_env_amount", 0.6f },
            { "filter_attack", 0.005f }, { "filter_decay", 0.3f }, { "filter_sustain", 0.2f }, { "filter_release", 0.25f },
            { "amp_sustain", 0.7f },
        }},
        // Mod wheel patched to vibrato at a tasteful depth - a performance
        // lead built to be played with the wheel during a take
        { "Vibrato Lead", {
            { "mod_wheel_dest", 1.0f }, { "mod_wheel_amount", 0.6f }, // Vibrato
            { "filter_mode", 0.0f }, { "filter_cutoff", 6000.0f },
            { "amp_attack", 0.01f }, { "amp_decay", 0.1f }, { "amp_sustain", 0.8f }, { "amp_release", 0.2f },
        }},
        // Narrow pulse, no sustain, fast everything - a short percussive blip
        { "Percussive Blip", {
            { "waveform", 1.0f }, { "pulse_width", 15.0f },
            { "filter_mode", 0.0f }, { "filter_cutoff", 5000.0f },
            { "amp_attack", 0.001f }, { "amp_decay", 0.06f }, { "amp_sustain", 0.0f }, { "amp_release", 0.05f },
        }},
        // Highpass with a snappy filter-env sweep and no amp sustain - a
        // plucky, thin lead-in transient
        { "Highpass Pluck", {
            { "filter_mode", 2.0f }, { "filter_cutoff", 800.0f }, { "filter_resonance", 0.3f },
            { "filter_env_amount", 0.5f },
            { "filter_attack", 0.001f }, { "filter_decay", 0.15f }, { "filter_sustain", 0.0f }, { "filter_release", 0.1f },
            { "amp_attack", 0.001f }, { "amp_decay", 0.12f }, { "amp_sustain", 0.0f }, { "amp_release", 0.08f },
        }},
        // Mod wheel patched to Cutoff at full depth - a performance filter
        // sweep played by hand
        { "Mod Wheel Filter Sweep", {
            { "mod_wheel_dest", 2.0f }, { "mod_wheel_amount", 1.0f }, // Cutoff
            { "filter_mode", 1.0f }, { "filter_cutoff", 300.0f }, { "filter_resonance", 0.4f },
            { "amp_sustain", 0.85f },
        }},
    };
    return presets;
}

// writes every factory preset to disk via the normal savePreset() path (so
// they're indistinguishable from user-saved presets from that point on) -
// call once, only when the preset library is empty (first run).
inline void seedFactoryPresets()
{
    for (const FactoryPreset& fp : factoryPresets())
    {
        float values[kParamCount];
        for (uint32_t i = 0; i < kParamCount; ++i)
            values[i] = getParamInfo(i).def;

        for (const auto& kv : fp.overrides)
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
            {
                if (std::strcmp(getParamInfo(i).symbol, kv.first) == 0)
                {
                    values[i] = kv.second;
                    break;
                }
            }
        }

        savePreset(fp.name, values);
    }
}

} // namespace ui
} // namespace sideous
