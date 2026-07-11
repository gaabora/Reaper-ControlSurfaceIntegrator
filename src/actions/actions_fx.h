// FX-related actions
#pragma once

class FXParam : public FXAction
{
public:
    ActionType GetType() const override { return ActionType::FXParam; }
};

class JSFXParam : public FXAction
{
public:
    ActionType GetType() const override { return ActionType::JSFXParam; }
};

class TCPFXParam : public FXAction
{
public:
    ActionType GetType() const override { return ActionType::TCPFXParam; }

    virtual bool CheckCurrentContext(ActionContext* context) {
        return context->CheckCurrentTcpFxContext();
    }
};

class LastTouchedFXParam : public FXAction
{
public:
    ActionType GetType() const override { return ActionType::LastTouchedFXParam; }

    bool CheckCurrentContext(ActionContext* context) {
        return context->CheckLastTouchedFxContext();
    }
};

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
