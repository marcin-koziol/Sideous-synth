/*
 * UI scale for the Sideous family of plugins - one setting shared by every
 * plugin on this machine (it's about the monitor, not about a plugin), kept
 * in a small text file. Each UI cycles it from a button on its preset bar,
 * labelled by formatLabel().
 *
 * Why not just DPF's automatic scaling: hosts disagree. Some pass the desktop
 * scale (2.0 on a 4K screen), some pass a flat 1.0, some nothing (DPF then
 * reads Xft.dpi). AUTO keeps that behaviour; any fixed value overrides the
 * host so the plugin looks the same everywhere. DPF's automatic mode also
 * refuses to go below the host's factor, so the UIs scale by hand instead:
 * the window is sized logical * scale, painting runs under cairo_scale(),
 * and mouse positions are divided back down - layout code never changes.
 *
 * Copied verbatim into each plugin project (like PresetStore.hpp); keep the
 * copies identical. POSIX mkdir rather than std::filesystem for the same
 * macOS deployment-target reason as PresetStore.hpp.
 */

#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#endif

namespace sideous_uiscale {

// 0 = AUTO (host / desktop factor); anything else is a fixed scale
static constexpr float kSettings[] = { 0.0f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
static constexpr int kNumSettings = (int)(sizeof(kSettings) / sizeof(kSettings[0]));

inline std::string configDirectory()
{
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    return appData ? std::string(appData) + "\\sideous" : ".";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/Library/Application Support/sideous" : ".";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::string(xdg) + "/sideous";
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/.config/sideous" : ".";
#endif
}

inline std::string configPath()
{
#if defined(_WIN32)
    return configDirectory() + "\\ui-scale";
#else
    return configDirectory() + "/ui-scale";
#endif
}

// the stored setting, snapped to a known one; AUTO if missing or unreadable
inline float loadSetting()
{
    std::ifstream in(configPath());
    float value = 0.0f;
    if (!(in >> value))
        return 0.0f;
    for (float s : kSettings)
        if (std::fabs(s - value) < 0.01f)
            return s;
    return 0.0f;
}

inline void saveSetting(float setting)
{
    const std::string dir = configDirectory();
#if defined(_WIN32)
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
    std::ofstream out(configPath());
    if (out.is_open())
        out << setting << "\n";
}

inline float nextSetting(float setting)
{
    for (int i = 0; i < kNumSettings; ++i)
        if (std::fabs(kSettings[i] - setting) < 0.01f)
            return kSettings[(i + 1) % kNumSettings];
    return kSettings[0];
}

// the scale actually used: a fixed setting wins, AUTO follows the host
inline double resolve(float setting, double hostScale)
{
    if (setting > 0.0f)
        return setting;
    return hostScale > 0.0 ? hostScale : 1.0;
}

// label for the UI-scale button on each plugin's preset bar:
// "UI 150%" for a fixed setting, "AUTO 200%" when following the host
inline void formatLabel(float setting, double scale, char* out, size_t outSize)
{
    std::snprintf(out, outSize, setting > 0.0f ? "UI %d%%" : "AUTO %d%%", (int)std::lround(scale * 100.0));
}

} // namespace sideous_uiscale
