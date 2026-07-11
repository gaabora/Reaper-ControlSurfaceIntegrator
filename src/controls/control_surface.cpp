#include "integrator.h"

// ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
static string GetOskSurfaceEnabledSettingsKey(const string& surfaceName) {
    return string("SurfaceEnabled_") + surfaceName;
}

void ControlSurface::LoadOskEnabledSetting() {
    const string key = GetOskSurfaceEnabledSettingsKey(name_);
    if (!::HasExtState("ReaCtrlSurf_OSK_SETTINGS", key.c_str())) return;
    const string value = ::GetExtState("ReaCtrlSurf_OSK_SETTINGS", key.c_str());
    isOskEnabled_ = IsSameString(value.c_str(), "true") || value == "1";
}

void ControlSurface::SetOskEnabled(bool value) {
    isOskEnabled_ = value;
    const string key = GetOskSurfaceEnabledSettingsKey(name_);
    ::SetExtState("ReaCtrlSurf_OSK_SETTINGS", key.c_str(), value ? "true" : "false", true);
}

void ControlSurface::Stop() {
    if (isRewinding_ || isFastForwarding_) CSurf_OnPlay(); // set the cursor to the Play position
    page_->SignalStop();
    CancelRewindAndFastForward();
    CSurf_OnStop();
}

void ControlSurface::Play() {
    page_->SignalPlay();
    CancelRewindAndFastForward();
    CSurf_OnPlay();
}

void ControlSurface::Record() {
    page_->SignalRecord();
    CancelRewindAndFastForward();
    CSurf_OnRecord();
}

void ControlSurface::OnTrackSelection(MediaTrack* track) {
    string onTrackSelection("OnTrackSelection");

    if (widgetsByName_.count(onTrackSelection) > 0) {
        if (GetMediaTrackInfo_Value(track, "I_SELECTED"))
            zoneManager_->DoAction(widgetsByName_[onTrackSelection].get(), 1.0);
        else
            zoneManager_->OnTrackDeselection();
        zoneManager_->OnTrackSelection();
    }
}

void ControlSurface::ForceClearTrack(int trackNum) {
    for (auto widget : widgets_)
        if (widget->GetChannelNumber() + channelOffset_ == trackNum) //TODO +break?
            widget->ForceClear();
}

void ControlSurface::ForceUpdateTrackColors() {
    for (auto trackColorFeedbackProcessor : trackColorFeedbackProcessors_)
        trackColorFeedbackProcessor->ForceUpdateTrackColors();
}

rgba_color ControlSurface::GetTrackColorForChannel(int channel) {
    rgba_color white;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    if (channel < 0 || channel >= numChannels_) return white;

    if (MediaTrack* track = page_->GetTrackNavigationManager()->GetNavigatorForChannel(channel + channelOffset_)->GetTrack())
        return DAW::GetTrackColor(track);
    else
        return white;
}

void ControlSurface::RequestUpdate() {
    for (auto widget : widgets_)
        widget->ClearHasBeenUsedByUpdate();

    zoneManager_->RequestUpdate();

    const PropertyList properties;

    for (auto& widget : widgets_) {
        if (!widget->GetHasBeenUsedByUpdate()) {
            widget->SetHasBeenUsedByUpdate();
            rgba_color color;
            widget->UpdateValue(properties, 0.0);
            widget->UpdateValue(properties, "");
            widget->UpdateColorValue(color);
        }
    }

    if (isRewinding_) {
        if (GetCursorPosition() == 0)
            StopRewinding();
        else {
            CSurf_OnRew(0);

            if (speedX5_ == true) {
                CSurf_OnRew(0);
                CSurf_OnRew(0);
                CSurf_OnRew(0);
                CSurf_OnRew(0);
            }
        }
    }

    else if (isFastForwarding_) {
        if (GetCursorPosition() > GetProjectLength(NULL))
            StopFastForwarding();
        else {
            CSurf_OnFwd(0);

            if (speedX5_ == true) {
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
                CSurf_OnFwd(0);
            }
        }
    }
}

bool ControlSurface::GetModifierState(bool (ModifierManager::*getter)()) {
    if (usesLocalModifiers_ || listensToModifiers_)
        return (modifierManager_.get()->*getter)();
    else
        return (page_->GetModifierManager()->*getter)();
}

bool ControlSurface::GetShift() { return GetModifierState(&ModifierManager::GetShift); }
bool ControlSurface::GetOption() { return GetModifierState(&ModifierManager::GetOption); }
bool ControlSurface::GetControl() { return GetModifierState(&ModifierManager::GetControl); }
bool ControlSurface::GetAlt() { return GetModifierState(&ModifierManager::GetAlt); }
bool ControlSurface::GetFlip() { return GetModifierState(&ModifierManager::GetFlip); }
bool ControlSurface::GetGlobal() { return GetModifierState(&ModifierManager::GetGlobal); }
bool ControlSurface::GetMarker() { return GetModifierState(&ModifierManager::GetMarker); }
bool ControlSurface::GetNudge() { return GetModifierState(&ModifierManager::GetNudge); }
bool ControlSurface::GetZoom() { return GetModifierState(&ModifierManager::GetZoom); }
bool ControlSurface::GetScrub() { return GetModifierState(&ModifierManager::GetScrub); }

void ControlSurface::SetModifierValue(int value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
        modifierManager_->SetModifierValue(value);
    else if (usesLocalModifiers_)
        modifierManager_->SetModifierValue(value);
    else
        page_->GetModifierManager()->SetModifierValue(value);
}

void ControlSurface::SetModifier(void (ModifierManager::*setter)(bool, int), bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        (modifierManager_.get()->*setter)(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                (listener->GetSurface()->GetModifierManager()->*setter)(value, latchTime_);
    } else if (usesLocalModifiers_)
        (modifierManager_.get()->*setter)(value, latchTime_);
    else
        (page_->GetModifierManager()->*setter)(value, latchTime_);
}

void ControlSurface::ApplyToBroadcastModifierListeners(void (ModifierManager::*method)()) {
    for (auto& listener : zoneManager_->GetListeners())
        if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
            (listener->GetSurface()->GetModifierManager()->*method)();
}

void ControlSurface::ApplyToBroadcastModifierListeners(void (ModifierManager::*method)(const char*), const char* argument) {
    for (auto& listener : zoneManager_->GetListeners())
        if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
            (listener->GetSurface()->GetModifierManager()->*method)(argument);
}

void ControlSurface::SetShift(bool value) { SetModifier(&ModifierManager::SetShift, value); }
void ControlSurface::SetOption(bool value) { SetModifier(&ModifierManager::SetOption, value); }
void ControlSurface::SetControl(bool value) { SetModifier(&ModifierManager::SetControl, value); }
void ControlSurface::SetAlt(bool value) { SetModifier(&ModifierManager::SetAlt, value); }
void ControlSurface::SetFlip(bool value) { SetModifier(&ModifierManager::SetFlip, value); }
void ControlSurface::SetGlobal(bool value) { SetModifier(&ModifierManager::SetGlobal, value); }
void ControlSurface::SetMarker(bool value) { SetModifier(&ModifierManager::SetMarker, value); }
void ControlSurface::SetNudge(bool value) { SetModifier(&ModifierManager::SetNudge, value); }
void ControlSurface::SetZoom(bool value) { SetModifier(&ModifierManager::SetZoom, value); }
void ControlSurface::SetScrub(bool value) { SetModifier(&ModifierManager::SetScrub, value); }

const vector<int>& ControlSurface::GetModifiers() {
    if (usesLocalModifiers_ || listensToModifiers_) return modifierManager_->GetModifiers();
    else return page_->GetModifierManager()->GetModifiers();
}

void ControlSurface::ClearModifier(const char* modifier) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->ClearModifier(modifier);
        ApplyToBroadcastModifierListeners(&ModifierManager::ClearModifier, modifier);
    } else if (usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifier(modifier);
    else
        page_->GetModifierManager()->ClearModifier(modifier);
}

void ControlSurface::ClearModifiers() {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->ClearModifiers();
        ApplyToBroadcastModifierListeners(&ModifierManager::ClearModifiers);
    } else if (usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifiers();
    else
        page_->GetModifierManager()->ClearModifiers();
}

//////////////////////////////////////////////////////////////////////////////
// ControlSurface
//////////////////////////////////////////////////////////////////////////////
void ControlSurface::ProcessValues(const vector<vector<string>>& lines) {
    bool inStepSizes = false;
    bool inAccelerationValues = false;

    for (int i = 0; i < (int) lines.size(); ++i) {
        if (lines[i].size() > 0) {
            if (lines[i][0] == "StepSize") { inStepSizes = true; continue; }
            else if (lines[i][0] == "StepSizeEnd") { inStepSizes = false; continue; }
            else if (lines[i][0] == "AccelerationValues") { inAccelerationValues = true; continue; }
            else if (lines[i][0] == "AccelerationValuesEnd") { inAccelerationValues = false; continue; }

            if (lines[i].size() > 1) {
                const string& widgetClass = lines[i][0];

                if (inStepSizes)
                    stepSize_[widgetClass] = atof(lines[i][1].c_str());
                else if (lines[i].size() > 2 && inAccelerationValues) {
                    if (lines[i][1] == "Dec")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValuesForDecrement_[widgetClass][strtol(lines[i][j].c_str(), NULL, 16)] = j - 2;
                    else if (lines[i][1] == "Inc")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValuesForIncrement_[widgetClass][strtol(lines[i][j].c_str(), NULL, 16)] = j - 2;
                    else if (lines[i][1] == "Val")
                        for (int j = 2; j < lines[i].size(); ++j)
                            accelerationValues_[widgetClass].push_back(atof(lines[i][j].c_str()));
                }
            }
        }
    }
}
