#include "integrator.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ModifierManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ModifierManager::RecalculateModifiers() {
    if (surface_ == NULL && page_ == NULL) return;

    if (modifierCombinations_.ResizeOK(1, false))
        modifierCombinations_.Get()[0] = 0;
    Modifiers activeModifierIndices[MaxModifiers];
    int activeModifierIndices_cnt = 0;

    for (int i = 0; i < MaxModifiers; ++i)
        if (modifiers_[i].isEngaged)
            activeModifierIndices[activeModifierIndices_cnt++] = (Modifiers) i;

    if (activeModifierIndices_cnt > 0) {
        GetCombinations(activeModifierIndices, activeModifierIndices_cnt, modifierCombinations_);
        qsort(modifierCombinations_.Get(), modifierCombinations_.GetSize(), sizeof(modifierCombinations_.Get()[0]), intcmp_rev);
    }

    modifierVector_.clear();

    for (int i = 0; i < modifierCombinations_.GetSize(); ++i)
        modifierVector_.push_back(modifierCombinations_.Get()[i]);
    if (surface_ != NULL)
        surface_->GetZoneManager()->UpdateCurrentActionContextModifiers();
    else if (page_ != NULL)
        page_->UpdateCurrentActionContextModifiers();
}

void ModifierManager::SetLatchModifier(bool value, Modifiers modifier, int latchTime) {
    if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
        const char* modifierName = stringFromModifier(modifier);
        DWORD keyReleasedTime = GetTickCount();
        DWORD heldTime = keyReleasedTime - modifiers_[modifier].pressedTime;
        if (heldTime >= (DWORD) latchTime) {
            if (modifiers_[modifier].isLocked == true) {
                modifiers_[modifier].isLocked = false;
                if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] [%s] UNLOCK\n", modifierName);
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s Unlock", modifierName);
                csi_->Speak(tmp);
            }
            modifiers_[modifier].isEngaged = false;
        } else {
            auto modifierName = stringFromModifier(modifier);
            if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] [%s] [LOCK]\n", modifierName);
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "%s Lock", modifierName);
            csi_->Speak(tmp);
            modifiers_[modifier].isLocked = true;
        }
        modifiers_[modifier].pressedTime = 0;
    } else {
        if (modifiers_[modifier].isEngaged == false) {
            modifiers_[modifier].isEngaged = true;
            modifiers_[modifier].pressedTime = GetTickCount();
        } else {
            modifiers_[modifier].pressedTime = 0;
        }
    }
    RecalculateModifiers();
}

void ModifierManager::SetModifier(bool value, Modifiers modifier, int latchTime, ModifierMode mode, ModifierEvent inputEvent) {
    if (inputEvent == ModifierEvent::Raw) {
        this->SetLatchModifier(value, modifier, latchTime);
        return;
    }

    ModifierState& state = this->modifiers_[modifier];
    if (mode == ModifierMode::Momentary) {
        state.isEngaged = inputEvent == ModifierEvent::Press;
        state.isLocked = false;
        state.pressedTime = state.isEngaged ? GetTickCount() : 0;
    } else if (mode == ModifierMode::Latch && inputEvent == ModifierEvent::Tap) {
        state.isEngaged = !state.isEngaged;
        state.isLocked = state.isEngaged;
        state.pressedTime = 0;
    } else if (mode == ModifierMode::Hybrid && inputEvent == ModifierEvent::Press) {
        if (!state.isLocked) state.isEngaged = true;
    } else if (mode == ModifierMode::Hybrid && inputEvent == ModifierEvent::Tap) {
        state.isLocked = !state.isLocked;
        state.isEngaged = state.isLocked;
    } else if (mode == ModifierMode::Hybrid && inputEvent == ModifierEvent::Release) {
        state.isLocked = false;
        state.isEngaged = false;
    }
    this->RecalculateModifiers();
}
