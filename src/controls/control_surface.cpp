#include "integrator.h"

void ControlSurface::ApplyInitialFeedbackValues() {
    const PropertyList properties;
    for (const std::pair<FeedbackProcessor*, double>& initialValue : this->initialFeedbackValues_) initialValue.first->ForceValue(properties, initialValue.second);
}

static float ClampPositiveScale(float value) {
    return value > 0.0f ? value : 1.0f;
}

static float ClampNonNegativeScale(float value) {
    return value >= 0.0f ? value : 0.0f;
}

static bool IsNearNeutralColor(const rgba_color& color, int tolerancePercent) {
    if (tolerancePercent <= 0)
        return false;

    const int maxChannel = (std::max)(color.r, (std::max)(color.g, color.b));
    const int minChannel = (std::min)(color.r, (std::min)(color.g, color.b));
    const int tolerance = (std::max)(2, (int) std::lround((float) maxChannel * ((float) tolerancePercent / 100.0f)));
    return maxChannel > 0 && (maxChannel - minChannel) <= tolerance;
}

static int MapColorChannelToDeviceLevel(int channel, int inputMax, int outputMax, float brightnessScale, float channelScale, float neutralScale, float neutralCurve, bool applyNeutralScale) {
    if (inputMax <= 0) inputMax = 255;
    if (outputMax < 0) outputMax = 0;

    float normalized = (float) wdl_clamp(channel, 0, inputMax) / (float) inputMax;
    float effectiveScale = ClampPositiveScale(channelScale);

    if (applyNeutralScale) {
        const float clampedNeutralScale = ClampPositiveScale(neutralScale);
        const float clampedNeutralCurve = ClampPositiveScale(neutralCurve);
        effectiveScale *= clampedNeutralScale + (1.0f - clampedNeutralScale) * std::pow(normalized, clampedNeutralCurve);
    }

    return wdl_clamp((int) std::lround(normalized * ClampNonNegativeScale(brightnessScale) * effectiveScale * (float) outputMax), 0, outputMax);
}

// ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
static string GetOskSurfaceEnabledSettingsKey(const string& surfaceName) {
    return string("SurfaceEnabled_") + surfaceName;
}

void ControlSurface::LoadOskEnabledSetting() {
    const string key = GetOskSurfaceEnabledSettingsKey(name_);
    if (!::HasExtState(ProductIdentity::ExtStateOskSettings, key.c_str())) return;
    const string value = ::GetExtState(ProductIdentity::ExtStateOskSettings, key.c_str());
    isOskEnabled_ = IsSameString(value.c_str(), "true") || value == "1";
}

void ControlSurface::SetOskEnabled(bool value) {
    isOskEnabled_ = value;
    const string key = GetOskSurfaceEnabledSettingsKey(name_);
    ::SetExtState(ProductIdentity::ExtStateOskSettings, key.c_str(), value ? "true" : "false", true);
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

void ControlSurface::SetTrackColorSourceText(Widget* source, const char* text) {
    for (auto trackColorFeedbackProcessor : this->trackColorFeedbackProcessors_)
        trackColorFeedbackProcessor->SetTrackColorSourceText(source, text);
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

rgba_color ControlSurface::GetDeviceFeedbackColor(const rgba_color& color, int defaultOutputMax, float brightnessScale) const {
    const int inputMax = this->colorCalibration_.enabled && this->colorCalibration_.inputMax > 0 ? this->colorCalibration_.inputMax : 255;
    const int outputMax = this->colorCalibration_.enabled && this->colorCalibration_.outputMax > 0 ? this->colorCalibration_.outputMax : defaultOutputMax;
    const bool applyNeutralScale = this->colorCalibration_.enabled && IsNearNeutralColor(color, this->colorCalibration_.neutralTolerancePercent);

    rgba_color deviceColor;
    deviceColor.r = MapColorChannelToDeviceLevel(color.r, inputMax, outputMax, brightnessScale, this->colorCalibration_.redScale, this->colorCalibration_.neutralRedScale, this->colorCalibration_.neutralCurve, applyNeutralScale);
    deviceColor.g = MapColorChannelToDeviceLevel(color.g, inputMax, outputMax, brightnessScale, this->colorCalibration_.greenScale, this->colorCalibration_.neutralGreenScale, this->colorCalibration_.neutralCurve, applyNeutralScale);
    deviceColor.b = MapColorChannelToDeviceLevel(color.b, inputMax, outputMax, brightnessScale, this->colorCalibration_.blueScale, this->colorCalibration_.neutralBlueScale, this->colorCalibration_.neutralCurve, applyNeutralScale);
    deviceColor.a = color.a;
    return deviceColor;
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
            this->csi_->ShowCurrentPositionOSD();
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
            this->csi_->ShowCurrentPositionOSD();
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

void ControlSurface::SetModifier(void (ModifierManager::*setter)(bool, int, ModifierMode), bool value, ActionModifierMode mode) {
    const ModifierMode resolvedMode = mode == ActionModifierMode::Momentary ? ModifierMode::Momentary : mode == ActionModifierMode::Latch ? ModifierMode::Latch : ModifierMode::Hybrid;
    if (zoneManager_->GetIsBroadcaster() && usesLocalModifiers_) {
        (modifierManager_.get()->*setter)(value, latchTime_, resolvedMode);

        for (auto& listener : zoneManager_->GetListeners())
            if (listener->GetSurface()->GetListensToModifiers() && !listener->GetSurface()->GetUsesLocalModifiers() & listener->GetSurface()->GetName() != name_)
                (listener->GetSurface()->GetModifierManager()->*setter)(value, latchTime_, resolvedMode);
    } else if (usesLocalModifiers_)
        (modifierManager_.get()->*setter)(value, latchTime_, resolvedMode);
    else
        (page_->GetModifierManager()->*setter)(value, latchTime_, resolvedMode);
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

void ControlSurface::SetShift(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetShift, value, mode); }
void ControlSurface::SetOption(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetOption, value, mode); }
void ControlSurface::SetControl(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetControl, value, mode); }
void ControlSurface::SetAlt(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetAlt, value, mode); }
void ControlSurface::SetFlip(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetFlip, value, mode); }
void ControlSurface::SetGlobal(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetGlobal, value, mode); }
void ControlSurface::SetMarker(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetMarker, value, mode); }
void ControlSurface::SetNudge(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetNudge, value, mode); }
void ControlSurface::SetZoom(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetZoom, value, mode); }
void ControlSurface::SetScrub(bool value, ActionModifierMode mode) { SetModifier(&ModifierManager::SetScrub, value, mode); }

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
    bool inColorCalibration = false;

    for (int i = 0; i < (int) lines.size(); ++i) {
        if (lines[i].size() > 0) {
            if (lines[i][0] == "StepSize") { inStepSizes = true; continue; }
            else if (lines[i][0] == "StepSizeEnd") { inStepSizes = false; continue; }
            else if (lines[i][0] == "AccelerationValues") { inAccelerationValues = true; continue; }
            else if (lines[i][0] == "AccelerationValuesEnd") { inAccelerationValues = false; continue; }
            else if (lines[i][0] == "ColorCalibration") {
                inColorCalibration = true;
                this->colorCalibration_ = ColorCalibrationConfig{};
                this->colorCalibration_.enabled = true;
                continue;
            } else if (lines[i][0] == "ColorCalibrationEnd") {
                inColorCalibration = false;
                continue;
            }

            if (inColorCalibration && lines[i].size() > 1) {
                const string& key = lines[i][0];
                const char* value = lines[i][1].c_str();

                if (key == "Enabled") this->colorCalibration_.enabled = !IsSameString(value, "No");
                else if (key == "InputMax") this->colorCalibration_.inputMax = atoi(value);
                else if (key == "OutputMax") this->colorCalibration_.outputMax = atoi(value);
                else if (key == "NeutralTolerancePercent") this->colorCalibration_.neutralTolerancePercent = atoi(value);
                else if (key == "RedScale") this->colorCalibration_.redScale = (float) atof(value);
                else if (key == "GreenScale") this->colorCalibration_.greenScale = (float) atof(value);
                else if (key == "BlueScale") this->colorCalibration_.blueScale = (float) atof(value);
                else if (key == "NeutralRedScale") this->colorCalibration_.neutralRedScale = (float) atof(value);
                else if (key == "NeutralGreenScale") this->colorCalibration_.neutralGreenScale = (float) atof(value);
                else if (key == "NeutralBlueScale") this->colorCalibration_.neutralBlueScale = (float) atof(value);
                else if (key == "NeutralCurve") this->colorCalibration_.neutralCurve = (float) atof(value);
                continue;
            }

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
