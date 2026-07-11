// FX-related actions
#pragma once

//! @action FXParam
//!
//! @brief Controls an FX parameter by slot and param index. The primary FX parameter action.
//!
//! @zone_usage  Rotary|    FXParam 0   (in FX zone files, param index from zone layout)
//!
//! @feedback Continuous — sends FX param value (0.0–1.0). Skips if value unchanged.
//!
//! @notes Touch for automation. CheckCurrentContext validates track/FX/param exist.
//!
//! @see JSFXParam, TCPFXParam, LastTouchedFXParam
class FXParam : public FXAction
{
public:
    ActionType GetType() const override { return ActionType::FXParam; }
};

//! @action JSFXParam
//!
//! @brief Controls a JSFX plugin parameter. Same as FXParam but uses JSFX-specific context.
//!
//! @zone_usage  Rotary|    JSFXParam 0
//!
//! @feedback Continuous — sends FX param value (0.0–1.0).
class JSFXParam : public FXAction
{
public:
    ActionType GetType() const override { return ActionType::JSFXParam; }
};

//! @action TCPFXParam
//!
//! @brief Controls an FX parameter exposed on the Track Control Panel (TCP). Uses TCP-specific context validation.
//!
//! @zone_usage  Rotary|    TCPFXParam 0
//!
//! @feedback Continuous — sends FX param value (0.0–1.0).
//!
//! @notes Uses CheckCurrentTcpFxContext instead of the standard FX context check.
class TCPFXParam : public FXAction
{
public:
    ActionType GetType() const override { return ActionType::TCPFXParam; }

    virtual bool CheckCurrentContext(ActionContext* context) {
        return context->CheckCurrentTcpFxContext();
    }
};

//! @action LastTouchedFXParam
//!
//! @brief Controls the last-touched FX parameter in Reaper. Automatically maps to whichever FX param was last touched.
//!
//! @zone_usage  Rotary|    LastTouchedFXParam
//!
//! @feedback Continuous — sends current value of the last-touched parameter.
//!
//! @see ClearLastTouchedFXParam, LastTouchedFXParamNameDisplay, LastTouchedFXParamValueDisplay
class LastTouchedFXParam : public FXAction
{
public:
    ActionType GetType() const override { return ActionType::LastTouchedFXParam; }

    bool CheckCurrentContext(ActionContext* context) {
        return context->CheckLastTouchedFxContext();
    }
};

//! @action ToggleFXBypass
//!
//! @brief Toggles bypass on/off for an FX plugin in a specific slot.
//!
//! @zone_usage  WidgetName    ToggleFXBypass
//!
//! @feedback Toggle — 1.0 when FX is enabled, 0.0 when bypassed. Also 0.0 if the entire track FX chain is bypassed.
//!
//! @see FXBypassDisplay
class ToggleFXBypass : public PressOnlyFXAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleFXBypass; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (DAW::GetTrackBypass(track))
                context->UpdateWidgetValue(0.0);
            else if (TrackFX_GetCount(track) > context->GetSlotIndex()) {
                if (TrackFX_GetEnabled(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue(1.0);
                else
                    context->UpdateWidgetValue(0.0);
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            TrackFX_SetEnabled(track, context->GetSlotIndex(), !TrackFX_GetEnabled(track, context->GetSlotIndex()));
    }
};

//! @action FXBypassDisplay
//!
//! @brief Displays the bypass state of an FX slot as text.
//!
//! @zone_usage  DisplayWidget    FXBypassDisplay
//!
//! @feedback Text — "Enabled", "Bypassed", or clears if no FX in slot.
class FXBypassDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::FXBypassDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (DAW::GetTrackBypass(track))
                context->UpdateWidgetValue("Bypassed");
            else if (TrackFX_GetCount(track) > context->GetSlotIndex()) {
                if (TrackFX_GetEnabled(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue("Enabled");
                else
                    context->UpdateWidgetValue("Bypassed");
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

//! @action ToggleFXOffline
//!
//! @brief Toggles offline state for an FX plugin in a specific slot (unloads the plugin from memory).
//!
//! @zone_usage  WidgetName    ToggleFXOffline
//!
//! @feedback Toggle — 1.0 when FX is online, 0.0 when offline.
//!
//! @see FXOfflineDisplay
class ToggleFXOffline : public PressOnlyFXAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleFXOffline; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (TrackFX_GetCount(track) > context->GetSlotIndex()) {
                if (TrackFX_GetOffline(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue(0.0);
                else
                    context->UpdateWidgetValue(1.0);
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            TrackFX_SetOffline(track, context->GetSlotIndex(), !TrackFX_GetOffline(track, context->GetSlotIndex()));
    }
};

//! @action FXOfflineDisplay
//!
//! @brief Displays the online/offline state of an FX slot as text.
//!
//! @zone_usage  DisplayWidget    FXOfflineDisplay
//!
//! @feedback Text — "Online" or "Offline".
class FXOfflineDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::FXOfflineDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (TrackFX_GetCount(track) > context->GetSlotIndex()) {
                if (TrackFX_GetOffline(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue("Offline");
                else
                    context->UpdateWidgetValue("Online");
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};
