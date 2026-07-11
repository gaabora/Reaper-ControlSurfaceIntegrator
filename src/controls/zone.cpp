#include "integrator.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Zone
///////////////////////////////////////////////////////////////////////////////////////////////////////
void Zone::InitSubZones(const vector<string>& subZones, const char* widgetSuffix) {
    map<const string, ZoneInfo>& zoneInfo = zoneManager_->GetZoneInfo();

    for (int i = 0; i < (int) subZones.size(); ++i) {
        if (zoneInfo.find(subZones[i]) != zoneInfo.end()) {
            subZones_.push_back(make_unique<SubZone>(csi_, zoneManager_, GetNavigator(), GetSlotIndex(), subZones[i], zoneInfo[subZones[i]].alias, zoneInfo[subZones[i]].filePath, this));
            zoneManager_->LoadZoneFile(subZones_.back().get(), widgetSuffix);
        }
    }
}

int Zone::GetSlotIndex() {
    if (name_ == "TrackSend") return zoneManager_->GetTrackSendOffset();
    if (name_ == "TrackReceive") return zoneManager_->GetTrackReceiveOffset();
    if (name_ == "TrackFXMenu") return zoneManager_->GetTrackFXMenuOffset();
    if (name_ == "SelectedTrack") return slotIndex_;
    if (name_ == "SelectedTrackSend") return slotIndex_ + zoneManager_->GetSelectedTrackSendOffset();
    if (name_ == "SelectedTrackReceive") return slotIndex_ + zoneManager_->GetSelectedTrackReceiveOffset();
    if (name_ == "SelectedTrackFXMenu") return slotIndex_ + zoneManager_->GetSelectedTrackFXMenuOffset();
    if (name_ == "MasterTrackFXMenu") return slotIndex_ + zoneManager_->GetMasterTrackFXMenuOffset();
    else
        return slotIndex_;
}

void Zone::AddWidget(Widget* widget) {
    if (find(widgets_.begin(), widgets_.end(), widget) == widgets_.end())
        widgets_.push_back(widget);
}

void Zone::Activate() {
    UpdateCurrentActionContextModifiers();
    //TODO: fix WidgetN forme HomeZone stops working if subzone has Shift+WidgetN but no WidgetN / subsone requires redefining WidgetN if there are WidgetN+ModifierX
    for (auto& widget : widgets_) {
        if (IsSameString(widget->GetName(), "OnZoneActivation"))
            for (auto& actionContext : GetActionContexts(widget))
                actionContext->DoAction(1.0);

        widget->Configure(GetActionContexts(widget));
    }

    isActive_ = true;

    if (IsSameString(GetName(), "VCA"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->ActivateVCAMode();
    else if (IsSameString(GetName(), "Folder"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->ActivateFolderMode();
    else if (IsSameString(GetName(), "SelectedTracks"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->ActivateSelectedTracksMode();

    zoneManager_->GetSurface()->SendOSCMessage(GetName());

    for (auto& subZone : subZones_)
        subZone->Deactivate();

    for (auto& includedZone : includedZones_)
        includedZone->Activate();
}

void Zone::Deactivate() {
    if (!isActive_) return;
    for (auto& widget : widgets_) {
        for (auto& actionContext : GetActionContexts(widget)) {
            actionContext->UpdateWidgetValue(0.0);
            actionContext->UpdateWidgetValue("");

            if (IsSameString(widget->GetName(), "OnZoneDeactivation"))
                actionContext->DoAction(1.0);
        }
    }

    isActive_ = false;

    if (IsSameString(GetName(), "VCA"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->DeactivateVCAMode();
    else if (IsSameString(GetName(), "Folder"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->DeactivateFolderMode();
    else if (IsSameString(GetName(), "SelectedTracks"))
        zoneManager_->GetSurface()->GetPage()->GetTrackNavigationManager()->DeactivateSelectedTracksMode();

    for (auto& includedZone : includedZones_)
        includedZone->Deactivate();

    for (auto& subZone : subZones_)
        subZone->Deactivate();
}

void Zone::RequestUpdate() {
    if (!isActive_) return;

    for (auto& subZone : subZones_)
        subZone->RequestUpdate();

    for (auto& includedZone : includedZones_)
        includedZone->RequestUpdate();

    for (auto widget : widgets_) {
        if (!widget->GetHasBeenUsedByUpdate()) {
            widget->SetHasBeenUsedByUpdate();
            RequestUpdateWidget(widget);
        }
    }
}

void Zone::SetXTouchDisplayColors(const char* colors) {
    for (auto& widget : widgets_)
        widget->SetXTouchDisplayColors(colors);
}

void Zone::RestoreXTouchDisplayColors() {
    for (auto& widget : widgets_)
        widget->RestoreXTouchDisplayColors();
}

void Zone::DoAction(Widget* widget, bool& isUsed, double value) {
    if (!isActive_ || isUsed) return;

    for (auto& subZone : subZones_)
        subZone->DoAction(widget, isUsed, value);

    if (isUsed) return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end()) {
        isUsed = true;

        for (auto& actionContext : GetActionContexts(widget))
            actionContext->DoAction(value);
    } else {
        for (auto& includedZone : includedZones_)
            includedZone->DoAction(widget, isUsed, value);
    }
}

void Zone::DoRelativeAction(Widget* widget, bool& isUsed, double delta) {
    if (!isActive_ || isUsed) return;

    for (auto& subZone : subZones_)
        subZone->DoRelativeAction(widget, isUsed, delta);

    if (isUsed) return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end()) {
        isUsed = true;

        for (auto& actionContext : GetActionContexts(widget))
            actionContext->DoRelativeAction(delta);
    } else {
        for (auto& includedZone : includedZones_)
            includedZone->DoRelativeAction(widget, isUsed, delta);
    }
}

void Zone::DoRelativeAction(Widget* widget, bool& isUsed, int accelerationIndex, double delta) {
    if (!isActive_ || isUsed) return;

    for (auto& subZone : subZones_)
        subZone->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed) return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end()) {
        isUsed = true;

        for (auto& actionContext : GetActionContexts(widget))
            actionContext->DoRelativeAction(accelerationIndex, delta);
    } else {
        for (auto& includedZone : includedZones_)
            includedZone->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
    }
}

void Zone::DoTouch(Widget* widget, const char* widgetName, bool& isUsed, double value) {
    if (!isActive_ || isUsed) return;

    for (auto& subZone : subZones_)
        subZone->DoTouch(widget, widgetName, isUsed, value);

    if (isUsed) return;

    if (find(widgets_.begin(), widgets_.end(), widget) != widgets_.end()) {
        isUsed = true;

        for (auto& actionContext : GetActionContexts(widget))
            actionContext->DoTouch(value);
    } else {
        for (auto& includedZone : includedZones_)
            includedZone->DoTouch(widget, widgetName, isUsed, value);
    }
}

void Zone::UpdateCurrentActionContextModifiers() {
    for (auto& widget : widgets_) {
        UpdateCurrentActionContextModifier(widget);
        widget->Configure(GetActionContexts(widget, currentActionContextModifiers_[widget]));
    }

    for (auto& includedZone : includedZones_)
        includedZone->UpdateCurrentActionContextModifiers();

    for (auto& subZone : subZones_)
        subZone->UpdateCurrentActionContextModifiers();
}

void Zone::UpdateCurrentActionContextModifier(Widget* widget) {
    for (int i = 0; i < (int) widget->GetSurface()->GetModifiers().size(); ++i) {
        if (actionContextDictionary_[widget].count(widget->GetSurface()->GetModifiers()[i]) > 0) {
            currentActionContextModifiers_[widget] = widget->GetSurface()->GetModifiers()[i];
            break;
        }
    }
}

ActionContext* Zone::AddActionContext(Widget* widget, int modifier, Zone* zone, const char* actionName, vector<string>& params) {
    const auto& action = csi_->GetAction(actionName);
    if (!IsSameString(action->GetName(), actionName) && IsSameString(action->GetName(), "InvalidAction"))
        LogToConsole("[ERROR] @%s/{%s} [%s] InvalidAction: %s\n", widget->GetSurface()->GetName(), zone->GetName(), widget->GetName(), actionName);

    if (action->IsModifier())
        widget->SetIsModifier();

    actionContextDictionary_[widget][modifier].push_back(make_unique<ActionContext>(csi_, action, widget, zone, 0, params));

    return actionContextDictionary_[widget][modifier].back().get();
}

void Zone::ClearActionContexts(Widget* widget) {
    if (!widget) return;
    actionContextDictionary_.erase(widget);
    currentActionContextModifiers_.erase(widget);
}

const vector<unique_ptr<ActionContext>>& Zone::GetActionContexts(Widget* widget) {
    if (currentActionContextModifiers_.count(widget) == 0) UpdateCurrentActionContextModifier(widget);

    bool isTouched = false;
    bool isToggled = false;

    if (widget->GetSurface()->GetIsChannelTouched(widget->GetChannelNumber())) isTouched = true;

    if (widget->GetSurface()->GetIsChannelToggled(widget->GetChannelNumber())) isToggled = true;

    if (currentActionContextModifiers_.count(widget) > 0 && actionContextDictionary_.count(widget) > 0) {
        int modifier = currentActionContextModifiers_[widget];

        if (isTouched && isToggled && actionContextDictionary_[widget].count(modifier + 3) > 0)
            return actionContextDictionary_[widget][modifier + 3];
        else if (isTouched && actionContextDictionary_[widget].count(modifier + 1) > 0)
            return actionContextDictionary_[widget][modifier + 1];
        else if (isToggled && actionContextDictionary_[widget].count(modifier + 2) > 0)
            return actionContextDictionary_[widget][modifier + 2];
        else if (actionContextDictionary_[widget].count(modifier) > 0)
            return actionContextDictionary_[widget][modifier];
    }

    return emptyContexts_;
}
