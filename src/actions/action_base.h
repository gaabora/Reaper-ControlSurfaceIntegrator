#include <functional>

#ifndef control_surface_action_contexts_h
    #define control_surface_action_contexts_h

//! @action NoAction
//!
//! @brief Clears the widget to zero/empty. Used when no action is mapped.
//!
//! @zone_usage  WidgetName    NoAction
//!
//! @feedback Clears widget value to 0.0 and text to "".
class NoAction : public Action
{
public:
    ActionType GetType() const override { return ActionType::NoAction; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
        context->ClearWidget();
    }
};

//! @action InvalidAction
//!
//! @brief Placeholder created when an unrecognized action name is used in a .zon file. Behaves like NoAction.
//!
//! @zone_usage  (generated automatically for unknown action names)
//!
//! @feedback Clears widget (inherited from NoAction).
class InvalidAction : public NoAction
{
public:
    ActionType GetType() const override { return ActionType::InvalidAction; }
};

//! @action Reaper
//!
//! @brief Executes a native Reaper action by command ID or named command string.
//!
//! @zone_usage  WidgetName    Reaper 40340   or   WidgetName    Reaper _SWS_SHOWMASTER
//!
//! @feedback Toggle — queries GetToggleCommandState(); 1.0 if on, 0.0 if off or stateless.
//!
//! @params First param: command ID (int) or named command string (e.g. _SWS_SHOWMASTER).
//!
//! @notes Supports Increase/Decrease pseudo-modifiers for directional actions. Can trigger ReloadPluginException for project-switching commands.
class ReaperAction : public Action
{
public:
    ActionType GetType() const override { return ActionType::ReaperAction; }

    virtual void RequestUpdate(ActionContext* context) override {
        int state = GetToggleCommandState(context->GetCommandId());

        if (state == -1) // this Action does not report state //TODO: review together with BUTTON_RELEASE_MESSAGE_VALUE case
            state = 0;

        if (!(context->GetRangeMinimum() == -2.0 || context->GetRangeMaximum() == 2.0)) // used for Increase/Decrease
            context->UpdateWidgetValue(state);
    }

    virtual void Do(ActionContext* context, double value) override {
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

//! @action (FX base)
//!
//! @brief Base class for FX parameter control actions. Reads/writes FX param values with context validation and touch support.
//!
//! @feedback Continuous — sends FX param value (0.0–1.0) to widget. Skips update if value unchanged.
//!
//! @notes Subclasses override CheckCurrentContext() to validate track/FX/param existence. Supports touch for automation write via EndTrackFxParamEdit.
class FXAction : public Action
{
public:
    virtual bool IsFxRelated() { return true; }

    virtual bool CheckCurrentContext(ActionContext* context) {
        return context->CheckCurrentFxContext();
    }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (!CheckCurrentContext(context)) return 0.0;
        return context->GetTrackFxParamValue();
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (!CheckCurrentContext(context)) return context->ClearWidget();
        double value = context->GetTrackFxParamValue();
        if (context->IsSameAsLastValue(value)) return;
        context->UpdateWidgetValue(value);
        context->SetLastValue(value);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (context->IsSameAsLastValue(value)) return;
        if (!CheckCurrentContext(context)) return;
        context->SetTrackFxParamValue(value);
        if (!context->GetProvideFeedback())
            context->SetLastValue(value);
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (!CheckCurrentContext(context)) return;
        context->EndTrackFxParamEdit();
    }
};

//! @action (settings base)
//!
//! @brief Base for configuration actions (SetBlinkTime, SetHoldTime, etc.). Ignores button release.
class SettingsAction : public Action
{
public:
    virtual bool IsSettingsRelated() { return true; }
    bool IgnoresRelease() const override { return true; }
};

//! @action (press-only base)
//!
//! @brief Base for actions that fire on press only, ignoring button release (value=0.0).
class PressOnlyAction : public Action
{
public:
    bool IgnoresRelease() const override { return true; }
};

//! @action (switch base)
//!
//! @brief Base for toggle/switch actions that respond to both press AND release.
class SwitchAction : public Action
{
public:
    virtual bool IsSwitch() { return true; }
    bool IgnoresRelease() const override { return false; }
};
//! @action (modifier base)
//!
//! @brief Base for modifier actions (Shift, Option, Control, etc.). Feedback reflects the modifier's engaged state.
//!
//! @feedback Toggle — sends 1.0 when modifier is engaged, 0.0 when disengaged.
//!
//! @notes Supports Blink property for visual indication of latched/held state. Both press and release are processed to handle latch timing.
class ModifierAction : public Action
{
public:
    virtual bool IsModifier() { return true; }
    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
};

//! @action (transport base)
//!
//! @brief Base for transport-related actions (Play, Stop, Record, Rewind, etc.). Ignores button release.
class TransportAction : public Action
{
public:
    virtual bool IsTransportRelated() { return true; }
    bool IgnoresRelease() const override { return true; }
};

//! @action (display base)
//!
//! @brief Base for display-only actions that send text/value to a display widget but have no Do() behavior.
class DisplayAction : public Action
{
public:
    virtual bool IsDisplayRelated() { return true; }
};
//! @action (track base)
//!
//! @brief Base for track-related actions. Provides helpers to read/toggle boolean track states across selected tracks.
//!
//! @feedback Sends current track state value. Clears widget if no selected tracks.
//!
//! @notes Uses GetSelectedTracks() which returns the navigator's track(s). IncludeMasterTrack() controls whether master track is included — overridden to false for actions like TrackRecordArm.
class TrackAction : public Action
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
    virtual void RequestUpdate(ActionContext* context) {
        if (context->GetSelectedTracks().empty())
            context->ClearWidget();
        else
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
};

//! @action (press-only track base)
//!
//! @brief Track action that fires on press only, ignoring button release.
class PressOnlyTrackAction : public TrackAction
{
public:
    bool IgnoresRelease() const override { return true; }
};

//! @action (track send base)
//!
//! @brief Base for track send actions. Used as the base class for Send-direction template instantiations.
class TrackSendAction : public TrackAction
{
public:
    virtual bool IsTrackSendRelated() { return true; }
};

//! @action (press-only FX base)
//!
//! @brief FX action that fires on press only, ignoring button release. Used for ToggleFXBypass, ToggleFXOffline.
class PressOnlyFXAction : public FXAction
{
public:
    bool IgnoresRelease() const override { return true; }
};

//! @action (track receive base)
//!
//! @brief Base for track receive actions. Used as the base class for Receive-direction template instantiations.
class TrackReceiveAction : public TrackAction
{
public:
    virtual bool IsTrackReceiveRelated() { return true; }
};

//! @action (track display base)
//!
//! @brief Base for track-related display actions. Clears widget when no track is available; otherwise delegates to GetCurrentNormalizedValue.
class TrackDisplayAction : public TrackAction
{
public:
    virtual bool IsDisplayRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
};

//! @action (track meter base)
//!
//! @brief Base for meter/level display actions (TrackOutputMeter, FXGainReductionMeter, etc.).
class TrackMeterAction : public TrackAction
{
public:
    virtual bool IsMeterRelated() { return true; }
};

//! @action (volume base)
//!
//! @brief Base for volume fader actions. Provides full get/set/touch cycle with CheckCurrentTrackContext validation.
//!
//! @feedback Continuous — sends normalized volume (0.0–1.0) to motorized fader or knob widget.
//!
//! @notes Touch support writes automation. Do() skips if value unchanged (prevents feedback loop).
class VolumeAction : public TrackAction
{
public:
    virtual bool IsVolumeRelated() { return true; }

    virtual bool CheckCurrentContext(ActionContext* context) {
        return context->CheckCurrentTrackContext();
    }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (!CheckCurrentContext(context))
            return 0.0;
        return context->GetTrackVolumeNormalized();
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (!CheckCurrentContext(context))
            return context->ClearWidget();
        double value = context->GetTrackVolumeNormalized();
        context->UpdateWidgetValue(value);
        context->SetLastValue(value);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (context->IsSameAsLastValue(value))
            return;
        if (!CheckCurrentContext(context))
            return;
        context->SetTrackVolumeNormalized(value);
        if (!context->GetProvideFeedback())
            context->SetLastValue(value);
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (!CheckCurrentContext(context))
            return;
        double currentValue = GetCurrentNormalizedValue(context);
        this->Do(context, currentValue);
    }
};

//! @action (pan base)
//!
//! @brief Base for pan/width actions. Marker class — concrete pan logic is in subclasses.
class PanAction : public TrackAction

{
public:
    virtual bool IsPanRelated() { return true; }
};

#endif /* control_surface_action_contexts_h */
