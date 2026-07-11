// actions_transport.h

#pragma once

//! @action Rewind
//!
//! @brief Starts rewinding (scrubbing backward). Press again or Play/Stop to cancel.
//!
//! @zone_usage  WidgetName    Rewind
//!
//! @feedback Toggle — 1.0 while rewinding, 0.0 when stopped.
class Rewind : public TransportAction
{
public:
    ActionType GetType() const override { return ActionType::Rewind; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetSurface()->GetIsRewinding())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        context->GetSurface()->StartRewinding();
    }
};

//! @action MoveEditCursor
//!
//! @brief Moves the edit cursor forward/backward by bars or markers, depending on TransportStepAmount.
//!
//! @zone_usage  WidgetName    MoveEditCursor
//!
//! @feedback Always 1.0 (button stays lit).
//!
//! @notes Direction is determined by the incoming value (>0.5=forward, <0.5=backward). Step size is set by TransportStepAmount (Bar or Marker).
class MoveCursor : public TransportAction
{
public:
    ActionType GetType() const override { return ActionType::MoveCursor; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override { return 0.5; }

    virtual void RequestUpdate(ActionContext* context) override { context->UpdateWidgetValue(1.0); }

    virtual void Do(ActionContext* context, double value) override {
        // Below is the Reaper API call, might be worth investigating using this.
        // MoveEditCursor(double adjamt, bool dosel);

        auto amount = context->GetTransportStepAmount();

        if (value > 0.5) {
            if (amount == ActionContext::TransportStepAmount::Bar)
                DAW::SendCommandMessage(41042); // move to next bar

            else if (amount == ActionContext::TransportStepAmount::Marker)
                DAW::SendCommandMessage(40173); // move to next marker/region
        } else if (value < 0.5) {
            if (amount == ActionContext::TransportStepAmount::Bar)
                DAW::SendCommandMessage(41043); // move to previous bar

            else if (amount == ActionContext::TransportStepAmount::Marker)
                DAW::SendCommandMessage(40172); // move to previous marker/region
        }
    }
};

//! @action FastForward
//!
//! @brief Starts fast-forwarding (scrubbing forward). Press again or Play/Stop to cancel.
//!
//! @zone_usage  WidgetName    FastForward
//!
//! @feedback Toggle — 1.0 while fast-forwarding, 0.0 when stopped.
class FastForward : public TransportAction
{
public:
    ActionType GetType() const override { return ActionType::FastForward; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetSurface()->GetIsFastForwarding())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        context->GetSurface()->StartFastForwarding();
    }
};

//! @action Play
//!
//! @brief Starts or resumes playback.
//!
//! @zone_usage  WidgetName    Play
//!
//! @feedback Toggle — 1.0 when playing/recording/paused, 0.0 when stopped. Off during rewind/fast-forward.
class Play : public TransportAction
{
public:
    ActionType GetType() const override { return ActionType::Play; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        int playState = GetPlayState();
        if (playState == PLAYSTATE_PLAYING || playState == PLAYSTATE_PAUSED || playState == PLAYSTATE_RECORDING || playState == PLAYSTATE_PAUSED_WHILE_RECORDING)
            playState = PLAYSTATE_PLAYING;
        else
            playState = PLAYSTATE_STOPPED;

        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            playState = PLAYSTATE_STOPPED;

        return playState;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }

    virtual void Do(ActionContext* context, double value) override {
        context->GetSurface()->Play();
    }
};

//! @action Stop
//!
//! @brief Stops playback/recording.
//!
//! @zone_usage  WidgetName    Stop
//!
//! @feedback Toggle — 1.0 when stopped or paused, 0.0 when playing/recording.
class Stop : public TransportAction
{
public:
    ActionType GetType() const override { return ActionType::Stop; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        int stopState = GetPlayState();
        if (stopState == PLAYSTATE_STOPPED || stopState == PLAYSTATE_PAUSED || stopState == PLAYSTATE_PAUSED_WHILE_RECORDING)
            stopState = PLAYSTATE_PLAYING;
        else
            stopState = PLAYSTATE_STOPPED;

        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            stopState = PLAYSTATE_STOPPED;

        return stopState;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }

    virtual void Do(ActionContext* context, double value) override {
        context->GetSurface()->Stop();
    }
};

//! @action Record
//!
//! @brief Toggles recording on/off.
//!
//! @zone_usage  WidgetName    Record
//!
//! @feedback Toggle — 1.0 when recording (or paused while recording), 0.0 when not.
class Record : public TransportAction
{
public:
    ActionType GetType() const override { return ActionType::Record; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        int recordState = GetPlayState();
        if (recordState == PLAYSTATE_RECORDING || recordState == PLAYSTATE_PAUSED_WHILE_RECORDING)
            recordState = PLAYSTATE_PLAYING;
        else
            recordState = PLAYSTATE_STOPPED;

        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            recordState = PLAYSTATE_STOPPED;
        return recordState;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }

    virtual void Do(ActionContext* context, double value) override {
        context->GetSurface()->Record();
    }
};

//! @action TrackToggleVCASpill
//!
//! @brief Toggles VCA spill view for the mapped track (shows VCA follower tracks on the surface).
//!
//! @zone_usage  WidgetName    TrackToggleVCASpill
//!
//! @feedback Toggle — 1.0 when VCA is spilled for this track, 0.0 when not.
class TrackToggleVCASpill : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::TrackToggleVCASpill; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetIsVCASpilled(track));
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            context->GetTrackNavigationManager()->ToggleVCASpill(track);
    }
};

//! @action TrackToggleFolderSpill
//!
//! @brief Toggles folder spill view for the mapped track (shows child tracks on the surface).
//!
//! @zone_usage  WidgetName    TrackToggleFolderSpill
//!
//! @feedback Toggle — 1.0 when folder is spilled for this track, 0.0 when not.
class TrackToggleFolderSpill : public PressOnlyAction //TrackAction?
{
public:
    ActionType GetType() const override { return ActionType::TrackToggleFolderSpill; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetIsFolderSpilled(track));
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            context->GetTrackNavigationManager()->ToggleFolderSpill(track);
    }
};

//! @action ClearAllSolo
//!
//! @brief Clears solo on all tracks in the project.
//!
//! @zone_usage  WidgetName    ClearAllSolo
//!
//! @feedback Toggle — 1.0 when any track is soloed, 0.0 when none are.
class ClearAllSolo : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ClearAllSolo; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return AnyTrackSolo(NULL);
    }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }

    void Do(ActionContext* context, double value) override {
        SoloAllTracks(0);
    }
};

//! @action GlobalAutoMode
//!
//! @brief Sets the global automation override mode.
//!
//! @zone_usage  WidgetName    GlobalAutoMode 1
//!
//! @feedback Toggle — 1.0 when the current global auto mode matches the int param, 0.0 otherwise.
//!
//! @params Int param: automation mode (0=trim, 1=read, 2=touch, 3=write, 4=latch).
class GlobalAutoMode : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::GlobalAutoMode; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (context->GetIntParam() == GetGlobalAutomationOverride())
            return 1.0;
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }

    virtual void Do(ActionContext* context, double value) override {
        SetGlobalAutomationOverride(context->GetIntParam());
    }
};

//! @action TrackAutoMode
//!
//! @brief Sets the automation mode for selected tracks.
//!
//! @zone_usage  WidgetName    TrackAutoMode 2
//!
//! @feedback Toggle — 1.0 when any selected track's auto mode matches the int param.
//!
//! @params Int param: automation mode (0=trim, 1=read, 2=touch, 3=write, 4=latch).
class TrackAutoMode : public PressOnlyTrackAction
{
public:
    ActionType GetType() const override { return ActionType::TrackAutoMode; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        for (MediaTrack* selectedTrack : context->GetSelectedTracks(true)) {
            if (context->GetIntParam() == GetMediaTrackInfo_Value(selectedTrack, "I_AUTOMODE"))
                return 1.0;
        }
        return 0.0;
    }

    virtual void Do(ActionContext* context, double value) override {
        int mode = context->GetIntParam();
        if (context->GetZone()->GetNavigator()->GetType() == NavigatorType::MasterTrackNavigator) {
            SetMediaTrackInfo_Value(GetMasterTrack(NULL), "I_AUTOMODE", mode);
        } else {
            const vector<MediaTrack*>& selectedTracks = context->GetSelectedTracks(true);
            for (MediaTrack* selectedTrack : selectedTracks)
                SetMediaTrackInfo_Value(selectedTrack, "I_AUTOMODE", mode);
        }
    }
};

//! @action CycleTrackAutoMode
//!
//! @brief Cycles through automation modes for the mapped track on each press.
//!
//! @zone_usage  WidgetName    CycleTrackAutoMode
//!
//! @feedback Text — sends the current automation mode name (e.g. "Read", "Touch").
class CycleTrackAutoMode : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::CycleTrackAutoMode; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetAutoModeDisplayName((int) GetMediaTrackInfo_Value(track, "I_AUTOMODE")));
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            DAW::CycleTrackAutoMode(track);
        }
    }
};

//! @action CycleTimeline
//!
//! @brief Toggles the timeline repeat/loop state on/off.
//!
//! @zone_usage  WidgetName    CycleTimeline
//!
//! @feedback Toggle — 1.0 when repeat is on, 0.0 when off.
class CycleTimeline : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::CycleTimeline; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetSetRepeatEx(NULL, -1);
    }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }

    virtual void Do(ActionContext* context, double value) override {
        GetSetRepeatEx(NULL, !GetSetRepeatEx(NULL, -1));
    }
};

//! @action CycleTrackInputMonitor
//!
//! @brief Cycles through input monitor modes (off → normal → not-when-playing → off) for the mapped track.
//!
//! @zone_usage  WidgetName    CycleTrackInputMonitor
//!
//! @feedback None (clears color only).
class CycleTrackInputMonitor : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::CycleTrackInputMonitor; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            context->GetTrackNavigationManager()->NextInputMonitorMode(track);
    }
};

//! @action TrackAutoModeDisplay
//!
//! @brief Displays the current automation mode name for the mapped track.
//!
//! @zone_usage  DisplayWidget    TrackAutoModeDisplay
//!
//! @feedback Text — sends mode name (e.g. "Trim", "Read", "Touch", "Write", "Latch").
class TrackAutoModeDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackAutoModeDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetAutoModeDisplayName((int) GetMediaTrackInfo_Value(track, "I_AUTOMODE")));
    }
};

//! @action TrackVCALeaderDisplay
//!
//! @brief Displays "Leader" if the mapped track is a VCA leader, empty string otherwise.
//!
//! @zone_usage  DisplayWidget    TrackVCALeaderDisplay
//!
//! @feedback Text — "Leader" or "".
class TrackVCALeaderDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackVCALeaderDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") != 0 || DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") != 0)
                context->UpdateWidgetValue("Leader");
            else
                context->UpdateWidgetValue("");
        } else
            context->UpdateWidgetValue("");
    }
};

//! @action TrackFolderParentDisplay
//!
//! @brief Displays "Parent" if the mapped track is a folder parent, empty string otherwise.
//!
//! @zone_usage  DisplayWidget    TrackFolderParentDisplay
//!
//! @feedback Text — "Parent" or "".
class TrackFolderParentDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackFolderParentDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 1)
                context->UpdateWidgetValue("Parent");
            else
                context->UpdateWidgetValue("");
        } else
            context->UpdateWidgetValue("");
    }
};

//! @action ToggleFolderView
//!
//! @brief Toggles folder view mode on the surface (shows only folder parent tracks).
//!
//! @zone_usage  WidgetName    ToggleFolderView
//!
//! @feedback Toggle — 1.0 when folder view is active, 0.0 when not.
class ToggleFolderView : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleFolderView; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetTrackNavigationManager()->GetIsFolderViewActive())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        context->GetTrackNavigationManager()->ToggleFolderView();
    }
};

//! @action TrackEnterFolder
//!
//! @brief Enters a folder (sets it as the current navigation scope and selects the first child track).
//!
//! @zone_usage  WidgetName    TrackEnterFolder
//!
//! @feedback None (clears color only).
//!
//! @see ExitCurrentFolder
class TrackEnterFolder : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::TrackEnterFolder; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* trackToSelect = context->GetTrackNavigationManager()->SetCurrentFolder(track);
            if (trackToSelect != nullptr) {
                SetOnlyTrackSelected(trackToSelect);
                context->GetPage()->OnTrackSelectionBySurface(trackToSelect);
            }
        }
    }
};

//! @action ExitCurrentFolder
//!
//! @brief Exits the current folder scope and selects the parent folder track.
//!
//! @zone_usage  WidgetName    ExitCurrentFolder
//!
//! @feedback Toggle — 1.0 when inside a folder (can exit), 0.0 when at root level.
//!
//! @see TrackEnterFolder
class ExitCurrentFolder : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ExitCurrentFolder; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetTrackNavigationManager()->IsAtRootFolderLevel())
            context->UpdateWidgetValue(0.0);
        else
            context->UpdateWidgetValue(1.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        MediaTrack* trackToSelect = context->GetTrackNavigationManager()->ExitCurrentFolder();
        if (trackToSelect != nullptr) {
            SetOnlyTrackSelected(trackToSelect);
            context->GetPage()->OnTrackSelectionBySurface(trackToSelect);
        }
    }
};

//! @action GlobalAutoModeDisplay
//!
//! @brief Displays the current global automation override mode name.
//!
//! @zone_usage  DisplayWidget    GlobalAutoModeDisplay
//!
//! @feedback Text — sends mode name string.
class GlobalAutoModeDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::GlobalAutoModeDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetGlobalAutoModeDisplayName());
    }
};

//! @action TrackInputMonitorDisplay
//!
//! @brief Displays the current input monitor mode for the mapped track.
//!
//! @zone_usage  DisplayWidget    TrackInputMonitorDisplay
//!
//! @feedback Text — sends mode name (e.g. "Off", "Normal", "Not when playing").
class TrackInputMonitorDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackInputMonitorDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetCurrentInputMonitorMode(track));
    }
};

//! @action MCUTimeDisplay
//!
//! @brief Sends time display data formatted for MCU-protocol displays (7-segment LED).
//!
//! @zone_usage  TimeDisplay    MCUTimeDisplay
//!
//! @feedback Value — sends 0.0 (MCU time display formatting is handled by the feedback processor).
class MCUTimeDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::MCUTimeDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(0.0);
    }
};

//! @action OSCTimeDisplay
//!
//! @brief Sends formatted time position string for OSC displays.
//!
//! @zone_usage  DisplayWidget    OSCTimeDisplay
//!
//! @feedback Text — sends formatted time string based on current time display mode (bars+beats, hh:mm:ss:fff, samples, or frames).
class OSCTimeDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::OSCTimeDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        char timeStr[MEDBUF];

        double pp = (GetPlayState() & 1) ? GetPlayPosition() : GetCursorPosition();

        int tmode = context->GetCSI()->GetResolvedTimeMode();

        if (tmode == TIMEMODE_SECONDS) {
            double* toptr = context->GetCSI()->GetTimeOffsPtr();
            if (toptr) pp += *toptr;

            snprintf(timeStr, sizeof(timeStr), "%d %d", (int) pp, ((int) (pp * 100.0)) % 100);
        } else if (tmode == TIMEMODE_SAMPLES) {
            format_timestr_pos(pp, timeStr, sizeof(timeStr), TIMEMODE_SAMPLES);
        } else if (tmode == TIMEMODE_FRAMES) {
            format_timestr_pos(pp, timeStr, sizeof(timeStr), TIMEMODE_FRAMES);
        } else if (tmode > TIMEMODE_DEFAULT) {
            int num_measures = 0;
            int currentTimeSignatureNumerator = 0;
            double beats = TimeMap2_timeToBeats(NULL, pp, &num_measures, &currentTimeSignatureNumerator, NULL, NULL) + 0.000000000001;
            double nbeats = floor(beats);
            beats -= nbeats;
            if (num_measures <= 0 && pp < 0.0) --num_measures;
            int* measptr = context->GetCSI()->GetMeasOffsPtr();
            int subBeats = (int) (1000.0 * beats);
            snprintf(timeStr, sizeof(timeStr), "%d %d %03d", (num_measures + 1 + (measptr ? *measptr : 0)), ((int) (nbeats + 1)), subBeats);
        } else {
            double* toptr = context->GetCSI()->GetTimeOffsPtr();
            if (toptr)
                pp += (*toptr);

            int ipp = (int) pp;
            int fr = (int) ((pp - ipp) * 1000.0);

            int hours = (int) (ipp / 3600);
            int minutes = ((int) (ipp / 60)) % 3600;
            int seconds = ((int) ipp) % 60;
            int frames = (int) fr;
            snprintf(timeStr, sizeof(timeStr), "%03d:%02d:%02d:%03d", hours, minutes, seconds, frames);
        }

        context->UpdateWidgetValue(timeStr);
    }
};
