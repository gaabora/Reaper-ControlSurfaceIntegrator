#include "integrator.h"

// ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
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

bool ControlSurface::GetShift() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetShift();
    else
        return page_->GetModifierManager()->GetShift();
}

bool ControlSurface::GetOption() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetOption();
    else
        return page_->GetModifierManager()->GetOption();
}

bool ControlSurface::GetControl() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetControl();
    else
        return page_->GetModifierManager()->GetControl();
}

bool ControlSurface::GetAlt() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetAlt();
    else
        return page_->GetModifierManager()->GetAlt();
}

bool ControlSurface::GetFlip() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetFlip();
    else
        return page_->GetModifierManager()->GetFlip();
}

bool ControlSurface::GetGlobal() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetGlobal();
    else
        return page_->GetModifierManager()->GetGlobal();
}

bool ControlSurface::GetMarker() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetMarker();
    else
        return page_->GetModifierManager()->GetMarker();
}

bool ControlSurface::GetNudge() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetNudge();
    else
        return page_->GetModifierManager()->GetNudge();
}

bool ControlSurface::GetZoom() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetZoom();
    else
        return page_->GetModifierManager()->GetZoom();
}

bool ControlSurface::GetScrub() {
    if (usesLocalModifiers_ || listensToModifiers_)
        return modifierManager_->GetScrub();
    else
        return page_->GetModifierManager()->GetScrub();
}

void ControlSurface::SetModifierValue(int value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_)
        modifierManager_->SetModifierValue(value);
    else if (usesLocalModifiers_)
        modifierManager_->SetModifierValue(value);
    else
        page_->GetModifierManager()->SetModifierValue(value);
}
//FIXME: maybe make repeating ControlSurface::Get* ControlSurface::Set* modifier functions more dry?
void ControlSurface::SetShift(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetShift(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetShift(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetShift(value, latchTime_);
    else
        page_->GetModifierManager()->SetShift(value, latchTime_);
}

void ControlSurface::SetOption(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetOption(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetOption(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetOption(value, latchTime_);
    else
        page_->GetModifierManager()->SetOption(value, latchTime_);
}

void ControlSurface::SetControl(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetControl(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetControl(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetControl(value, latchTime_);
    else
        page_->GetModifierManager()->SetControl(value, latchTime_);
}

void ControlSurface::SetAlt(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetAlt(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetAlt(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetAlt(value, latchTime_);
    else
        page_->GetModifierManager()->SetAlt(value, latchTime_);
}

void ControlSurface::SetFlip(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetFlip(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetFlip(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetFlip(value, latchTime_);
    else
        page_->GetModifierManager()->SetFlip(value, latchTime_);
}

void ControlSurface::SetGlobal(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetGlobal(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetGlobal(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetGlobal(value, latchTime_);
    else
        page_->GetModifierManager()->SetGlobal(value, latchTime_);
}

void ControlSurface::SetMarker(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetMarker(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetMarker(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetMarker(value, latchTime_);
    else
        page_->GetModifierManager()->SetMarker(value, latchTime_);
}

void ControlSurface::SetNudge(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetNudge(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetNudge(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetNudge(value, latchTime_);
    else
        page_->GetModifierManager()->SetNudge(value, latchTime_);
}

void ControlSurface::SetZoom(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetZoom(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetZoom(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetZoom(value, latchTime_);
    else
        page_->GetModifierManager()->SetZoom(value, latchTime_);
}

void ControlSurface::SetScrub(bool value) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->SetScrub(value, latchTime_);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->SetScrub(value, latchTime_);
    } else if (usesLocalModifiers_)
        modifierManager_->SetScrub(value, latchTime_);
    else
        page_->GetModifierManager()->SetScrub(value, latchTime_);
}

const vector<int>& ControlSurface::GetModifiers() {
    if (usesLocalModifiers_ || listensToModifiers_) return modifierManager_->GetModifiers();
    else return page_->GetModifierManager()->GetModifiers();
}

void ControlSurface::ClearModifier(const char* modifier) {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->ClearModifier(modifier);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->ClearModifier(modifier);
    } else if (usesLocalModifiers_ || listensToModifiers_)
        modifierManager_->ClearModifier(modifier);
    else
        page_->GetModifierManager()->ClearModifier(modifier);
}

void ControlSurface::ClearModifiers() {
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        modifierManager_->ClearModifiers();

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                listener->GetSurface()->GetModifierManager()->ClearModifiers();
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
