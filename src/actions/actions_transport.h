// actions_transport.h

#pragma once

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

class TrackAutoModeDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackAutoModeDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetAutoModeDisplayName((int) GetMediaTrackInfo_Value(track, "I_AUTOMODE")));
    }
};

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

class GlobalAutoModeDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::GlobalAutoModeDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetGlobalAutoModeDisplayName());
    }
};

class TrackInputMonitorDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackInputMonitorDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetCurrentInputMonitorMode(track));
    }
};

class MCUTimeDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::MCUTimeDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(0.0);
    }
};

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
