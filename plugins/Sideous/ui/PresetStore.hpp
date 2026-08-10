/*
 * Sideous - simple file-based preset storage, entirely UI-side (no DPF
 * state/program mechanism involved - a preset is just "every parameter's
 * current value", and the UI already owns exactly that in its fValues[]
 * array). One plain-text ".sidpreset" file per preset, one "symbol=value"
 * line per parameter, keyed by Params.hpp's stable symbol strings rather
 * than numeric index so presets survive future parameter reordering.
 *
 * Deliberately independent of any host preset mechanism, since VST3/CLAP
 * already save automatable parameters as part of project state - this
 * exists for a *named, browsable library* that works identically across
 * every format, including the JACK standalone which has no host preset
 * browser at all.
 *
 * Uses plain POSIX directory calls (mkdir/opendir/readdir), not
 * std::filesystem: libc++'s std::filesystem symbols are annotated
 * unavailable below a macOS 10.15 deployment target, and DPF/PawPaw's
 * macOS-universal build targets older than that for host compatibility -
 * std::filesystem here reliably broke the macOS CI build (confirmed the
 * same failure across every sideous-* plugin that copied this file).
 * POSIX mkdir/opendir/readdir/remove carry no such version gating.
 */

#pragma once

#include "../Params.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#endif

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

// mkdir, one path component at a time (POSIX mkdir() doesn't create missing
// parents) - failures (e.g. a component already existing) are silently
// ignored, same as the std::filesystem version this replaced.
inline void createDirectories(const std::string& path)
{
    for (size_t pos = 0; pos < path.size(); )
    {
        size_t next = path.find_first_of("/\\", pos);
        if (next == std::string::npos)
            next = path.size();
        const std::string partial = path.substr(0, next);
        if (!partial.empty())
        {
#if defined(_WIN32)
            _mkdir(partial.c_str());
#else
            mkdir(partial.c_str(), 0755);
#endif
        }
        pos = next + 1;
    }
}

inline bool hasSuffix(const std::string& s, const std::string& suffix) noexcept
{
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// sorted (alphabetical) list of preset display names - the ".sidpreset"
// extension is stripped, and is also what savePreset()/loadPreset()/
// deletePreset() expect as their `name` argument.
inline std::vector<std::string> listPresets()
{
    std::vector<std::string> names;
    DIR* dir = opendir(presetsDirectory().c_str());
    if (dir == nullptr)
        return names;

    static constexpr const char* kExt = ".sidpreset";
    while (dirent* entry = readdir(dir))
    {
        const std::string name = entry->d_name;
        if (hasSuffix(name, kExt))
            names.push_back(name.substr(0, name.size() - std::strlen(kExt)));
    }
    closedir(dir);

    std::sort(names.begin(), names.end());
    return names;
}

inline bool savePreset(const std::string& name, const float* values)
{
    createDirectories(presetsDirectory());

    std::ofstream out(presetsDirectory() + "/" + name + ".sidpreset");
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
    std::ifstream in(presetsDirectory() + "/" + name + ".sidpreset");
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
    return std::remove((presetsDirectory() + "/" + name + ".sidpreset").c_str()) == 0;
}

} // namespace ui
} // namespace sideous
