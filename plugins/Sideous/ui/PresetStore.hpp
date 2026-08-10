/*
 * Sideous - simple file-based preset storage, entirely UI-side (no DPF
 * state/program mechanism involved - a preset is just "every parameter's
 * current value", and the UI already owns exactly that in its fValues[]
 * array). One plain-text ".sidpreset" file per preset, one "symbol=value"
 * line per parameter, keyed by Params.hpp's stable symbol strings rather
 * than numeric index so presets survive future parameter reordering.
 *
 * This is deliberately independent of any host preset mechanism (VST3/CLAP
 * hosts already save/restore automatable parameters as part of their own
 * project state without any of this) - it exists so there's a *named,
 * browsable preset library* that works identically across every format,
 * including the JACK standalone, which has no host preset browser at all.
 */

#pragma once

#include "../Params.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace sideous {
namespace ui {

inline std::string presetsDirectory()
{
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    return appData ? std::string(appData) + "\\sideous\\presets" : ".\\presets";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/Library/Application Support/sideous/presets" : "./presets";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        return std::string(xdg) + "/sideous/presets";
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/.local/share/sideous/presets" : "./presets";
#endif
}

// filesystem-safe preset name: strips path separators and other characters
// that would break a single-component filename, trims whitespace, and
// falls back to a sane default if that leaves nothing usable.
inline std::string sanitizePresetName(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (char c : raw)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|' || (unsigned char)c < 0x20)
            out += '_';
        else
            out += c;
    }

    const size_t start = out.find_first_not_of(' ');
    if (start == std::string::npos)
        return "Preset";
    const size_t end = out.find_last_not_of(' ');
    return out.substr(start, end - start + 1);
}

// sorted (alphabetical) list of preset display names - the ".sidpreset"
// extension is stripped, and is also what savePreset()/loadPreset()/
// deletePreset() expect as their `name` argument.
inline std::vector<std::string> listPresets()
{
    std::vector<std::string> names;
    std::error_code ec;
    const std::filesystem::path dir(presetsDirectory());
    if (!std::filesystem::exists(dir, ec) || ec)
        return names;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec)
            break;
        if (entry.path().extension() == ".sidpreset")
            names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

inline bool savePreset(const std::string& name, const float* values)
{
    std::error_code ec;
    const std::filesystem::path dir(presetsDirectory());
    std::filesystem::create_directories(dir, ec);

    std::ofstream out(dir / (name + ".sidpreset"));
    if (!out.is_open())
        return false;

    out << "name=" << name << "\n";
    for (uint32_t i = 0; i < kParamCount; ++i)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6g", (double)values[i]);
        out << getParamInfo(i).symbol << "=" << buf << "\n";
    }
    return true;
}

// unknown symbols (e.g. from a future version's extra params) are ignored;
// symbols missing from the file (e.g. an older preset predating a param
// that was added later) simply leave that slot's existing value untouched -
// so loading never needs to know which plugin version wrote the file.
inline bool loadPreset(const std::string& name, float* values)
{
    const std::filesystem::path file = std::filesystem::path(presetsDirectory()) / (name + ".sidpreset");
    std::ifstream in(file);
    if (!in.is_open())
        return false;

    std::string line;
    while (std::getline(in, line))
    {
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);
        if (key == "name")
            continue;

        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            if (key == getParamInfo(i).symbol)
            {
                values[i] = std::strtof(val.c_str(), nullptr);
                break;
            }
        }
    }
    return true;
}

inline bool deletePreset(const std::string& name)
{
    std::error_code ec;
    const std::filesystem::path file = std::filesystem::path(presetsDirectory()) / (name + ".sidpreset");
    return std::filesystem::remove(file, ec);
}

} // namespace ui
} // namespace sideous
