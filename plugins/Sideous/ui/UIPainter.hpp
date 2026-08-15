/*
 * Sideous - GUI layout and Cairo painting, shared between the real plugin
 * UI and an offline PNG renderer used to check the look during development.
 *
 * Retro chiptune styling: dark navy panels, C64/Atari-ish accent colors,
 * bold monospace labels, flat shapes (no gradients/skeuomorphism).
 */

#pragma once

#include <cairo.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

#include "../Params.hpp"

namespace sideous {
namespace ui {

// -----------------------------------------------------------------------------------------------------------
// palette

struct Color { double r, g, b; };

static constexpr Color kBg        { 0.031, 0.031, 0.071 };
static constexpr Color kPanelBg   { 0.086, 0.086, 0.173 };
static constexpr Color kPanelEdge { 0.298, 0.267, 0.663 };
static constexpr Color kTextMain  { 0.75, 0.79, 1.00 };
static constexpr Color kTextDim   { 0.46, 0.46, 0.66 };
static constexpr Color kKnobBody  { 0.145, 0.137, 0.290 };
static constexpr Color kKnobRing  { 0.243, 0.227, 0.443 };
static constexpr Color kBtnBg     { 0.114, 0.110, 0.235 };

static constexpr Color kCyan    { 0.30, 0.90, 0.90 };
static constexpr Color kMagenta { 1.00, 0.35, 0.75 };
static constexpr Color kYellow  { 1.00, 0.85, 0.30 };
static constexpr Color kGreen   { 0.45, 0.95, 0.55 };
static constexpr Color kOrange  { 1.00, 0.55, 0.20 };
static constexpr Color kPurple  { 0.65, 0.45, 1.00 };
static constexpr Color kRust    { 0.776, 0.341, 0.180 }; // preset bar: delete button
static constexpr Color kAmber   { 0.937, 0.600, 0.220 }; // preset bar: unsaved-changes marker

inline void setColor(cairo_t* cr, Color c, double a = 1.0) noexcept
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

// -----------------------------------------------------------------------------------------------------------
// layout model

struct Knob
{
    uint32_t param;
    float cx, cy, radius;
    const char* label;
    Color accent;
};

struct SelectorOption
{
    float value;
    const char* label;
};

struct Selector
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
    const char* caption = nullptr; // optional small label drawn above the row,
                                    // for selectors that live outside a titled panel
};

// like a Selector, but the options are hidden behind a closed combo-box until
// clicked open - used where there are too many options for a button row
struct Dropdown
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
};

// interactive step-sequencer grid: `stepCount` bar columns spanning
// [param0 .. param0+stepCount-1], each bipolar around a drawn center line.
// Click/drag sets a column's value from vertical mouse position; see
// SideousUI.cpp's hitTestStepSeq()/onMouse()/onMotion() for the interaction.
struct StepSeq
{
    uint32_t param0;      // kParamLfoStep1 or kParamLfo2Step1
    uint32_t countParam;  // kParamLfoStepCount or kParamLfo2StepCount
    float x, y, w, h;
    Color accent;
};

struct PanelBox { float x, y, w, h; const char* title; Color accent; };

// preset bar: prev/next/save/delete buttons plus a name field. Deliberately
// plain rectangles rather than reusing the Selector/Knob widgets - it isn't
// a parameter, it's file-system CRUD.
struct Button { float x, y, w, h; const char* label; Color accent; };

struct PresetBarLayout
{
    Button prev, next, save, del;
    float nameX, nameY, nameW, nameH;
    Color accent;
};

// read-only ADSR shape preview - no interaction, just visualizes the
// attack/decay/sustain/release/curve parameters of one envelope
struct EnvelopeGraph
{
    uint32_t attackParam, decayParam, sustainParam, releaseParam, curveParam;
    float x, y, w, h;
    Color accent;
};

struct Layout
{
    float width, height;
    std::vector<PanelBox> panels;
    std::vector<Knob> knobs;
    std::vector<Selector> selectors;
    std::vector<Dropdown> dropdowns;
    std::vector<StepSeq> stepSeqs;
    std::vector<EnvelopeGraph> envelopeGraphs;
    PresetBarLayout presetBar;
};

// -----------------------------------------------------------------------------------------------------------
// layout builder

inline void addKnobRow(std::vector<Knob>& knobs, float panelX, float panelW, float rowY, float radius,
                        Color accent, std::initializer_list<std::pair<uint32_t, const char*>> items)
{
    const float slot = panelW / (float)items.size();
    size_t i = 0;
    for (const auto& item : items)
    {
        const float cx = panelX + slot * ((float)i + 0.5f);
        knobs.push_back({ item.first, cx, rowY, radius, item.second, accent });
        ++i;
    }
}

// shared by both the LFO and arp rate dropdowns: 0 = free-running (use the
// paired Rate Hz knob), 1..16 = tempo note divisions (dotted/straight/triplet
// down through 1/32, then plain 1/64 and 1/128 - same as 1/32, no dotted/
// triplet variant, to keep the dropdown from growing enormous)
inline std::vector<SelectorOption> syncOptions()
{
    return {
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
}

inline Layout buildLayout(float width, float height)
{
    Layout L;
    L.width = width;
    L.height = height;

    // wide-and-short rather than narrow-and-tall: a 2/3-column grid instead
    // of a single stacked column, so the window actually fits on a 1080p
    // screen (matches the footprint of sideous-bass/sideous-drums/
    // Ostinateous - this plugin was the odd one out, stacked tall enough to
    // run off the bottom of the screen).
    const float margin = 12.0f;
    const float gap = 8.0f;
    const float fullW = width - margin * 2.0f;
    const float colW = (fullW - gap) / 2.0f;             // 2-col rows: AMP/FILTER ENV, LFO/LFO2
    const float colBx = margin + colW + gap;
    const float col3W = (fullW - gap * 2.0f) / 3.0f;      // 3-col row: OSC/FILTER/ARP
    const float col3Bx = margin + col3W + gap;
    const float col3Cx = col3Bx + col3W + gap;

    // preset bar: full width, right below the header; everything else starts below it
    const float presetBarY = 48.0f, presetBarH = 40.0f;

    // row1H is set by ARPEGGIATOR (an extra selector/dropdown row that
    // OSCILLATOR/FILTER don't have), not by those two - they end up with a
    // little slack below their content, which is unavoidable since all three
    // panels in this row share one height.
    const float row1Y = presetBarY + presetBarH + gap, row1H = 152.0f;
    const float row2Y = row1Y + row1H + gap, row2H = 146.0f;
    // LFO/LFO2 sit side by side rather than stacked - each panel's own
    // sidebar-plus-grid layout (see buildLfoPanel below) is what drives
    // row3H, not the full window width.
    const float row3Y = row2Y + row2H + gap, row3H = 210.0f;
    const float barY = row3Y + row3H + gap, barH = height - barY - margin;

    // --- preset bar --- (all positions absolute, left-to-right: prev/next,
    // then the name field filling the middle, then save/delete flush right)
    {
        L.presetBar.accent = kCyan;
        const float y = presetBarY, h = presetBarH;
        const float barLeftX = margin;
        const float barRightX = width - margin;

        L.presetBar.prev = { barLeftX, y, 40.0f, h, "<", kCyan };
        L.presetBar.next = { barLeftX + 40.0f + 8.0f, y, 40.0f, h, ">", kCyan };

        const float delX = barRightX - 100.0f;
        const float saveX = delX - 8.0f - 90.0f;
        L.presetBar.save = { saveX, y, 90.0f, h, "SAVE", kCyan };
        L.presetBar.del  = { delX, y, 100.0f, h, "DELETE", kRust };

        L.presetBar.nameX = L.presetBar.next.x + 40.0f + 14.0f;
        L.presetBar.nameY = y;
        L.presetBar.nameW = saveX - 14.0f - L.presetBar.nameX;
        L.presetBar.nameH = h;
    }

    L.panels.push_back({ margin,   row1Y, col3W, row1H, "OSCILLATOR", kCyan });    // 0
    L.panels.push_back({ col3Bx,   row1Y, col3W, row1H, "FILTER",     kMagenta }); // 1
    L.panels.push_back({ margin,   row2Y, colW,  row2H, "AMP ENV",    kYellow });  // 2
    L.panels.push_back({ colBx,    row2Y, colW,  row2H, "FILTER ENV", kGreen });   // 3
    L.panels.push_back({ margin,   row3Y, colW,  row3H, "LFO",        kOrange });  // 4
    L.panels.push_back({ colBx,    row3Y, colW,  row3H, "LFO2",       kAmber });   // 5
    L.panels.push_back({ col3Cx,   row1Y, col3W, row1H, "ARPEGGIATOR",kPurple });  // 6
    L.panels.push_back({ margin,   barY,  fullW, barH,  nullptr, kCyan });         // 7

    // --- oscillator ---
    {
        const PanelBox& p = L.panels[0];
        Selector wave;
        wave.param = kParamWaveform;
        wave.accent = p.accent;
        wave.x = p.x + 12.0f; wave.y = p.y + 26.0f; wave.w = p.w - 24.0f; wave.h = 24.0f;
        wave.options = { { 0.0f, "SAW" }, { 1.0f, "PULSE" }, { 2.0f, "TRI" } };
        L.selectors.push_back(wave);

        const float rowY = p.y + 26.0f + 24.0f + 40.0f;

        // PW knob | sub-osc octave selector | sub-osc level knob, side by side
        L.knobs.push_back({ kParamPulseWidth, p.x + p.w * 0.16f, rowY, 22.0f, "PW", p.accent });

        Selector subOct;
        subOct.param = kParamSubOctave;
        subOct.accent = p.accent;
        subOct.w = p.w * 0.40f;
        subOct.x = p.x + p.w * 0.5f - subOct.w * 0.5f;
        subOct.h = 24.0f;
        subOct.y = rowY - subOct.h * 0.5f;
        subOct.options = { { -2.0f, "-2" }, { -1.0f, "-1" }, { 1.0f, "+1" }, { 2.0f, "+2" } };
        L.selectors.push_back(subOct);

        L.knobs.push_back({ kParamSubLevel, p.x + p.w * 0.84f, rowY, 22.0f, "SUB LVL", p.accent });
    }

    // --- filter ---
    {
        const PanelBox& p = L.panels[1];
        Dropdown mode;
        mode.param = kParamFilterMode;
        mode.accent = p.accent;
        mode.x = p.x + 12.0f; mode.y = p.y + 26.0f; mode.w = p.w - 24.0f; mode.h = 24.0f;
        mode.options = {
            { 0.0f, "LOWPASS 12dB" },
            { 1.0f, "LOWPASS 24dB" },
            { 2.0f, "HIGHPASS 12dB" },
            { 3.0f, "HIGHPASS 24dB" },
            { 4.0f, "BANDPASS 12dB" },
            { 5.0f, "LADDER 24dB" },
        };
        L.dropdowns.push_back(mode);

        addKnobRow(L.knobs, p.x, p.w, p.y + 26.0f + 24.0f + 40.0f, 22.0f, p.accent,
                   {
                       { kParamFilterCutoff,    "CUTOFF" },
                       { kParamFilterResonance, "RESO" },
                       { kParamFilterDrive,     "DRIVE" },
                       { kParamFilterEnvAmount, "ENV AMT" },
                   });
    }

    // --- amp envelope ---
    {
        const PanelBox& p = L.panels[2];
        const float rowY = p.y + 26.0f + 30.0f;
        addKnobRow(L.knobs, p.x, p.w, rowY, 20.0f, p.accent,
                   {
                       { kParamAmpAttack,           "ATTACK" },
                       { kParamAmpDecay,             "DECAY" },
                       { kParamAmpSustain,           "SUSTAIN" },
                       { kParamAmpRelease,           "RELEASE" },
                       { kParamAmpCurve,             "CURVE" },
                       { kParamVelocitySensitivity,  "VELOCITY" },
                   });

        L.envelopeGraphs.push_back({
            kParamAmpAttack, kParamAmpDecay, kParamAmpSustain, kParamAmpRelease, kParamAmpCurve,
            p.x + 12.0f, rowY + 20.0f + 22.0f, p.w - 24.0f, 38.0f, p.accent });
    }

    // --- filter envelope ---
    {
        const PanelBox& p = L.panels[3];
        const float rowY = p.y + 26.0f + 30.0f;
        addKnobRow(L.knobs, p.x, p.w, rowY, 20.0f, p.accent,
                   {
                       { kParamFilterAttack,   "ATTACK" },
                       { kParamFilterDecay,    "DECAY" },
                       { kParamFilterSustain,  "SUSTAIN" },
                       { kParamFilterRelease,  "RELEASE" },
                       { kParamFilterEnvCurve, "CURVE" },
                   });

        L.envelopeGraphs.push_back({
            kParamFilterAttack, kParamFilterDecay, kParamFilterSustain, kParamFilterRelease, kParamFilterEnvCurve,
            p.x + 12.0f, rowY + 20.0f + 22.0f, p.w - 24.0f, 38.0f, p.accent });
    }

    // --- LFO / LFO2 --- (identical layout, shared by this lambda: a sidebar
    // of controls on the left, the step-sequencer grid filling the rest of
    // the panel on the right - like a hardware step sequencer, controls
    // beside the steps rather than stacked above them, which keeps the
    // panel height from ballooning even with 16 clickable/drawable columns)
    auto buildLfoPanel = [&](const PanelBox& p, uint32_t waveParam, uint32_t destParam, uint32_t syncParam,
                              uint32_t stepCountParam, uint32_t step1Param,
                              uint32_t rateParam, uint32_t amountParam)
    {
        const float sideW = p.w * 0.34f;
        const float sideX = p.x + 10.0f;
        const float sideContentW = sideW - 10.0f;

        Selector wave;
        wave.param = waveParam;
        wave.accent = p.accent;
        wave.x = sideX; wave.y = p.y + 24.0f; wave.w = sideContentW; wave.h = 22.0f;
        wave.options = { { 0.0f, "SINE" }, { 1.0f, "SAW" }, { 2.0f, "SQR" }, { 3.0f, "GATE" } };
        L.selectors.push_back(wave);

        Dropdown dest;
        dest.param = destParam;
        dest.accent = p.accent;
        dest.x = sideX; dest.y = wave.y + wave.h + 6.0f; dest.w = sideContentW; dest.h = 22.0f;
        dest.options = { { 0.0f, "PITCH" }, { 1.0f, "CUTOFF" }, { 2.0f, "AMP" }, { 3.0f, "PW" } };
        L.dropdowns.push_back(dest);

        Dropdown sync;
        sync.param = syncParam;
        sync.accent = p.accent;
        sync.x = sideX; sync.y = dest.y + dest.h + 6.0f; sync.w = sideContentW; sync.h = 22.0f;
        sync.options = syncOptions();
        L.dropdowns.push_back(sync);

        // quick-pick step counts rather than a numeric entry widget - covers
        // the common resolutions without needing a new input type
        Selector stepCount;
        stepCount.param = stepCountParam;
        stepCount.accent = p.accent;
        stepCount.x = sideX; stepCount.y = sync.y + sync.h + 6.0f; stepCount.w = sideContentW; stepCount.h = 20.0f;
        stepCount.options = { { 4.0f, "4" }, { 8.0f, "8" }, { 16.0f, "16" } };
        L.selectors.push_back(stepCount);

        addKnobRow(L.knobs, p.x, sideW, stepCount.y + stepCount.h + 34.0f, 16.0f, p.accent,
                   {
                       { rateParam,   "RATE" },
                       { amountParam, "AMOUNT" },
                   });

        StepSeq grid;
        grid.param0 = step1Param;
        grid.countParam = stepCountParam;
        grid.accent = p.accent;
        grid.x = p.x + sideW + 10.0f;
        grid.y = p.y + 24.0f;
        grid.w = (p.x + p.w - 10.0f) - grid.x;
        grid.h = p.h - 24.0f - 10.0f;
        L.stepSeqs.push_back(grid);
    };

    buildLfoPanel(L.panels[4], kParamLfoWaveform, kParamLfoDestination, kParamLfoSync,
                  kParamLfoStepCount, kParamLfoStep1, kParamLfoRateHz, kParamLfoAmount);
    buildLfoPanel(L.panels[5], kParamLfo2Waveform, kParamLfo2Destination, kParamLfo2Sync,
                  kParamLfo2StepCount, kParamLfo2Step1, kParamLfo2RateHz, kParamLfo2Amount);

    // --- arpeggiator --- (moved to its own full-width row: no longer fits
    // half-width beside LFO1 now that LFO1 needs the full panel width)
    {
        const PanelBox& p = L.panels[6];

        Selector enabled;
        enabled.param = kParamArpEnabled;
        enabled.accent = p.accent;
        enabled.x = p.x + 12.0f; enabled.y = p.y + 26.0f; enabled.w = p.w * 0.26f; enabled.h = 24.0f;
        enabled.options = { { 0.0f, "OFF" }, { 1.0f, "ON" } };
        L.selectors.push_back(enabled);

        Selector pattern;
        pattern.param = kParamArpPattern;
        pattern.accent = p.accent;
        pattern.x = enabled.x + enabled.w + 8.0f; pattern.y = enabled.y;
        pattern.w = (p.x + p.w - 12.0f) - pattern.x; pattern.h = 24.0f;
        pattern.options = { { 0.0f, "UP" }, { 1.0f, "DOWN" }, { 2.0f, "UP/DN" }, { 3.0f, "RAND" } };
        L.selectors.push_back(pattern);

        Dropdown sync;
        sync.param = kParamArpSync;
        sync.accent = p.accent;
        sync.x = p.x + 12.0f; sync.y = enabled.y + enabled.h + 8.0f; sync.w = p.w - 24.0f; sync.h = 24.0f;
        sync.options = syncOptions();
        L.dropdowns.push_back(sync);

        addKnobRow(L.knobs, p.x, p.w, sync.y + sync.h + 26.0f, 18.0f, p.accent,
                   {
                       { kParamArpOctaves,    "OCTAVES" },
                       { kParamArpRateHz,     "RATE" },
                       { kParamArpGateLength, "GATE" },
                   });
    }

    // --- bottom bar ---
    {
        const PanelBox& p = L.panels[7];
        const float cy = p.y + p.h * 0.5f;
        L.knobs.push_back({ kParamMasterVolume, p.x + 60.0f, cy, 16.0f, "VOLUME", kCyan });

        Selector mode;
        mode.param = kParamVoiceMode;
        mode.accent = p.accent;
        mode.x = p.x + 180.0f; mode.y = cy - 12.0f; mode.w = 130.0f; mode.h = 24.0f;
        mode.options = { { 0.0f, "POLY" }, { 1.0f, "MONO" } };
        L.selectors.push_back(mode);

        L.knobs.push_back({ kParamPortamentoTime, p.x + 430.0f, cy, 16.0f, "GLIDE", kCyan });
        L.knobs.push_back({ kParamPitchBendRange, p.x + 490.0f, cy, 16.0f, "BEND", kCyan });

        Selector glideMode;
        glideMode.param = kParamGlideMode;
        glideMode.accent = p.accent;
        glideMode.caption = "GLIDE MODE";
        glideMode.x = p.x + 550.0f; glideMode.y = cy - 12.0f; glideMode.w = 130.0f; glideMode.h = 24.0f;
        glideMode.options = { { 0.0f, "LEGATO" }, { 1.0f, "ALWAYS" } };
        L.selectors.push_back(glideMode);

        L.knobs.push_back({ kParamModWheelAmount, p.x + 820.0f, cy, 16.0f, "MW AMT", kCyan });

        Selector modWheel;
        modWheel.param = kParamModWheelDestination;
        modWheel.accent = p.accent;
        modWheel.caption = "MOD WHEEL";
        modWheel.x = p.x + 880.0f; modWheel.y = cy - 12.0f; modWheel.w = (p.w - 12.0f) - 880.0f; modWheel.h = 24.0f;
        modWheel.options = { { 0.0f, "OFF" }, { 1.0f, "VIBRATO" }, { 2.0f, "CUTOFF" }, { 3.0f, "VOLUME" } };
        L.selectors.push_back(modWheel);
    }

    return L;
}

// -----------------------------------------------------------------------------------------------------------
// value <-> knob-rotation mapping (shared by drawing and mouse-drag hit logic)

inline float paramToNormalized(uint32_t param, float value) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    float t;
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        const float v = value < lo ? lo : (value > hi ? hi : value);
        t = (float)(std::log(v / lo) / std::log(hi / lo));
    }
    else
    {
        t = (value - info.min) / (info.max - info.min);
    }
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

inline float normalizedToParam(uint32_t param, float t) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        return lo * std::pow(hi / lo, t);
    }
    return info.min + (info.max - info.min) * t;
}

// selector option rects are needed both for drawing and for mouse hit-testing;
// compute them in one place so the two can't drift apart.
inline void getSelectorOptionRect(const Selector& sel, size_t i, double& ox, double& ow) noexcept
{
    const size_t n = sel.options.size();
    const double gap = 6.0;
    ow = (sel.w - gap * (double)(n - 1)) / (double)n;
    ox = sel.x + (ow + gap) * (double)i;
}

// dropdown open-list option rects, same drawing/hit-test-sharing rationale.
// opens downward normally, but flips upward if the list (which can be long -
// the tempo-sync menus have 17 entries) would otherwise run off the bottom
// of the window and there's more room above the closed box than below it.
inline void getDropdownOptionRect(const Dropdown& dd, size_t i, float layoutHeight, double& oy, double& oh) noexcept
{
    const double n = (double)dd.options.size();
    oh = dd.h - 4.0;

    const double downSpace = (double)layoutHeight - (dd.y + dd.h);
    const double upSpace = dd.y;
    const bool openUpward = (n * oh > downSpace) && (upSpace > downSpace);

    if (openUpward)
        oy = (dd.y - n * oh) + oh * (double)i;
    else
        oy = dd.y + dd.h + oh * (double)i;
}

// step-sequencer column rects, same drawing/hit-test-sharing rationale as
// the selector/dropdown option rects above. stepCount is passed separately
// (rather than read off the struct) since it's live parameter state, not
// part of the static layout.
inline void getStepSeqColumnRect(const StepSeq& seq, int stepCount, int i, double& ox, double& ow) noexcept
{
    ow = seq.w / (double)stepCount;
    ox = seq.x + ow * (double)i;
}

// -----------------------------------------------------------------------------------------------------------
// value formatting

inline void formatParamValue(uint32_t index, float value, char* out, size_t outSize) noexcept
{
    switch (index)
    {
    case kParamWaveform:
        std::snprintf(out, outSize, "%s", value < 0.5f ? "SAW" : value < 1.5f ? "PULSE" : "TRI");
        return;
    case kParamArpOctaves:
        std::snprintf(out, outSize, "%d", (int)(value + 0.5f));
        return;
    default:
        break;
    }

    const ParamInfo& info = getParamInfo(index);

    if (std::strcmp(info.unit, "Hz") == 0)
    {
        if (value >= 1000.0f)
            std::snprintf(out, outSize, "%.2fk", value / 1000.0);
        else
            std::snprintf(out, outSize, "%.0f", value);
    }
    else if (std::strcmp(info.unit, "s") == 0)
    {
        if (value < 1.0f)
            std::snprintf(out, outSize, "%.0fms", value * 1000.0);
        else
            std::snprintf(out, outSize, "%.2fs", value);
    }
    else if (std::strcmp(info.unit, "%") == 0)
    {
        std::snprintf(out, outSize, "%.0f%%", value);
    }
    else if (std::strcmp(info.unit, "st") == 0)
    {
        std::snprintf(out, outSize, "\xc2\xb1%dst", (int)(value + 0.5f)); // U+00B1 PLUS-MINUS SIGN
    }
    else
    {
        std::snprintf(out, outSize, "%.2f", value);
    }
}

// -----------------------------------------------------------------------------------------------------------
// drawing helpers

inline void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -M_PI_2, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0,      M_PI_2);
    cairo_arc(cr, x + r,     y + h - r, r, M_PI_2,   M_PI);
    cairo_arc(cr, x + r,     y + r,     r, M_PI,     3.0 * M_PI_2);
    cairo_close_path(cr);
}

// cairo_show_text() leaves a dangling current point; if the next drawing call
// is cairo_arc()/cairo_line_to() without an explicit new subpath, cairo will
// implicitly connect from that stray point. Always reset the path after text.
inline void drawText(cairo_t* cr, const char* text, double x, double y)
{
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
    cairo_new_path(cr);
}

inline void centeredText(cairo_t* cr, const char* text, double cx, double cy)
{
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    drawText(cr, text, cx - ext.width / 2.0 - ext.x_bearing, cy - ext.height / 2.0 - ext.y_bearing);
}

inline void setFont(cairo_t* cr, double size, bool bold = true)
{
    cairo_select_font_face(cr, "monospace",
                            CAIRO_FONT_SLANT_NORMAL,
                            bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

// -----------------------------------------------------------------------------------------------------------
// paint

struct PaintState
{
    const float* values = nullptr;   // [kParamCount]
    int hoverKnob = -1;               // index into layout.knobs, or -1
    int dragKnob = -1;
    int hoverSelector = -1;           // index into layout.selectors, or -1
    int hoverSelectorOption = -1;
    int hoverDropdown = -1;            // index into layout.dropdowns, or -1 (closed-box hover)
    int openDropdown = -1;            // index into layout.dropdowns, or -1
    int hoverDropdownOption = -1;
    int hoverStepSeq = -1;             // index into layout.stepSeqs, or -1
    int hoverStepSeqColumn = -1;
    int dragStepSeq = -1;
    const bool* autoShowValue = nullptr; // [kParamCount], true = briefly show this
                                          // knob's value even without dragging
                                          // (e.g. just changed via automation)

    const char* presetName = "INIT";
    bool presetIsUnsaved = false; // true when a param has changed since the
                                   // preset shown was last loaded/saved
    bool presetEditingName = false;
    const char* presetEditBuffer = "";
    int hoverPresetButton = -1;   // 0=prev,1=next,2=save,3=delete, or -1
    bool presetDeleteEnabled = false; // false while on INIT - nothing to delete
};

inline void paintKnob(cairo_t* cr, const Knob& k, float value, bool hovered, bool dragging, bool autoShow)
{
    const float t = paramToNormalized(k.param, value);

    const double startAngle = M_PI * 0.75;   // 135deg
    const double sweep = M_PI * 1.5;         // 270deg total
    const double valueAngle = startAngle + sweep * t;

    // outer ring track
    cairo_set_line_width(cr, 5.0);
    setColor(cr, kKnobRing);
    cairo_arc(cr, k.cx, k.cy, k.radius, startAngle, startAngle + sweep);
    cairo_stroke(cr);

    // value arc
    setColor(cr, k.accent, dragging ? 1.0 : (hovered ? 0.9 : 0.75));
    cairo_arc(cr, k.cx, k.cy, k.radius, startAngle, valueAngle);
    cairo_stroke(cr);

    // body
    setColor(cr, kKnobBody);
    cairo_arc(cr, k.cx, k.cy, k.radius - 6.0, 0.0, 2.0 * M_PI);
    cairo_fill(cr);

    if (hovered || dragging)
    {
        setColor(cr, k.accent, 0.18);
        cairo_arc(cr, k.cx, k.cy, k.radius - 6.0, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
    }

    // pointer
    setColor(cr, k.accent);
    cairo_set_line_width(cr, 3.0);
    const double px = k.cx + std::cos(valueAngle) * (k.radius - 10.0);
    const double py = k.cy + std::sin(valueAngle) * (k.radius - 10.0);
    cairo_move_to(cr, k.cx, k.cy);
    cairo_line_to(cr, px, py);
    cairo_stroke(cr);

    // label
    setFont(cr, 10.5);
    setColor(cr, kTextDim);
    centeredText(cr, k.label, k.cx, k.cy + k.radius + 14.0);

    if (dragging || autoShow)
    {
        char buf[32];
        formatParamValue(k.param, value, buf, sizeof(buf));
        setFont(cr, 11.5);
        setColor(cr, k.accent);
        centeredText(cr, buf, k.cx, k.cy - k.radius - 12.0);
    }
}

// mirrors ADSR::setCurve()'s shaping exactly, so the preview matches what's
// actually heard: exponent 1 (curve=0, linear) down to 0.25 (curve=1,
// fast-move/slow-settle analog feel)
inline float envelopeCurveShape(float t, float curveAmount) noexcept
{
    const float exponent = std::pow(4.0f, -curveAmount);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return std::pow(t, exponent);
}

// read-only ADSR shape preview. Segment widths are fixed proportions of the
// graph (not literal time), since attack/decay/release can differ by
// several orders of magnitude - a literal time scale would make short
// stages invisible next to a multi-second release. The curve shaping is
// still drawn exactly, which is the musically relevant part to see anyway.
inline void paintEnvelopeGraph(cairo_t* cr, const EnvelopeGraph& eg, const float* values)
{
    const float sustain = values[eg.sustainParam];
    const float curve = values[eg.curveParam];

    const float attackFrac = 0.25f, decayFrac = 0.25f, holdFrac = 0.20f, releaseFrac = 0.30f;

    roundedRect(cr, eg.x, eg.y, eg.w, eg.h, 4.0);
    setColor(cr, kBg, 0.55);
    cairo_fill(cr);

    const int steps = 14;
    const double x0 = eg.x, y0 = eg.y, w = eg.w, h = eg.h;
    auto plotX = [&](float frac) { return x0 + w * (double)frac; };
    auto plotY = [&](float level) { return y0 + h * (1.0 - (double)level); };

    cairo_new_path(cr);
    cairo_move_to(cr, plotX(0.0f), plotY(0.0f));

    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float level = envelopeCurveShape(t, curve);
        cairo_line_to(cr, plotX(attackFrac * t), plotY(level));
    }
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float shaped = envelopeCurveShape(t, curve);
        const float level = 1.0f + (sustain - 1.0f) * shaped;
        cairo_line_to(cr, plotX(attackFrac + decayFrac * t), plotY(level));
    }
    cairo_line_to(cr, plotX(attackFrac + decayFrac + holdFrac), plotY(sustain));
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float shaped = envelopeCurveShape(t, curve);
        const float level = sustain * (1.0f - shaped);
        cairo_line_to(cr, plotX(attackFrac + decayFrac + holdFrac + releaseFrac * t), plotY(level));
    }

    setColor(cr, eg.accent, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke_preserve(cr);

    cairo_line_to(cr, plotX(1.0f), plotY(0.0f));
    cairo_line_to(cr, plotX(0.0f), plotY(0.0f));
    cairo_close_path(cr);
    setColor(cr, eg.accent, 0.16);
    cairo_fill(cr);
}

inline void paintSelector(cairo_t* cr, const Selector& sel, float value, int hoverOption)
{
    if (sel.caption != nullptr)
    {
        setFont(cr, 9.0, false);
        setColor(cr, kTextDim);
        drawText(cr, sel.caption, sel.x, sel.y - 6.0);
    }

    const size_t n = sel.options.size();

    for (size_t i = 0; i < n; ++i)
    {
        double ox, optW;
        getSelectorOptionRect(sel, i, ox, optW);
        const bool active = std::fabs(sel.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        roundedRect(cr, ox, sel.y, optW, sel.h, 4.0);
        if (active)
            setColor(cr, sel.accent, 0.9);
        else
            setColor(cr, kBtnBg, hovered ? 1.0 : 0.85);
        cairo_fill_preserve(cr);
        setColor(cr, active ? sel.accent : kPanelEdge);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        setFont(cr, 11.0);
        setColor(cr, active ? kBg : kTextMain);
        centeredText(cr, sel.options[i].label, ox + optW / 2.0, sel.y + sel.h / 2.0);
    }
}

inline const char* findOptionLabel(const std::vector<SelectorOption>& options, float value) noexcept
{
    for (const SelectorOption& opt : options)
        if (std::fabs(opt.value - value) < 0.001f)
            return opt.label;
    return "?";
}

inline void paintDropdownClosed(cairo_t* cr, const Dropdown& dd, float value, bool hovered, bool open)
{
    roundedRect(cr, dd.x, dd.y, dd.w, dd.h, 4.0);
    setColor(cr, kBtnBg, (hovered || open) ? 1.0 : 0.85);
    cairo_fill_preserve(cr);
    setColor(cr, open ? dd.accent : kPanelEdge, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    setFont(cr, 11.0);
    setColor(cr, dd.accent);
    cairo_text_extents_t ext;
    const char* label = findOptionLabel(dd.options, value);
    cairo_text_extents(cr, label, &ext);
    drawText(cr, label, dd.x + 12.0, dd.y + dd.h / 2.0 - ext.height / 2.0 - ext.y_bearing);

    // little down/up arrow on the right to signal "combo box"
    const double ax = dd.x + dd.w - 20.0, ay = dd.y + dd.h / 2.0;
    setColor(cr, kTextDim);
    cairo_move_to(cr, ax - 5.0, open ? ay + 3.0 : ay - 3.0);
    cairo_line_to(cr, ax + 5.0, open ? ay + 3.0 : ay - 3.0);
    cairo_line_to(cr, ax,       open ? ay - 3.0 : ay + 3.0);
    cairo_close_path(cr);
    cairo_fill(cr);
}

inline void paintDropdownOpenList(cairo_t* cr, const Dropdown& dd, float value, int hoverOption, float layoutHeight)
{
    const size_t n = dd.options.size();
    double oyFirst, oyLast, oh;
    getDropdownOptionRect(dd, 0, layoutHeight, oyFirst, oh);
    getDropdownOptionRect(dd, n - 1, layoutHeight, oyLast, oh);
    const double listTop = oyFirst < oyLast ? oyFirst : oyLast;
    const double listBottom = (oyFirst < oyLast ? oyLast : oyFirst) + oh;

    // backdrop so the list reads clearly over whatever is beneath it
    setColor(cr, kBg, 0.92);
    cairo_rectangle(cr, dd.x, listTop, dd.w, listBottom - listTop);
    cairo_fill(cr);

    for (size_t i = 0; i < n; ++i)
    {
        double oy;
        getDropdownOptionRect(dd, i, layoutHeight, oy, oh);
        const bool active = std::fabs(dd.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        cairo_rectangle(cr, dd.x, oy, dd.w, oh);
        if (active)
            setColor(cr, dd.accent, 0.9);
        else
            setColor(cr, kBtnBg, hovered ? 1.0 : 0.0);
        cairo_fill(cr);

        setFont(cr, 10.5);
        setColor(cr, active ? kBg : kTextMain);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, dd.options[i].label, &ext);
        drawText(cr, dd.options[i].label, dd.x + 12.0, oy + oh / 2.0 - ext.height / 2.0 - ext.y_bearing);
    }

    setColor(cr, dd.accent, 0.8);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, dd.x, listTop, dd.w, listBottom - listTop);
    cairo_stroke(cr);
}

inline void paintStepSeq(cairo_t* cr, const StepSeq& seq, const float* values, int hoverColumn, bool dragging)
{
    const int stepCount = (int)(values[seq.countParam] + 0.5f);

    roundedRect(cr, seq.x, seq.y, seq.w, seq.h, 4.0);
    setColor(cr, kBg, 0.55);
    cairo_fill(cr);

    // zero/center line - values are bipolar (-1..1), so bars grow up or
    // down from here rather than from the bottom like a normal level meter
    const double zeroY = seq.y + seq.h * 0.5;
    setColor(cr, kTextDim, 0.5);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, seq.x, zeroY);
    cairo_line_to(cr, seq.x + seq.w, zeroY);
    cairo_stroke(cr);

    const double barGap = 2.0;
    for (int i = 0; i < stepCount; ++i)
    {
        double ox, ow;
        getStepSeqColumnRect(seq, stepCount, i, ox, ow);
        const float v = values[seq.param0 + (uint32_t)i];
        const double barH = (double)std::fabs(v) * (seq.h * 0.5);
        const double barY = v >= 0.0f ? zeroY - barH : zeroY;
        const bool hovered = i == hoverColumn;

        setColor(cr, seq.accent, hovered ? (dragging ? 1.0 : 0.85) : 0.65);
        cairo_rectangle(cr, ox + barGap * 0.5, barY, ow - barGap, barH > 0.0 ? barH : 0.5);
        cairo_fill(cr);
    }

    setColor(cr, kPanelEdge, 0.8);
    cairo_set_line_width(cr, 1.5);
    roundedRect(cr, seq.x, seq.y, seq.w, seq.h, 4.0);
    cairo_stroke(cr);
}

inline void paintButton(cairo_t* cr, const Button& b, bool hovered, bool enabled)
{
    roundedRect(cr, b.x, b.y, b.w, b.h, 4.0);
    setColor(cr, kBtnBg, enabled ? (hovered ? 1.0 : 0.85) : 0.4);
    cairo_fill_preserve(cr);
    setColor(cr, b.accent, enabled ? 1.0 : 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    setFont(cr, 11.5);
    setColor(cr, b.accent, enabled ? 1.0 : 0.35);
    centeredText(cr, b.label, b.x + b.w / 2.0, b.y + b.h / 2.0);
}

inline void paintPresetBar(cairo_t* cr, const PresetBarLayout& pb, const PaintState& state)
{
    paintButton(cr, pb.prev, state.hoverPresetButton == 0, true);
    paintButton(cr, pb.next, state.hoverPresetButton == 1, true);
    paintButton(cr, pb.save, state.hoverPresetButton == 2, true);
    paintButton(cr, pb.del,  state.hoverPresetButton == 3, state.presetDeleteEnabled);

    roundedRect(cr, pb.nameX, pb.nameY, pb.nameW, pb.nameH, 4.0);
    setColor(cr, state.presetEditingName ? kKnobBody : kBtnBg, 0.9);
    cairo_fill_preserve(cr);
    setColor(cr, state.presetEditingName ? pb.accent : kPanelEdge, state.presetEditingName ? 1.0 : 0.8);
    cairo_set_line_width(cr, state.presetEditingName ? 2.0 : 1.5);
    cairo_stroke(cr);

    setFont(cr, 9.0, false);
    setColor(cr, kTextDim);
    drawText(cr, "PRESET", pb.nameX + 12.0, pb.nameY + 14.0);

    setFont(cr, 13.0);
    if (state.presetEditingName)
    {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s_", state.presetEditBuffer);
        setColor(cr, kTextMain);
        drawText(cr, buf, pb.nameX + 12.0, pb.nameY + pb.nameH - 12.0);
    }
    else
    {
        setColor(cr, state.presetIsUnsaved ? kAmber : kTextMain);
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s%s", state.presetName, state.presetIsUnsaved ? " *" : "");
        drawText(cr, buf, pb.nameX + 12.0, pb.nameY + pb.nameH - 12.0);
    }
}

inline void paint(cairo_t* cr, const Layout& L, const PaintState& state)
{
    // background
    setColor(cr, kBg);
    cairo_paint(cr);

    // header
    setFont(cr, 26.0);
    setColor(cr, kCyan);
    drawText(cr, "SIDEOUS", 20.0, 40.0);

    setFont(cr, 10.0, false);
    setColor(cr, kTextDim);
    {
        const char* subtitle = "SID / POKEY VIBE SYNTH";
        cairo_text_extents_t ext;
        cairo_text_extents(cr, subtitle, &ext);
        drawText(cr, subtitle, L.width - 20.0 - ext.width, 24.0);
    }

    paintPresetBar(cr, L.presetBar, state);

    // panels
    for (const PanelBox& p : L.panels)
    {
        roundedRect(cr, p.x, p.y, p.w, p.h, 8.0);
        setColor(cr, kPanelBg);
        cairo_fill_preserve(cr);
        setColor(cr, kPanelEdge, 0.8);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        if (p.title != nullptr)
        {
            setFont(cr, 12.0);
            setColor(cr, p.accent);
            drawText(cr, p.title, p.x + 14.0, p.y + 22.0);
        }
    }

    // envelope graphs (read-only, drawn under the knobs)
    for (const EnvelopeGraph& eg : L.envelopeGraphs)
        paintEnvelopeGraph(cr, eg, state.values);

    // step-sequencer grids
    for (size_t i = 0; i < L.stepSeqs.size(); ++i)
    {
        const StepSeq& seq = L.stepSeqs[i];
        const int hoverColumn = (int)i == state.hoverStepSeq ? state.hoverStepSeqColumn : -1;
        paintStepSeq(cr, seq, state.values, hoverColumn, (int)i == state.dragStepSeq);
    }

    // selectors
    for (size_t i = 0; i < L.selectors.size(); ++i)
    {
        const Selector& sel = L.selectors[i];
        const int hoverOption = (int)i == state.hoverSelector ? state.hoverSelectorOption : -1;
        paintSelector(cr, sel, state.values[sel.param], hoverOption);
    }

    // dropdowns (closed boxes only here; the open list, if any, must draw
    // after the knobs below so it visually overlays them)
    for (size_t i = 0; i < L.dropdowns.size(); ++i)
    {
        const Dropdown& dd = L.dropdowns[i];
        paintDropdownClosed(cr, dd, state.values[dd.param], (int)i == state.hoverDropdown, (int)i == state.openDropdown);
    }

    // knobs
    for (size_t i = 0; i < L.knobs.size(); ++i)
    {
        const Knob& k = L.knobs[i];
        const bool autoShow = state.autoShowValue != nullptr && state.autoShowValue[k.param];
        paintKnob(cr, k, state.values[k.param], (int)i == state.hoverKnob, (int)i == state.dragKnob, autoShow);
    }

    // open dropdown list, drawn last so it overlays whatever is beneath it
    if (state.openDropdown >= 0 && (size_t)state.openDropdown < L.dropdowns.size())
    {
        const Dropdown& dd = L.dropdowns[state.openDropdown];
        paintDropdownOpenList(cr, dd, state.values[dd.param], state.hoverDropdownOption, L.height);
    }
}

} // namespace ui
} // namespace sideous
