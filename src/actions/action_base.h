//
//  control_surface_base_actions.h
//  reaper_csurf_integrator
//
//
#include <functional>

#ifndef control_surface_action_contexts_h
#define control_surface_action_contexts_h

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class NoAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::NoAction; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
        context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class InvalidAction : public NoAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::InvalidAction; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ReaperAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::ReaperAction; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        int state = GetToggleCommandState(context->GetCommandId());
        
        if (state == -1) // this Action does not report state
            state = 0;
        
        if ( ! (context->GetRangeMinimum() == -2.0 || context->GetRangeMaximum() == 2.0)) // used for Increase/Decrease
            context->UpdateWidgetValue(state);
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        int commandId = context->GetCommandId();
        bool needsReload = context->NeedsReloadAfterRun();
        string commandName = context->GetCommandText();

        // used for Increase/Decrease
        if (value < 0 && context->GetRangeMinimum() < 0)
            DAW::SendCommandMessage(commandId);
        else if (value > 0 && context->GetRangeMinimum() >= 0)
            DAW::SendCommandMessage(commandId);

        if (needsReload) {
            throw ReloadPluginException(commandName);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsFxRelated() { return true; }

    virtual bool CheckCurrentContext(ActionContext *context) {
        return context->CheckCurrentFxContext();
    }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override
    {
        if (!CheckCurrentContext(context))
            return 0.0;
        return context->GetTrackFxParamValue();
    }

    virtual void RequestUpdate(ActionContext* context) override
    {
        if (!CheckCurrentContext(context))
            return context->ClearWidget();
        double value = context->GetTrackFxParamValue();
        if (context->IsSameAsLastValue(value))
            return;
        context->UpdateWidgetValue(value);
        context->SetLastValue(value);
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (context->IsSameAsLastValue(value))
            return;
        if (!CheckCurrentContext(context))
            return;
        context->SetTrackFxParamValue(value);
        if (!context->GetProvideFeedback())
            context->SetLastValue(value);
    }

    virtual void Touch(ActionContext* context, double value) override
    {
        if (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE)
            return;
        if (!CheckCurrentContext(context))
            return;
        context->EndTrackFxParamEdit();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SettingsAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsSettingsRelated() { return true; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SwitchAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsSwitch() { return true; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ModifierAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsModifier() { return true; }
    void RequestUpdate(ActionContext *context) override {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TransportAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsTransportRelated() { return true; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DisplayAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsDisplayRelated() { return true; }
};
class TrackAction : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    virtual bool IncludeMasterTrack() const { return true; }
protected:
    double GetTrackBoolStateNormalized(ActionContext* context, function<bool(MediaTrack*)> getStateFn) const {
        const vector<MediaTrack*>& selectedTracks = context->GetSelectedTracks(IncludeMasterTrack());
        if (selectedTracks.empty())
            return 0.0;
        return getStateFn(selectedTracks[0]) ? 1.0 : 0.0;
    }
    bool ToggleTrackState(ActionContext* context, const function<bool(MediaTrack*)>& getStateFn, const function<void(MediaTrack*, bool)>& setStateFn) {
        const vector<MediaTrack*>& selectedTracks = context->GetSelectedTracks(IncludeMasterTrack());
        if (selectedTracks.empty())
            return false;
        bool oldState = getStateFn(selectedTracks[0]);
        for (MediaTrack* track : selectedTracks)
            setStateFn(track, !oldState);
        return true;
    }
public:
    virtual bool IsTrackRelated() { return true; }
    virtual double GetCurrentNormalizedValue(ActionContext* context) { return 0.0; }
    virtual void RequestUpdate(ActionContext *context) {
        if (context->GetSelectedTracks().empty())
            context->ClearWidget();
        else
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
};

class TrackSendAction : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsTrackSendRelated() { return true; }
};

class TrackReceiveAction : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsTrackReceiveRelated() { return true; }
};

class TrackDisplayAction : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsDisplayRelated() { return true; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackMeterAction : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsMeterRelated() { return true; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class VolumeAction : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsVolumeRelated() { return true; }
    
    virtual bool CheckCurrentContext(ActionContext *context) {
        return context->CheckCurrentTrackContext();
    }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override 
    {
        if (!CheckCurrentContext(context))
            return 0.0;
        return context->GetTrackVolumeValue();
    }

    virtual void RequestUpdate(ActionContext* context) override
    {
        if (!CheckCurrentContext(context))
            return context->ClearWidget();
        double value = context->GetTrackVolumeValue();
        context->UpdateWidgetValue(value);
        context->SetLastValue(value);
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (context->IsSameAsLastValue(value))
            return;
        if (!CheckCurrentContext(context))
            return;
        context->SetTrackVolumeValue(value);
        if (!context->GetProvideFeedback())
            context->SetLastValue(value);
    }

    virtual void Touch(ActionContext* context, double value) override
    {
        if (!CheckCurrentContext(context))
            return;
        double currentValue = GetCurrentNormalizedValue(context);
        this->Do(context, currentValue);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class PanAction : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual bool IsPanRelated() { return true; }
};

#endif /* control_surface_action_contexts_h */
