#pragma once
//
//  modifier_manager.h — ModifierManager + ChannelTouch + ChannelToggle structs
//
#include "preamble.h"

enum class ModifierMode {
    Momentary,
    Latch,
    Hybrid,
};

class ModifierManager
{
private:
    CSurfIntegrator* const csi_;
    IPageContext* page_;
    ControlSurface* surface_;

    enum Modifiers {
        ErrorModifier = -1,
        Shift = 0,
        Option,
        Control,
        Alt,
        Flip,
        Global,
        Marker,
        Nudge,
        Zoom,
        Scrub,
        MaxModifiers
    };

    static int maskFromModifier(Modifiers m) {
        if (WDL_NOT_NORMALLY(m == ErrorModifier)) return 0;
        return 4 << (int) m;
    }

    static Modifiers modifierFromString(const char* s) {
        if (IsSameString(s, "Shift")) return Shift;
        if (IsSameString(s, "Option")) return Option;
        if (IsSameString(s, "Control")) return Control;
        if (IsSameString(s, "Alt")) return Alt;
        if (IsSameString(s, "Flip")) return Flip;
        if (IsSameString(s, "Global")) return Global;
        if (IsSameString(s, "Marker")) return Marker;
        if (IsSameString(s, "Nudge")) return Nudge;
        if (IsSameString(s, "Zoom")) return Zoom;
        if (IsSameString(s, "Scrub")) return Scrub;
        return ErrorModifier;
    }

    static const char* stringFromModifier(Modifiers mod) {
        switch (mod) {
            case Shift: return "Shift";
            case Option: return "Option";
            case Control: return "Control";
            case Alt: return "Alt";
            case Flip: return "Flip";
            case Global: return "Global";
            case Marker: return "Marker";
            case Nudge: return "Nudge";
            case Zoom: return "Zoom";
            case Scrub: return "Scrub";
            default:
                WDL_ASSERT(false);
                return "";
        }
    }

    struct ModifierState {
        bool isEngaged;
        bool isLocked;
        DWORD pressedTime;
    };

    ModifierState modifiers_[MaxModifiers];

    WDL_TypedBuf<int> modifierCombinations_;
    vector<int> modifierVector_;

    static int intcmp_rev(const void* a, const void* b) { return *(const int*) a > *(const int*) b ? -1 : *(const int*) a < *(const int*) b ? 1 : 0; }

    void GetCombinations(const Modifiers* indices, int num_indices, WDL_TypedBuf<int>& combinations) {
        for (int mask = 0; mask < (1 << num_indices); ++mask) {
            int combination = 0;
            for (int position = 0; position < num_indices; ++position)
                if (mask & (1 << position))
                    combination |= maskFromModifier(indices[position]);
            if (combination != 0)
                combinations.Add(combination);
        }
    }

    void SetLatchModifier(bool value, Modifiers modifier, int latchTime);
    void SetModifier(bool value, Modifiers modifier, int latchTime, ModifierMode mode);

public:
    ModifierManager(CSurfIntegrator* const csi, IPageContext* page = nullptr, ControlSurface* surface = NULL)
        : csi_(csi), page_(page), surface_(surface) {
        int* p = modifierCombinations_.ResizeOK(1);
        if (WDL_NORMALLY(p)) p[0] = 0;
        modifierVector_.push_back(0);
        memset(modifiers_, 0, sizeof(modifiers_));
    }

    void RecalculateModifiers();
    const vector<int>& GetModifiers() { return modifierVector_; }

    bool GetShift() { return modifiers_[Shift].isEngaged; }
    bool GetOption() { return modifiers_[Option].isEngaged; }
    bool GetControl() { return modifiers_[Control].isEngaged; }
    bool GetAlt() { return modifiers_[Alt].isEngaged; }
    bool GetFlip() { return modifiers_[Flip].isEngaged; }
    bool GetGlobal() { return modifiers_[Global].isEngaged; }
    bool GetMarker() { return modifiers_[Marker].isEngaged; }
    bool GetNudge() { return modifiers_[Nudge].isEngaged; }
    bool GetZoom() { return modifiers_[Zoom].isEngaged; }
    bool GetScrub() { return modifiers_[Scrub].isEngaged; }

    void ClearModifier(const char* modifierString) {
        Modifiers m = modifierFromString(modifierString);
        if (m != ErrorModifier) {
            modifiers_[m].isEngaged = false;
            RecalculateModifiers();
        }
    }

    static char* GetModifierString(int modifierValue, char* buf, int bufsz) {
        buf[0] = 0;
        for (int x = 0; x < MaxModifiers; ++x)
            if (modifierValue & maskFromModifier((Modifiers) x))
                snprintf_append(buf, bufsz, "%s+", stringFromModifier((Modifiers) x));

        return buf;
    }

    static bool IsModifierName(const char* name) {
        return modifierFromString(name) != ErrorModifier;
    }

    int GetModifierValue(const char* modifierString) {
        vector<string> modifierTokens;
        GetTokens(modifierTokens, modifierString, '+');
        return GetModifierValue(modifierTokens);
    }

    int GetModifierValue(const vector<string>& tokens) {
        int modifierValue = 0;
        for (int i = 0; i < tokens.size(); ++i) {
            Modifiers m = modifierFromString(tokens[i].c_str());
            if (m != ErrorModifier)
                modifierValue |= maskFromModifier(m);
        }

        return modifierValue;
    }

    void SetModifierValue(int value) {
        for (int i = 0; i < MaxModifiers; ++i)
            modifiers_[i].isEngaged = false;

        if (value & maskFromModifier(Shift)) modifiers_[Shift].isEngaged = true;
        if (value & maskFromModifier(Option)) modifiers_[Option].isEngaged = true;
        if (value & maskFromModifier(Control)) modifiers_[Control].isEngaged = true;
        if (value & maskFromModifier(Alt)) modifiers_[Alt].isEngaged = true;
        if (value & maskFromModifier(Flip)) modifiers_[Flip].isEngaged = true;
        if (value & maskFromModifier(Global)) modifiers_[Global].isEngaged = true;
        if (value & maskFromModifier(Marker)) modifiers_[Marker].isEngaged = true;
        if (value & maskFromModifier(Nudge)) modifiers_[Nudge].isEngaged = true;
        if (value & maskFromModifier(Zoom)) modifiers_[Zoom].isEngaged = true;
        if (value & maskFromModifier(Scrub)) modifiers_[Scrub].isEngaged = true;
    }

    void SetShift(bool value, int latchTime, ModifierMode mode) { SetModifier(value, Shift, latchTime, mode); }

    void SetOption(bool value, int latchTime, ModifierMode mode) { SetModifier(value, Option, latchTime, mode); }
    void SetControl(bool value, int latchTime, ModifierMode mode) { SetModifier(value, Control, latchTime, mode); }
    void SetAlt(bool value, int latchTime, ModifierMode mode) { SetModifier(value, Alt, latchTime, mode); }
    void SetFlip(bool value, int latchTime, ModifierMode mode) { SetModifier(value, Flip, latchTime, mode); }
    void SetGlobal(bool value, int latchTime, ModifierMode mode) { SetModifier(value, Global, latchTime, mode); }
    
    void SetMarker(bool value, int latchTime, ModifierMode mode) {
        modifiers_[Nudge].isEngaged = false;
        modifiers_[Zoom].isEngaged = false;
        modifiers_[Scrub].isEngaged = false;
        SetModifier(value, Marker, latchTime, mode);
    }

    void SetNudge(bool value, int latchTime, ModifierMode mode) {
        modifiers_[Marker].isEngaged = false;
        modifiers_[Zoom].isEngaged = false;
        modifiers_[Scrub].isEngaged = false;
        SetModifier(value, Nudge, latchTime, mode);
    }

    void SetZoom(bool value, int latchTime, ModifierMode mode) {
        modifiers_[Marker].isEngaged = false;
        modifiers_[Nudge].isEngaged = false;
        modifiers_[Scrub].isEngaged = false;
        SetModifier(value, Zoom, latchTime, mode);
    }

    void SetScrub(bool value, int latchTime, ModifierMode mode) {
        modifiers_[Marker].isEngaged = false;
        modifiers_[Nudge].isEngaged = false;
        modifiers_[Zoom].isEngaged = false;
        SetModifier(value, Scrub, latchTime, mode);
    }

    void ClearModifiers() {
        for (int i = 0; i < MaxModifiers; ++i)
            modifiers_[i].isEngaged = false;
        RecalculateModifiers();
    }
};

struct ChannelTouch {
    int channelNum = 0;
    bool isTouched = false;
    ChannelTouch() {}
};

struct ChannelToggle {
    int channelNum = 0;
    bool isToggled = false;
    ChannelToggle() {}
};
