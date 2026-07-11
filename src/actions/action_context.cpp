#include "../controls/integrator.h"

////////// ActionContext ////////

ActionContext::ActionContext(CSurfIntegrator* const csi, Action* action, Widget* widget, Zone* zone, int paramIndex, const vector<string>& paramsAndProperties)
    : csi_(csi), action_(action), widget_(widget), zone_(zone), paramIndex_(paramIndex) {
    vector<string> params;
    for (int i = 0; i < (int) (paramsAndProperties).size(); ++i) {
        if ((paramsAndProperties)[i].find("=") == string::npos)
            params.push_back((paramsAndProperties)[i]);
        sourceParams_.push_back((paramsAndProperties)[i]);
    }
    GetPropertiesFromTokens(0, (int) (paramsAndProperties).size(), paramsAndProperties, widgetProperties_);

    const char* feedback = widgetProperties_.get_prop(PropertyType_Feedback);
    if (feedback && IsSameString(feedback, "No"))
        provideFeedback_ = false;

    const char* holdDelay = widgetProperties_.get_prop(PropertyType_HoldDelay);
    if (holdDelay)
        timing_.holdDelayMs = atoi(holdDelay);

    const char* holdRepeatInterval = widgetProperties_.get_prop(PropertyType_HoldRepeatInterval);
    if (holdRepeatInterval)
        timing_.holdRepeatIntervalMs = atoi(holdRepeatInterval);

    const char* runCount = widgetProperties_.get_prop(PropertyType_RunCount);
    if (runCount) runCount_ = atoi(runCount);
    if (runCount_ < 1) {
        runCount_ = 1;
        this->LogMessage(string("invalid value for RunCount '") + runCount + "'", DEBUG_LEVEL_WARNING);
    }

    const char* blinkTime = widgetProperties_.get_prop(PropertyType_Blink);
    if (blinkTime)
        SetBlinkInterval(atoi(blinkTime));

    const char* meterMode = widgetProperties_.get_prop(PropertyType_MeterMode);
    if (meterMode && strlen(meterMode) > 0)
        strncpy(meterMode_, meterMode, sizeof(meterMode_) - 1);

    for (int i = 0; i < (int) (paramsAndProperties).size(); ++i) {
        if (paramsAndProperties[i] == "NoFeedback") {
            provideFeedback_ = false;
        } else if (paramsAndProperties[i] == "Blink") {
            SetBlinkInterval(INHERIT_VALUE);
        }
    }

    string actionName = "";

    if (params.size() > 0)
        actionName = params[0];

    // Action with int param, could include leading minus sign
    if (params.size() > 1 && (isdigit(params[1][0]) || params[1][0] == '-')) {
        intParam_ = atol(params[1].c_str());
    }

    if (actionName == "Bank" && (params.size() > 2 && (isdigit(params[2][0]) || params[2][0] == '-'))) {
        stringParam_ = params[1];
        intParam_ = atol(params[2].c_str());
    }

    // Action with param index, must be positive
    if (params.size() > 1 && isdigit(params[1][0])) {
        paramIndex_ = atol(params[1].c_str());
    }

    // Action with string param
    if (params.size() > 1)
        stringParam_ = params[1];

    if (actionName == "MoveEditCursor") {
        if (IsSameString(stringParam_, "Bar"))
            transportStepAmount_ = TransportStepAmount::Bar;
        else if (IsSameString(stringParam_, "Marker"))
            transportStepAmount_ = TransportStepAmount::Marker;
        else
            transportStepAmount_ = TransportStepAmount::Unknown;
    }

    if (actionName == "TrackVolumeDB" || actionName == "TrackSendVolumeDB" || actionName == "TrackReceiveVolumeDB") {
        value_.rangeMinimum = -144.0;
        value_.rangeMaximum = 24.0;
    }

    if (actionName == "TrackPanPercent" || actionName == "TrackPanWidthPercent" || actionName == "TrackPanLPercent" || actionName == "TrackPanRPercent") {
        value_.rangeMinimum = -100.0;
        value_.rangeMaximum = 100.0;
    }

    if ((actionName == "Reaper" || actionName == "ReaperDec" || actionName == "ReaperInc") && params.size() > 1) {
        if (isdigit(params[1][0])) {
            commandId_ = atol(params[1].c_str());
        } else {
            commandId_ = NamedCommandLookup(params[1].c_str());
            if (commandId_ == 0) {
                commandId_ = 65535;
                this->LogMessage(string("no actions found for '") + this->GetStringParam() + "'", DEBUG_LEVEL_ERROR);
            }
        }

        commandText_ = DAW::GetCommandName(commandId_);

        for (int id : GetCSI()->GetReloadingCommandIds()) {
            if (id == commandId_) {
                needsReloadAfterRun_ = true;
                break;
            }
        }

        int feedbackState = GetToggleCommandState(commandId_);
        if (feedbackState == -1 && provideFeedback_) {
            provideFeedback_ = false;
            this->LogMessage(string("action '") + DAW::GetCommandName(commandId_) + "' does not provide feedback", DEBUG_LEVEL_DEBUG);
        }
    }

    if ((actionName == "FXParam" || actionName == "JSFXParam") && params.size() > 1 && isdigit(params[1][0])) {
        paramIndex_ = atol(params[1].c_str());
    }

    if (actionName == "FXParamValueDisplay" && params.size() > 1 && isdigit(params[1][0])) {
        paramIndex_ = atol(params[1].c_str());
    }

    if (actionName == "FXParamNameDisplay" && params.size() > 1 && isdigit(params[1][0])) {
        paramIndex_ = atol(params[1].c_str());

        if (params.size() > 2 && params[2] != "{" && params[2] != "[")
            fxParamDisplayName_ = params[2];
    }

    if (actionName == "FixedTextDisplay" && (params.size() > 2 && (isdigit(params[2][0])))) {
        stringParam_ = params[1];
        paramIndex_ = atol(params[2].c_str());
    }

    if (params.size() > 0)
        color_.ParseColors(params);

    GetSteppedValues(widget, action_, zone_, paramIndex_, params, widgetProperties_, value_.deltaValue, value_.acceleratedDeltaValues, value_.rangeMinimum, value_.rangeMaximum, value_.steppedValues, value_.acceleratedTickValues);

    if (value_.acceleratedTickValues.size() < 1)
        value_.acceleratedTickValues.push_back(0);

    ProcessActionTitle(actionName);

    const char* osdValue = widgetProperties_.get_prop(PropertyType_OSD);
    osdData_ = osd_data(osdValue ? osdValue : "?");
    if (osdData_.message == "No")
        osdData_.message.clear();
    else if (osdData_.message == "?")
        osdData_.message = actionTitle_;

    if (actionName == "InvalidAction")
        osdData_.bgColor = osd_data::COLOR_ERROR;
}


void ActionContext::ProcessActionTitle(string origName) {
    if (commandId_ > 0) {
        actionTitle_ = DAW::GetCommandName(commandId_);
        return;
    }
    const ActionType actionType = this->GetAction()->GetType();
    const char* actionName = this->GetAction()->GetName();
    const char* stringParam = this->GetStringParam();

    switch (actionType) {
        case ActionType::InvalidAction:
            actionTitle_ = "InvalidAction: " + origName;
            break;
        case ActionType::TrackAutoMode:
            actionTitle_ = string("Automation: ") + TrackNavigationManager::GetAutoModeDisplayNameNoOverride(atoi(stringParam));
            break;
        default:
            if (IsSameString(stringParam, "{") || IsSameString(stringParam, "[")) actionTitle_ = actionName; //TODO: fix parser?
            else actionTitle_ = (stringParam && stringParam[0] != '\0') ? string(actionName) + " " + stringParam : actionName;
            break;
    }
}

IPageContext* ActionContext::GetPage() { return widget_->GetSurface()->GetPage(); }
TrackNavigationManager* ActionContext::GetTrackNavigationManager() { return GetPage()->GetTrackNavigationManager(); }
ControlSurface* ActionContext::GetSurface() { return widget_->GetSurface(); }
MediaTrack* ActionContext::GetTrack() { return zone_->GetNavigator()->GetTrack(); }
vector<MediaTrack*> ActionContext::GetSelectedTracks(bool includeMaster) {
    if (this->GetZone()->GetNavigator()->GetType() == NavigatorType::SelectedTrackNavigator) {
        return this->GetPage()->GetTrackNavigationManager()->GetSelectedTracks(includeMaster);
    } else {
        MediaTrack* track = this->GetTrack();
        return track ? vector<MediaTrack*> { track } : vector<MediaTrack*> {};
    }
}

int ActionContext::GetSlotIndex() { return zone_->GetSlotIndex(); }

const char* ActionContext::GetName() { return zone_->GetAlias(); }
void ActionContext::RequestUpdate() {
    if (provideFeedback_ && !timing_.isDoublePress) 
        action_->RequestUpdate(this);
}

void ActionContext::ClearWidget() {
    UpdateWidgetValue(0.0);
    UpdateWidgetValue("");
}

void ActionContext::UpdateColorValue(double value) {
    if (color_.supportsColor) {
        color_.currentColorIndex = value == 0 ? 0 : 1;
        if (color_.colorValues.size() > (size_t) color_.currentColorIndex)
            widget_->UpdateColorValue(color_.colorValues[color_.currentColorIndex]);
    }
}

void ActionContext::UpdateWidgetValue(double value) {
    if (value_.steppedValues.size() > 0)
        SetSteppedValueIndex(value);

    if (value_.isFeedbackInverted) {
        value = 1.0 - value;
    } else if (blink_.blinkSet) {
        bool shouldBlink = (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE);
        if (shouldBlink && !GetSurface()->IsBlinkLit(GetBlinkInterval())) {
            value = 1.0 - value;
        }
    }
    widget_->UpdateValue(widgetProperties_, value);

    UpdateColorValue(value);

    if (osdData_.IsAwaitFeedback())
        ProcessOSD(value, true);

    if (color_.supportsTrackColor)
        UpdateTrackColor();
}

void ActionContext::UpdateJSFXWidgetSteppedValue(double value) {
    if (value_.steppedValues.size() > 0)
        SetSteppedValueIndex(value);
}

void ActionContext::UpdateTrackColor() {
    if (MediaTrack* track = zone_->GetNavigator()->GetTrack()) {
        rgba_color color = DAW::GetTrackColor(track);
        widget_->UpdateColorValue(color);
    }
}

void ActionContext::UpdateWidgetValue(const char* value) {
    widget_->UpdateValue(widgetProperties_, value ? value : "");
}

void ActionContext::ForceWidgetValue(const char* value) {
    widget_->ForceValue(widgetProperties_, value ? value : "");
}

void ActionContext::LogAction(double value) {
    if (g_debugLevel < DEBUG_LEVEL_INFO) return;
    if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
    if (value < 0 && GetRangeMinimum() >= 0) return;
    if (value > 0 && GetRangeMinimum() < 0) return;

    std::ostringstream oss;
    if (color_.supportsColor) {
        oss << " { ";
        for (size_t i = 0; i < color_.colorValues.size(); ++i) {
            oss << " " << color_.colorValues[i].r << " " << color_.colorValues[i].g << " " << color_.colorValues[i].b;
            if (i != color_.colorValues.size() - 1) oss << ", ";
        }
        oss << " }[" << color_.currentColorIndex << "]";
    }
    if (!provideFeedback_) oss << " FeedBack=No";
    if (value_.isValueInverted) oss << " Invert";
    if (value_.isFeedbackInverted) oss << " InvertFB";
    if (timing_.holdDelayMs > 0) oss << " HoldDelay=" << timing_.holdDelayMs;
    if (timing_.holdRepeatIntervalMs > 0) oss << " HoldRepeatInterval=" << timing_.holdRepeatIntervalMs;
    if (runCount_ > 1) oss << " RunCount=" << runCount_;
    if (blink_.blinkSet) {
        oss << " Blink";
        if (blink_.blinkIntervalMs > 0) oss << "=" << blink_.blinkIntervalMs;
    }
    LogToConsole("[INFO] @%s/{%s}: [%s] '%s' > %s (%s) val:%0.2f ctx:%s\n", this->GetSurface()->GetName(), this->GetZone()->GetName(), this->GetWidget()->GetName(), JoinStringVector(sourceParams_, " ").c_str(), actionTitle_.c_str(), oss.str().c_str(), value, this->GetName());
}

void ActionContext::LogMessage(const std::string& msg, DebugLevel debugLevel) {
    if (g_debugLevel >= debugLevel) LogToConsole("[%s] @%s/{%s}: [%s] %s(%s) # %s\n", DebugLevelToString(debugLevel), this->GetSurface()->GetName(), this->GetZone()->GetName(), this->GetWidget()->GetName(), this->GetAction()->GetName(), this->GetStringParam(), msg.c_str());
}

int ActionContext::ClampValueWithWarning(int value, int min_val, int max_val) {
    int clamped = (std::max)(min_val, (std::min)(value, max_val));
    if (clamped != value) this->LogMessage("invalid value = " + to_string(value) + " (allowed: " + to_string(min_val) + "–" + to_string(max_val) + ")", DEBUG_LEVEL_WARNING);
    return clamped;
}

int ActionContext::GetBlinkInterval() { return blink_.blinkIntervalMs == INHERIT_VALUE ? this->GetSurface()->GetBlinkTime() : blink_.blinkIntervalMs; }
int ActionContext::GetHoldDelay() { return timing_.holdDelayMs == INHERIT_VALUE ? this->GetSurface()->GetHoldTime() : timing_.holdDelayMs; }

// runs once button pressed/released
void ActionContext::DoAction(double value) {
    int holdDelayMs = this->GetHoldDelay();

    // --- Hold/repeat timing cleanup on release (must happen before IgnoresRelease) ---
    if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
        timing_.holdActive = false;
        timing_.holdRepeatActive = false;
    }

    // --- Normal action deferred by hold buddy ---
    // When a Hold+ context exists on the same widget, the normal context
    // (holdDelay=0) must defer: fire on release only if the hold didn't fire.
    if (!timing_.isDoublePress && holdDelayMs == 0 && GetWidget()->HasHoldActions()) {
        if (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
            GetWidget()->ClearHoldFired();
            timing_.deferredValue = value;
            return;
        } else {
            if (!GetWidget()->GetHoldFired()) {
                PerformAction(timing_.deferredValue);
            }
            return;
        }
    }

    if (action_->IgnoresRelease() && value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) 
        return;

    DWORD nowTs = GetTickCount();
    timing_.deferredValue = value;

    // --- DoublePress detection ---
    if ((timing_.isDoublePress || GetWidget()->HasDoublePressActions()) && value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
        if (timing_.doublePressStartTs == 0 || nowTs > timing_.doublePressStartTs + GetSurface()->GetDoublePressTime()) {
            timing_.doublePressStartTs = nowTs;
            if (timing_.isDoublePress) return; // throttle normal press
        } else {
            timing_.doublePressStartTs = 0;
            if (!timing_.isDoublePress && holdDelayMs == 0) return; // block normal press inside double-press window
        }
    }

    // --- Hold repeat setup ---
    if (timing_.holdRepeatIntervalMs > 0) {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
            // already cleaned up above
        } else {
            if (holdDelayMs == 0) {
                timing_.holdRepeatActive = true;
                timing_.lastHoldRepeatTs = nowTs;
            }
        }
    }

    // --- Hold delay: defer action until held long enough ---
    if (holdDelayMs > 0) {
        if (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
            timing_.holdActive = true;
            timing_.lastHoldStartTs = nowTs;
        }
        // Release already handled above
    } else {
        PerformAction(value);
    }
}

// runs in loop to support button hold/repeat actions
void ActionContext::RunDeferredActions() {
    int holdDelayMs = GetHoldDelay();

    if (holdDelayMs > 0
        && timing_.holdActive
        && timing_.lastHoldStartTs > 0
        && GetTickCount() > (timing_.lastHoldStartTs + holdDelayMs)
    ) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] HOLD [%s] %d ms\n", GetWidget()->GetName(), GetTickCount() - timing_.lastHoldStartTs);
        GetWidget()->SetHoldFired(); // Signal to normal context: hold action fired, suppress normal on release
        PerformAction(timing_.deferredValue);
        timing_.holdActive = false; // to mark that this action with it's defined hold delay was performed and separate it from repeated action trigger
        if (timing_.holdRepeatIntervalMs > 0) {
            timing_.holdRepeatActive = true;
            timing_.lastHoldRepeatTs = GetTickCount();
        }
    }
    if (timing_.holdRepeatIntervalMs > 0
        && timing_.holdRepeatActive
        && timing_.lastHoldRepeatTs > 0
        && GetTickCount() > (timing_.lastHoldRepeatTs + timing_.holdRepeatIntervalMs)
    ) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] REPEAT [%s] %d ms\n", GetWidget()->GetName(), GetTickCount() - timing_.lastHoldRepeatTs);
        timing_.lastHoldRepeatTs = GetTickCount();
        PerformAction(timing_.deferredValue);
    }
}

void ActionContext::PerformAction(double value) {
    if (!value_.steppedValues.empty()) {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE)
            return;
        if (value_.steppedValuesIndex == (int) value_.steppedValues.size() - 1) {
            if (value_.steppedValues[0] < value_.steppedValues[value_.steppedValuesIndex]) // GAW -- only wrap if 1st value is lower
                value_.steppedValuesIndex = 0;
        } else
            value_.steppedValuesIndex++;

        for (int i = 0; i < runCount_; ++i)
            DoRangeBoundAction(value_.steppedValues[value_.steppedValuesIndex]);
    } else
        for (int i = 0; i < runCount_; ++i)
            DoRangeBoundAction(value);
}

void ActionContext::DoRelativeAction(double delta) {
    if (value_.steppedValues.size() > 0)
        DoSteppedValueAction(delta);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + (value_.deltaValue != 0.0 ? (delta > 0 ? value_.deltaValue : -value_.deltaValue) : delta));
}

void ActionContext::DoRelativeAction(int accelerationIndex, double delta) {
    if (value_.steppedValues.size() > 0)
        DoAcceleratedSteppedValueAction(accelerationIndex, delta);
    else if (value_.acceleratedDeltaValues.size() > 0)
        DoAcceleratedDeltaValueAction(accelerationIndex, delta);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + (value_.deltaValue != 0.0 ? (delta > 0 ? value_.deltaValue : -value_.deltaValue) : delta));
}

void ActionContext::ProcessOSD(double value, bool fromFeedback) {
    if (!GetSurface()->IsOsdEnabled()) return;
    if (GetWidget()->IsVirtual()) return;
    if (osdData_.message.empty()) return;
    if (!fromFeedback && OsdIgnoresButtonRelease() && value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
    if (value < 0 && GetRangeMinimum() >= 0) return;
    if (value > 0 && GetRangeMinimum() < 0) return;

    int colorIdx = (int) value;
    if (osdData_.bgColors.empty()) {
        if (color_.supportsColor && !color_.colorValues.empty()) {
            if (color_.colorValues.size() == 1)
                colorIdx = 0;
            if ((int) color_.colorValues.size() - 1 < colorIdx)
                colorIdx = (int) color_.colorValues.size() - 1;

            char hexColor[8];
            snprintf(hexColor, sizeof(hexColor), "#%02X%02X%02X", color_.colorValues[colorIdx].r, color_.colorValues[colorIdx].g, color_.colorValues[colorIdx].b);

            osdData_.bgColor = hexColor;
        } else if (GetWidget()->GetIsTwoState()) {
            osdData_.bgColor = (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) ? "1" : "0";
        }
    } else {
        if (osdData_.bgColors.size() == 1)
            colorIdx = 0;
        if ((int) osdData_.bgColors.size() - 1 < colorIdx)
            colorIdx = (int) osdData_.bgColors.size() - 1;
        osdData_.bgColor = osdData_.bgColors[colorIdx];
    }
    if (osdData_.timeoutMs == 0)
        osdData_.timeoutMs = GetSurface()->GetOSDTime();

    if ((action_->IsVolumeRelated() || action_->IsPanRelated()) && !(action_->IsDisplayRelated() || action_->IsMeterRelated())) {
        if (MediaTrack* track = this->GetTrack()) {
            ostringstream oss;
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            oss << "[" << trackName_ << "] ";
            if (action_->IsPanRelated()) {
                if (pan == 0.0) oss << "Center";
                else oss << std::fixed << std::setprecision(2) << std::abs(pan * 100) << "%" << (pan > 0 ? "R" : "L");
            } else
                oss << std::fixed << std::setprecision(2) << VAL2DB(vol) << " dB";

            osdData_.message = oss.str();
        } else {
            osdData_.message = string(action_->GetName()) + ": No track selected";
        }
        osdData_.SetAwaitFeedback(false);
        return GetCSI()->EnqueueOSD(osdData_);
    }

    if (action_->IsFxRelated() && !(action_->IsDisplayRelated() || action_->IsMeterRelated())) {
        osdData_.message = (fxParamDisplayName_.empty() ? fxParamDescription_ : fxParamDisplayName_);
        osdData_.message += (osdData_.message.empty()) ? "No FX selected" : ": " + GetTrackFxParamFormattedValue();
        osdData_.SetAwaitFeedback(false);
        return GetCSI()->EnqueueOSD(osdData_);
    }

    const ActionType actionType = action_->GetType();

    if (actionType == ActionType::CycleDebugLevel) {
        osdData_.message = string(action_->GetName()) + ": " + DebugLevelToString(g_debugLevel);
        osdData_.SetAwaitFeedback(false);
        return GetCSI()->EnqueueOSD(osdData_);
    }

    bool awaitFeedback = osdData_.IsAwaitFeedback();
    if (provideFeedback_) {
        if (awaitFeedback) {
            if (fromFeedback) {
                osdData_.SetAwaitFeedback(false);
            } else {
                return;
            }
        } else {
            return osdData_.SetAwaitFeedback(!fromFeedback);
        }
    } else {
        osdData_.SetAwaitFeedback(false);
    }

    GetCSI()->EnqueueOSD(osdData_);
}

bool ActionContext::OsdIgnoresButtonRelease() {
    const ActionType actionType = this->GetAction()->GetType();
    if (actionType == ActionType::Bank
        || actionType == ActionType::SetShift
        || actionType == ActionType::SetOption
        || actionType == ActionType::SetControl
        || actionType == ActionType::SetAlt
        || actionType == ActionType::SetFlip
        || actionType == ActionType::SetGlobal
        || actionType == ActionType::SetMarker
        || actionType == ActionType::SetNudge
        || actionType == ActionType::SetZoom
        || actionType == ActionType::SetScrub
        || actionType == ActionType::FXParam
        || actionType == ActionType::JSFXParam
        || actionType == ActionType::TCPFXParam
        || actionType == ActionType::LastTouchedFXParam
        || actionType == ActionType::TrackVolume
        || actionType == ActionType::SoftTakeover7BitTrackVolume
        || actionType == ActionType::SoftTakeover14BitTrackVolume
        || actionType == ActionType::TrackVolumeDB
        || actionType == ActionType::TrackPan
        || actionType == ActionType::TrackPanPercent
        || actionType == ActionType::TrackPanWidth
        || actionType == ActionType::TrackPanWidthPercent
        || actionType == ActionType::TrackPanL
        || actionType == ActionType::TrackPanLPercent
        || actionType == ActionType::TrackPanR
        || actionType == ActionType::TrackPanRPercent
        || actionType == ActionType::TrackPanAutoLeft
        || actionType == ActionType::TrackPanAutoRight
        || actionType == ActionType::TrackSendVolume
        || actionType == ActionType::TrackSendVolumeDB
        || actionType == ActionType::TrackSendPan
        || actionType == ActionType::TrackSendPanPercent
        || actionType == ActionType::TrackReceiveVolume
        || actionType == ActionType::TrackReceiveVolumeDB
        || actionType == ActionType::TrackReceivePan
        || actionType == ActionType::TrackReceivePanPercent
        || actionType == ActionType::MoveCursor
        || actionType == ActionType::TrackVolumeWithMeterAverageLR
        || actionType == ActionType::TrackVolumeWithMeterMaxPeakLR) {
        return false;
    }
    return true;
}

void ActionContext::DoRangeBoundAction(double value) {
    this->LogAction(value);

    if (value > value_.rangeMaximum)
        value = value_.rangeMaximum;

    if (value < value_.rangeMinimum)
        value = value_.rangeMinimum;

    if (value_.isValueInverted)
        value = 1.0 - value;

    for (int i = 0; i < runCount_; ++i)
        action_->Do(this, value);

    this->ProcessOSD(value, false);
}

void ActionContext::DoSteppedValueAction(double delta) {
    if (delta > 0) {
        value_.steppedValuesIndex++;

        if (value_.steppedValuesIndex > (int) value_.steppedValues.size() - 1)
            value_.steppedValuesIndex = (int) value_.steppedValues.size() - 1;

        DoRangeBoundAction(value_.steppedValues[value_.steppedValuesIndex]);
    } else {
        value_.steppedValuesIndex--;

        if (value_.steppedValuesIndex < 0)
            value_.steppedValuesIndex = 0;

        DoRangeBoundAction(value_.steppedValues[value_.steppedValuesIndex]);
    }
}

void ActionContext::DoAcceleratedSteppedValueAction(int accelerationIndex, double delta) {
    if (delta > 0) {
        value_.accumulatedIncTicks++;
        value_.accumulatedDecTicks = value_.accumulatedDecTicks - 1 < 0 ? 0 : value_.accumulatedDecTicks - 1;
    } else if (delta < 0) {
        value_.accumulatedDecTicks++;
        value_.accumulatedIncTicks = value_.accumulatedIncTicks - 1 < 0 ? 0 : value_.accumulatedIncTicks - 1;
    }

    accelerationIndex = accelerationIndex > (int) value_.acceleratedTickValues.size() - 1 ? (int) value_.acceleratedTickValues.size() - 1 : accelerationIndex;
    accelerationIndex = accelerationIndex < 0 ? 0 : accelerationIndex;

    if (delta > 0 && value_.accumulatedIncTicks >= value_.acceleratedTickValues[accelerationIndex]) {
        value_.accumulatedIncTicks = 0;
        value_.accumulatedDecTicks = 0;

        value_.steppedValuesIndex++;

        if (value_.steppedValuesIndex > (int) value_.steppedValues.size() - 1)
            value_.steppedValuesIndex = (int) value_.steppedValues.size() - 1;

        DoRangeBoundAction(value_.steppedValues[value_.steppedValuesIndex]);
    } else if (delta < 0 && value_.accumulatedDecTicks >= value_.acceleratedTickValues[accelerationIndex]) {
        value_.accumulatedIncTicks = 0;
        value_.accumulatedDecTicks = 0;

        value_.steppedValuesIndex--;

        if (value_.steppedValuesIndex < 0)
            value_.steppedValuesIndex = 0;

        DoRangeBoundAction(value_.steppedValues[value_.steppedValuesIndex]);
    }
}

void ActionContext::DoAcceleratedDeltaValueAction(int accelerationIndex, double delta) {
    accelerationIndex = accelerationIndex > (int) value_.acceleratedDeltaValues.size() - 1 ? (int) value_.acceleratedDeltaValues.size() - 1 : accelerationIndex;
    accelerationIndex = accelerationIndex < 0 ? 0 : accelerationIndex;

    if (delta > 0.0)
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) + value_.acceleratedDeltaValues[accelerationIndex]);
    else
        DoRangeBoundAction(action_->GetCurrentNormalizedValue(this) - value_.acceleratedDeltaValues[accelerationIndex]);
}

// GetColorValues and SetColor have been moved to ActionColorState::ParseColors() in action_color.h (Phase 6).

void ActionContext::GetSteppedValues(Widget* widget, Action* action, Zone* zone, int paramNumber, const vector<string>& params, const PropertyList& widgetProperties, double& deltaValue, vector<double>& acceleratedDeltaValues, double& rangeMinimum, double& rangeMaximum, vector<double>& steppedValues, vector<int>& acceleratedTickValues) {
    ::GetSteppedValues(params, 0, deltaValue, acceleratedDeltaValues, rangeMinimum, rangeMaximum, steppedValues, acceleratedTickValues);

    if (deltaValue == 0.0 && widget->GetStepSize() != 0.0)
        deltaValue = widget->GetStepSize();

    if (acceleratedDeltaValues.size() == 0 && widget->GetAccelerationValues().size() != 0)
        acceleratedDeltaValues = widget->GetAccelerationValues();

    if (steppedValues.size() > 0 && acceleratedTickValues.size() == 0) {
        double stepSize = deltaValue;

        if (stepSize != 0.0) {
            stepSize *= 10000.0;

            int stepCount = (int) steppedValues.size();
            int baseTickCount = (NUM_ELEM(s_tickCounts_) > stepCount)
                ? s_tickCounts_[stepCount]
                : s_tickCounts_[NUM_ELEM(s_tickCounts_) - 1];
            int tickCount = int(baseTickCount / stepSize + 0.5);
            acceleratedTickValues.push_back(tickCount);
        }
    }
}
