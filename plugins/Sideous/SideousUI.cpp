/*
 * Sideous - custom retro-styled Cairo UI. All drawing/layout logic lives in
 * ui/UIPainter.hpp (shared with an offline PNG renderer used during design);
 * this file only wires mouse input to parameter changes.
 */

#include "DistrhoUI.hpp"
#include "Params.hpp"
#include "ui/UIPainter.hpp"

#include <chrono>

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
        }

        fValues[index] = value;
        repaint();
    }

    // called periodically by the host; used only to expire the automation
    // value callouts above (repaint() itself is otherwise purely event-driven)
    void uiIdle() override
    {
        if (!fHasActiveValueDisplay)
            return;

        repaint();

        const auto now = std::chrono::steady_clock::now();
        bool anyActive = false;
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            if (now < fValueDisplayUntil[i])
            {
                anyActive = true;
                break;
            }
        }
        fHasActiveValueDisplay = anyActive;
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
        state.autoShowValue = autoShow;

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
                editParameter(sel.param, true);
                setParameterValue(sel.param, value);
                editParameter(sel.param, false);
                fValues[sel.param] = value;
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

        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const double mx = ev.pos.getX();
        const double my = ev.pos.getY();

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

        if (newHoverKnob != fHoverKnob || selIdx != fHoverSelector || optIdx != fHoverSelectorOption
            || newHoverDropdown != fHoverDropdown)
        {
            fHoverKnob = newHoverKnob;
            fHoverSelector = selIdx;
            fHoverSelectorOption = optIdx;
            fHoverDropdown = newHoverDropdown;
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

    ui::Layout fLayout;
    float fValues[kParamCount];

    std::chrono::steady_clock::time_point fValueDisplayUntil[kParamCount]{};
    bool fHasActiveValueDisplay = false;

    int fDragKnob = -1;
    double fDragStartY = 0.0;
    float fDragStartT = 0.0f;

    int fHoverKnob = -1;
    int fHoverSelector = -1;
    int fHoverSelectorOption = -1;

    int fHoverDropdown = -1;
    int fOpenDropdown = -1;
    int fHoverDropdownOption = -1;

    int fLastClickKnob = -1;
    std::chrono::steady_clock::time_point fLastClickTime{};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideousUI)
};

// -----------------------------------------------------------------------------------------------------------

UI* createUI()
{
    return new SideousUI();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
