/*
 * Sideous - custom retro-styled Cairo UI. Drawing/layout logic lives in
 * ui/UIPainter.hpp (shared with an offline PNG renderer used during design);
 * this file wires mouse/keyboard input to parameter changes, plus the
 * preset bar's CRUD (create/read/update/delete) - a small file-based preset
 * library (see ui/PresetStore.hpp) that's entirely UI-side, since a preset
 * is just "every parameter's current value" and the UI already owns that.
 */

#include "DistrhoUI.hpp"
#include "Params.hpp"
#include "dsp/LFO.hpp"
#include "ui/FactoryPresets.hpp"
#include "ui/PresetStore.hpp"
#include "ui/UIPainter.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::CairoGraphicsContext;
using namespace sideous;

// -----------------------------------------------------------------------------------------------------------

class SideousUI : public UI
{
public:
    SideousUI()
        : UI()
    {
        fLayout = ui::buildLayout((float)getWidth(), (float)getHeight());

        for (uint32_t i = 0; i < kParamCount; ++i)
            fValues[i] = getParamInfo(i).def;

        fPresetNames = ui::listPresets();
        if (fPresetNames.empty())
        {
            ui::seedFactoryPresets();
            fPresetNames = ui::listPresets();
        }
    }

protected:
    // ---------------------------------------------------------------------

    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= kParamCount)
            return;

        // briefly show a value callout on the knob whenever its value
        // actually changes - covers host automation as well as our own
        // drag/scroll/click edits (which already show it via dragging, so
        // this just harmlessly extends that window a little)
        if (value != fValues[index])
        {
            fValueDisplayUntil[index] = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
            fHasActiveValueDisplay = true;

            // a param moved for a reason other than us applying a preset ->
            // the currently displayed preset (if any) no longer matches
            // what's actually dialed in
            if (!fLoadingPreset)
                fDirty = true;
        }

        fValues[index] = value;
        repaint();
    }

    // called periodically by the host (often at a high, host-defined rate);
    // used only to expire the automation value callouts above. The callout
    // is a static show/hide, not an animation, so there's nothing to redraw
    // while one is simply still counting down - only repaint when the active
    // count actually drops, i.e. some callout just expired and needs erasing.
    // (Repainting unconditionally here used to mean up to ~1.2s of back-to-back
    // repaints, each forcing pugl to recreate its Cairo Xlib surface, after
    // every single knob touch - way more window churn than this feature
    // needs, and worth avoiding on its own even before touching a crash.)
    void uiIdle() override
    {
        if (!fHasActiveValueDisplay)
            return;

        const auto now = std::chrono::steady_clock::now();
        uint32_t activeCount = 0;
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (now < fValueDisplayUntil[i])
                ++activeCount;

        if (activeCount < fLastActiveValueDisplayCount)
            repaint();

        fLastActiveValueDisplayCount = activeCount;
        fHasActiveValueDisplay = activeCount > 0;
    }

    void onCairoDisplay(const CairoGraphicsContext& context) override
    {
        const auto now = std::chrono::steady_clock::now();
        bool autoShow[kParamCount];
        for (uint32_t i = 0; i < kParamCount; ++i)
            autoShow[i] = now < fValueDisplayUntil[i];

        ui::PaintState state;
        state.values = fValues;
        state.hoverKnob = fHoverKnob;
        state.dragKnob = fDragKnob;
        state.hoverSelector = fHoverSelector;
        state.hoverSelectorOption = fHoverSelectorOption;
        state.hoverDropdown = fHoverDropdown;
        state.openDropdown = fOpenDropdown;
        state.hoverDropdownOption = fHoverDropdownOption;
        state.hoverStepSeq = fHoverStepSeq;
        state.hoverStepSeqColumn = fHoverStepSeqColumn;
        state.dragStepSeq = fDragStepSeq;
        state.autoShowValue = autoShow;

        state.presetName = fCurrentName.c_str();
        state.presetIsUnsaved = fDirty;
        state.presetEditingName = fEditingName;
        state.presetEditBuffer = fEditBuffer.c_str();
        state.hoverPresetButton = fHoverPresetButton;
        state.presetDeleteEnabled = fPresetIndex >= 0;

        ui::paint(context.handle, fLayout, state);
    }

    // ---------------------------------------------------------------------
    // input

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;

        const double mx = ev.pos.getX();
        const double my = ev.pos.getY();

        if (ev.press)
        {
            const int presetBtn = hitTestPresetButton(mx, my);
            if (presetBtn >= 0)
            {
                // SAVE must see fEditingName/fEditBuffer as-is - it commits the
                // in-progress rename itself (and clears fEditingName when done).
                // Clearing it here first, like the other buttons do to cancel a
                // pending rename, made SAVE silently discard the typed name and
                // fall back to whatever fCurrentName already was.
                switch (presetBtn)
                {
                case 0: fEditingName = false; presetStep(-1); break;
                case 1: fEditingName = false; presetStep(1);  break;
                case 2: presetSave();   break;
                case 3: fEditingName = false; presetDelete(); break;
                default: fEditingName = false; break;
                }
                repaint();
                return true;
            }

            if (hitTestPresetName(mx, my))
            {
                fEditingName = true;
                fEditBuffer = fCurrentName;
                repaint();
                return true;
            }

            if (fEditingName)
            {
                // clicked elsewhere while editing - cancel without applying,
                // then fall through so the click still does whatever it was
                // actually pointing at (knob/selector/etc)
                fEditingName = false;
                repaint();
            }

            // an open dropdown eats every click until it resolves: either
            // pick the option under the cursor, or just close if elsewhere
            if (fOpenDropdown >= 0)
            {
                const ui::Dropdown& dd = fLayout.dropdowns[fOpenDropdown];
                const int optIdx = hitTestDropdownOption(dd, mx, my);
                if (optIdx >= 0)
                {
                    const float value = dd.options[optIdx].value;
                    editParameter(dd.param, true);
                    setParameterValue(dd.param, value);
                    editParameter(dd.param, false);
                    fValues[dd.param] = value;
                }
                fOpenDropdown = -1;
                repaint();
                return true;
            }

            const int dropdownIdx = hitTestDropdownClosed(mx, my);
            if (dropdownIdx >= 0)
            {
                fOpenDropdown = dropdownIdx;
                repaint();
                return true;
            }

            int selIdx = -1, optIdx = -1;
            if (hitTestSelector(mx, my, selIdx, optIdx))
            {
                const ui::Selector& sel = fLayout.selectors[selIdx];
                const float value = sel.options[optIdx].value;
                if (sel.param == kParamLfoWaveform)
                    applyLfoPreset(kParamLfoWaveform, kParamLfoStepCount, kParamLfoStep1, value);
                else if (sel.param == kParamLfo2Waveform)
                    applyLfoPreset(kParamLfo2Waveform, kParamLfo2StepCount, kParamLfo2Step1, value);
                else
                {
                    editParameter(sel.param, true);
                    setParameterValue(sel.param, value);
                    editParameter(sel.param, false);
                    fValues[sel.param] = value;
                }
                repaint();
                return true;
            }

            int stepSeqIdx = -1, stepSeqCol = -1;
            if (hitTestStepSeq(mx, my, stepSeqIdx, stepSeqCol))
            {
                fDragStepSeq = stepSeqIdx;
                setStepSeqValue(stepSeqIdx, stepSeqCol, my, (ev.mod & kModifierShift) != 0);
                repaint();
                return true;
            }

            const int knobIdx = hitTestKnob(mx, my);
            if (knobIdx < 0)
                return false;

            const ui::Knob& k = fLayout.knobs[knobIdx];

            const auto now = std::chrono::steady_clock::now();
            const bool doubleClick = fLastClickKnob == knobIdx &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - fLastClickTime).count() < 350;
            fLastClickKnob = knobIdx;
            fLastClickTime = now;

            if (doubleClick)
            {
                const float def = getParamInfo(k.param).def;
                editParameter(k.param, true);
                setParameterValue(k.param, def);
                editParameter(k.param, false);
                fValues[k.param] = def;
                fDragKnob = -1;
                repaint();
                return true;
            }

            fDragKnob = knobIdx;
            fDragStartY = my;
            fDragStartT = ui::paramToNormalized(k.param, fValues[k.param]);
            editParameter(k.param, true);
            return true;
        }

        if (fDragKnob >= 0)
        {
            editParameter(fLayout.knobs[fDragKnob].param, false);
            fDragKnob = -1;
            repaint();
            return true;
        }

        if (fDragStepSeq >= 0)
        {
            fDragStepSeq = -1;
            repaint();
            return true;
        }

        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const double mx = ev.pos.getX();
        const double my = ev.pos.getY();

        if (fDragStepSeq >= 0)
        {
            const ui::StepSeq& seq = fLayout.stepSeqs[fDragStepSeq];
            const int stepCount = (int)(fValues[seq.countParam] + 0.5f);
            const int col = stepSeqColumnAt(seq, stepCount, mx);
            setStepSeqValue(fDragStepSeq, col, my, (ev.mod & kModifierShift) != 0);
            repaint();
            return true;
        }

        if (fDragKnob >= 0)
        {
            const ui::Knob& k = fLayout.knobs[fDragKnob];
            // dragging up increases the value; ~200px covers the full sweep
            const double dy = fDragStartY - my;
            float t = fDragStartT + (float)(dy / 200.0);
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

            const float value = ui::normalizedToParam(k.param, t);
            fValues[k.param] = value;
            setParameterValue(k.param, value);
            repaint();
            return true;
        }

        if (fOpenDropdown >= 0)
        {
            const int newHoverOption = hitTestDropdownOption(fLayout.dropdowns[fOpenDropdown], mx, my);
            if (newHoverOption != fHoverDropdownOption)
            {
                fHoverDropdownOption = newHoverOption;
                repaint();
            }
            return true;
        }

        const int newHoverKnob = hitTestKnob(mx, my);
        int selIdx = -1, optIdx = -1;
        hitTestSelector(mx, my, selIdx, optIdx);
        const int newHoverDropdown = hitTestDropdownClosed(mx, my);
        const int newHoverPresetButton = hitTestPresetButton(mx, my);
        int stepSeqIdx = -1, stepSeqCol = -1;
        hitTestStepSeq(mx, my, stepSeqIdx, stepSeqCol);

        if (newHoverKnob != fHoverKnob || selIdx != fHoverSelector || optIdx != fHoverSelectorOption
            || newHoverDropdown != fHoverDropdown || newHoverPresetButton != fHoverPresetButton
            || stepSeqIdx != fHoverStepSeq || stepSeqCol != fHoverStepSeqColumn)
        {
            fHoverKnob = newHoverKnob;
            fHoverSelector = selIdx;
            fHoverSelectorOption = optIdx;
            fHoverDropdown = newHoverDropdown;
            fHoverPresetButton = newHoverPresetButton;
            fHoverStepSeq = stepSeqIdx;
            fHoverStepSeqColumn = stepSeqCol;
            repaint();
        }

        return false;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        if (fOpenDropdown >= 0)
            return false;

        const int knobIdx = hitTestKnob(ev.pos.getX(), ev.pos.getY());
        if (knobIdx < 0)
            return false;

        const ui::Knob& k = fLayout.knobs[knobIdx];
        const float t = ui::paramToNormalized(k.param, fValues[k.param]);
        const float step = 0.02f;
        float newT = t + (ev.delta.getY() > 0.0 ? step : -step);
        newT = newT < 0.0f ? 0.0f : (newT > 1.0f ? 1.0f : newT);

        const float value = ui::normalizedToParam(k.param, newT);
        editParameter(k.param, true);
        setParameterValue(k.param, value);
        editParameter(k.param, false);
        fValues[k.param] = value;
        repaint();
        return true;
    }

    // low-level key events: only used for the few control keys that matter
    // while renaming a preset (Enter/Escape/Backspace); actual typed
    // characters come through onCharacterInput() below, since KeyboardEvent
    // is explicitly documented as not safe to interpret as text input
    // (wrong for anything beyond plain ASCII, doesn't handle IME, etc).
    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (!fEditingName)
            return false;
        if (!ev.press)
            return true; // still swallow releases while editing so nothing else reacts to them

        if (ev.key == kKeyEnter || ev.key == kKeyPadEnter)
        {
            fCurrentName = ui::sanitizePresetName(fEditBuffer);
            fEditingName = false;
            repaint();
            return true;
        }
        if (ev.key == kKeyEscape)
        {
            fEditingName = false;
            repaint();
            return true;
        }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete)
        {
            if (!fEditBuffer.empty())
                fEditBuffer.pop_back();
            repaint();
            return true;
        }

        return true; // swallow everything else while editing (e.g. space) rather than let it leak through
    }

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (!fEditingName)
            return false;

        if (ev.character >= 32 && fEditBuffer.size() < 32) // skip control chars, cap length
            fEditBuffer += ev.string;

        repaint();
        return true;
    }

private:
    int hitTestKnob(double x, double y) const noexcept
    {
        for (size_t i = 0; i < fLayout.knobs.size(); ++i)
        {
            const ui::Knob& k = fLayout.knobs[i];
            const double dx = x - k.cx, dy = y - k.cy;
            const double reach = k.radius + 6.0;
            if (dx * dx + dy * dy <= reach * reach)
                return (int)i;
        }
        return -1;
    }

    int hitTestDropdownClosed(double x, double y) const noexcept
    {
        for (size_t i = 0; i < fLayout.dropdowns.size(); ++i)
        {
            const ui::Dropdown& dd = fLayout.dropdowns[i];
            if (x >= dd.x && x <= dd.x + dd.w && y >= dd.y && y <= dd.y + dd.h)
                return (int)i;
        }
        return -1;
    }

    int hitTestDropdownOption(const ui::Dropdown& dd, double x, double y) const noexcept
    {
        if (x < dd.x || x > dd.x + dd.w)
            return -1;

        for (size_t i = 0; i < dd.options.size(); ++i)
        {
            double oy, oh;
            ui::getDropdownOptionRect(dd, i, (float)fLayout.height, oy, oh);
            if (y >= oy && y <= oy + oh)
                return (int)i;
        }
        return -1;
    }

    bool hitTestSelector(double x, double y, int& selIdx, int& optIdx) const noexcept
    {
        selIdx = -1;
        optIdx = -1;

        for (size_t i = 0; i < fLayout.selectors.size(); ++i)
        {
            const ui::Selector& sel = fLayout.selectors[i];
            if (x < sel.x || x > sel.x + sel.w || y < sel.y || y > sel.y + sel.h)
                continue;

            for (size_t j = 0; j < sel.options.size(); ++j)
            {
                double ox, ow;
                ui::getSelectorOptionRect(sel, j, ox, ow);
                if (x >= ox && x <= ox + ow)
                {
                    selIdx = (int)i;
                    optIdx = (int)j;
                    return true;
                }
            }
        }
        return false;
    }

    bool hitTestStepSeq(double x, double y, int& seqIdx, int& colIdx) const noexcept
    {
        seqIdx = -1;
        colIdx = -1;

        for (size_t i = 0; i < fLayout.stepSeqs.size(); ++i)
        {
            const ui::StepSeq& seq = fLayout.stepSeqs[i];
            if (x < seq.x || x > seq.x + seq.w || y < seq.y || y > seq.y + seq.h)
                continue;

            const int stepCount = (int)(fValues[seq.countParam] + 0.5f);
            seqIdx = (int)i;
            colIdx = stepSeqColumnAt(seq, stepCount, x);
            return true;
        }
        return false;
    }

    // clamped rather than a strict hit-test: lets a drag that strays slightly
    // past the grid's left/right edge keep painting the boundary column
    // instead of dropping the gesture.
    static int stepSeqColumnAt(const ui::StepSeq& seq, int stepCount, double x) noexcept
    {
        double rel = (x - seq.x) / seq.w;
        rel = rel < 0.0 ? 0.0 : (rel > 0.9999 ? 0.9999 : rel);
        int col = (int)(rel * (double)stepCount);
        return col < 0 ? 0 : (col >= stepCount ? stepCount - 1 : col);
    }

    // vertical mouse position -> step value (top = +1, bottom = -1); Shift
    // quantizes to twelfths, so at LFO Amount=1 with Destination=Pitch a
    // hand-drawn step lands exactly on a semitone.
    void setStepSeqValue(int seqIdx, int colIdx, double my, bool quantize) noexcept
    {
        const ui::StepSeq& seq = fLayout.stepSeqs[seqIdx];
        const uint32_t param = seq.param0 + (uint32_t)colIdx;

        float t = (float)((seq.y + seq.h - my) / seq.h);
        float value = t * 2.0f - 1.0f;
        value = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
        if (quantize)
        {
            value = std::round(value * 12.0f) / 12.0f;
            value = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
        }

        editParameter(param, true);
        setParameterValue(param, value);
        editParameter(param, false);
        fValues[param] = value;
    }

    // fills an LFO's step array from a one-click shape preset. Computed
    // locally (not just delegated to the DSP side, which does the same fill
    // in SideousPlugin.cpp's setParameterValue()) so this UI's own fValues[]
    // cache - and therefore what's immediately drawn - reflects the new
    // shape without waiting on a round-trip through the host/audio thread.
    void applyLfoPreset(uint32_t waveParam, uint32_t stepCountParam, uint32_t step1Param, float presetValue) noexcept
    {
        const int stepCount = (int)(fValues[stepCountParam] + 0.5f);
        float steps[sideous::kLfoMaxSteps];
        sideous::fillLfoStepPreset((sideous::LfoStepPreset)(int)(presetValue + 0.5f), stepCount, steps);

        editParameter(waveParam, true);
        setParameterValue(waveParam, presetValue);
        editParameter(waveParam, false);
        fValues[waveParam] = presetValue;

        for (int i = 0; i < stepCount; ++i)
        {
            const uint32_t param = step1Param + (uint32_t)i;
            editParameter(param, true);
            setParameterValue(param, steps[i]);
            editParameter(param, false);
            fValues[param] = steps[i];
        }
    }

    static bool insideButton(const ui::Button& b, double x, double y) noexcept
    {
        return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
    }

    // 0=prev, 1=next, 2=save, 3=delete, or -1. Delete only hit-tests
    // positive when a real preset (not INIT) is currently loaded - nothing
    // to delete otherwise.
    int hitTestPresetButton(double x, double y) const noexcept
    {
        const ui::PresetBarLayout& pb = fLayout.presetBar;
        if (insideButton(pb.prev, x, y)) return 0;
        if (insideButton(pb.next, x, y)) return 1;
        if (insideButton(pb.save, x, y)) return 2;
        if (insideButton(pb.del, x, y) && fPresetIndex >= 0) return 3;
        return -1;
    }

    bool hitTestPresetName(double x, double y) const noexcept
    {
        const ui::PresetBarLayout& pb = fLayout.presetBar;
        return x >= pb.nameX && x <= pb.nameX + pb.nameW && y >= pb.nameY && y <= pb.nameY + pb.nameH;
    }

    // ---------------------------------------------------------------------
    // preset CRUD - index < 0 means INIT (compiled-in defaults, no file)

    void loadPresetAt(int index) noexcept
    {
        fLoadingPreset = true;

        if (index < 0 || index >= (int)fPresetNames.size())
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
                fValues[i] = getParamInfo(i).def;
            fCurrentName = "INIT";
            fPresetIndex = -1;
        }
        else
        {
            ui::loadPreset(fPresetNames[(size_t)index], fValues);
            fCurrentName = fPresetNames[(size_t)index];
            fPresetIndex = index;
        }

        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            editParameter(i, true);
            setParameterValue(i, fValues[i]);
            editParameter(i, false);
        }

        fDirty = false;
        fLoadingPreset = false;
        repaint();
    }

    // cyclical Prev(-1)/Next(+1) through [INIT, preset0, preset1, ...]
    void presetStep(int dir) noexcept
    {
        const int total = (int)fPresetNames.size() + 1;
        int slot = fPresetIndex + 1; // 0 = INIT, 1..N = fPresetNames[0..N-1]
        slot = ((slot + dir) % total + total) % total;
        loadPresetAt(slot - 1);
    }

    void presetSave() noexcept
    {
        std::string name = fEditingName ? ui::sanitizePresetName(fEditBuffer) : fCurrentName;
        if (name.empty() || name == "INIT")
            name = "New Preset";

        ui::savePreset(name, fValues);
        fPresetNames = ui::listPresets();
        fCurrentName = name;
        fEditingName = false;
        fDirty = false;

        fPresetIndex = -1;
        for (size_t i = 0; i < fPresetNames.size(); ++i)
        {
            if (fPresetNames[i] == name)
            {
                fPresetIndex = (int)i;
                break;
            }
        }
    }

    void presetDelete() noexcept
    {
        if (fPresetIndex < 0)
            return;

        ui::deletePreset(fCurrentName);
        fPresetNames = ui::listPresets();

        const int newIndex = fPresetIndex < (int)fPresetNames.size() ? fPresetIndex : (int)fPresetNames.size() - 1;
        loadPresetAt(newIndex);
    }

    ui::Layout fLayout;
    float fValues[kParamCount];

    std::chrono::steady_clock::time_point fValueDisplayUntil[kParamCount]{};
    bool fHasActiveValueDisplay = false;
    uint32_t fLastActiveValueDisplayCount = 0;

    int fDragKnob = -1;
    double fDragStartY = 0.0;
    float fDragStartT = 0.0f;

    int fHoverKnob = -1;
    int fHoverSelector = -1;
    int fHoverSelectorOption = -1;

    int fHoverDropdown = -1;
    int fOpenDropdown = -1;
    int fHoverDropdownOption = -1;

    int fHoverStepSeq = -1;
    int fHoverStepSeqColumn = -1;
    int fDragStepSeq = -1;

    int fLastClickKnob = -1;
    std::chrono::steady_clock::time_point fLastClickTime{};

    std::vector<std::string> fPresetNames; // sorted, ui::listPresets() order
    int fPresetIndex = -1;                 // -1 = INIT, else index into fPresetNames
    std::string fCurrentName = "INIT";
    bool fDirty = false;                   // true once a param has moved since the last load/save
    bool fLoadingPreset = false;           // guards parameterChanged() while loadPresetAt() is applying values
    bool fEditingName = false;
    std::string fEditBuffer;
    int fHoverPresetButton = -1;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideousUI)
};

// -----------------------------------------------------------------------------------------------------------

UI* createUI()
{
    return new SideousUI();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
