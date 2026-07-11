//
//  actions_track.h
//  reaper_csurf_integrator
//
//  Phase 2.2 — Track volume/pan/mute/solo/select/arm actions.
//
#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolume : public VolumeAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackVolume; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SoftTakeover7BitTrackVolume : public VolumeAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::SoftTakeover7BitTrackVolume; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double trackVolume, trackPan = 0.0;
            GetTrackUIVolPan(track, &trackVolume, &trackPan);
            trackVolume = volToNormalized(trackVolume);
            
            if ( fabs(value - trackVolume) < 0.025) // GAW -- Magic number -- ne touche pas
                CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SoftTakeover14BitTrackVolume : public VolumeAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::SoftTakeover14BitTrackVolume; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double trackVolume, trackPan = 0.0;
            GetTrackUIVolPan(track, &trackVolume, &trackPan);
            trackVolume = volToNormalized(trackVolume);
            
            if ( fabs(value - trackVolume) < 0.0025) // GAW -- Magic number -- ne touche pas
                CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolumeDB : public VolumeAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackVolumeDB; }
    
    virtual double GetCurrentDBValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            return VAL2DB(vol);
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(GetCurrentDBValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, DB2VAL(value), false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, DB2VAL(GetCurrentDBValue(context)), false), NULL);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPan : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPan; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                return panToNormalized(pan);
            }
        }
        
        return 0.0;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(value), false), NULL);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false), NULL);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanPercent : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanPercent; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                context->UpdateWidgetValue(pan  *100.0);
            }
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, value / 100.0, false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, pan, false), NULL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanWidth : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanWidth; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_WIDTH"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_OnWidthChange(track, normalizedToPan(value), false);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_OnWidthChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanWidthPercent : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanWidthPercent; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_WIDTH")  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_OnWidthChange(track, value / 100.0, false);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_OnWidthChange(track, GetMediaTrackInfo_Value(track, "D_WIDTH"), false);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanL : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanL; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANL", &pan);
            }
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanLPercent : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanLPercent; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_DUALPANL")  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double panFromPercent = value / 100.0;
                GetSetMediaTrackInfo(track, "D_DUALPANL", &panFromPercent);
            }
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double panL = GetMediaTrackInfo_Value(track, "D_DUALPANL");
                GetSetMediaTrackInfo(track, "D_DUALPANL", &panL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanR : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanR; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANR", &pan);
            }
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanRPercent : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanRPercent; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_DUALPANR")  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double panFromPercent = value / 100.0;
                GetSetMediaTrackInfo(track, "D_DUALPANR", &panFromPercent);
            }
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double panL = GetMediaTrackInfo_Value(track, "D_DUALPANR");
                GetSetMediaTrackInfo(track, "D_DUALPANR", &panL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanAutoLeft : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanAutoLeft; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL"));
            else
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                return panToNormalized(pan);
            }
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL")));
            else
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANL", &pan);
            }
            else
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(value), false), NULL);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
            else
            {
                context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false), NULL);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanAutoRight : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanAutoRight; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR"));
            else
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_WIDTH"));
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR")));
            else
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANR", &pan);
            }
            else
                CSurf_OnWidthChange(track, normalizedToPan(value), false);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
            else
            {
                context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
                CSurf_OnWidthChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false);

            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackRecordArm : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    bool IncludeMasterTrack() const override { return false; }
public:
    ActionType GetType() const override { return ActionType::TrackRecordArm; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackRecordArm);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        ToggleTrackState(context, DAW::GetTrackRecordArm, DAW::SetTrackRecordArm);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackRecordArmDisplay : public TrackDisplayAction
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackRecordArmDisplay; }

    virtual void RequestUpdate(ActionContext* context) override
    {
        if (MediaTrack* track = context->GetTrack())
        {
            double state = GetMediaTrackInfo_Value(track, "I_RECARM");

            if (state > 0.5)
                context->UpdateWidgetValue("ARM");
            else
                context->UpdateWidgetValue("");
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackMute : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackMute; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackMute);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        ToggleTrackState(context, DAW::GetTrackMute, DAW::SetTrackMute);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackEffectsBypass : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackEffectsBypass; }
    
    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackEffectsBypass);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        ToggleTrackState(context, DAW::GetTrackEffectsBypass, DAW::SetTrackEffectsBypass);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSolo : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSolo; }
    
    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackSolo);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        ToggleTrackState(context, DAW::GetTrackSolo, DAW::SetTrackSolo);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackInvertPolarity : public TrackAction //TODO: rename TrackInvertPhase
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackInvertPolarity; }
    
    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackInvertPhase);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        ToggleTrackState(context, DAW::GetTrackInvertPhase, DAW::SetTrackInvertPhase);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSelect : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSelect; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_SELECTED");
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            CSurf_SetSurfaceSelected(track, CSurf_OnSelectedChange(track, ! GetMediaTrackInfo_Value(track, "I_SELECTED")), NULL);
            context->GetPage()->OnTrackSelectionBySurface(track);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackUniqueSelect : public Action // TrackAction?
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackUniqueSelect; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_SELECTED");
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            SetOnlyTrackSelected(track);
            context->GetPage()->OnTrackSelectionBySurface(track);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackRangeSelect : public Action // TrackAction?
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackRangeSelect; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_SELECTED");
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        int currentlySelectedCount = 0;
        int selectedTrackIndex = 0;
        int trackIndex = 0;
        
       
        for (int i = 1; i <= context->GetPage()->GetTrackNavigationManager()->GetNumTracks(); ++i)
        {
            MediaTrack *currentTrack = context->GetPage()->GetTrackNavigationManager()->GetTrackFromId(i);
           
            if (currentTrack == NULL)
                continue;
            
            if (currentTrack == context->GetTrack())
                trackIndex = i;
            
            if (GetMediaTrackInfo_Value(currentTrack, "I_SELECTED"))
            {
                selectedTrackIndex = i;
                currentlySelectedCount++;
            }
        }
        
        if (currentlySelectedCount != 1)
            return;
        
        int lowerBound = trackIndex < selectedTrackIndex ? trackIndex : selectedTrackIndex;
        int upperBound = trackIndex > selectedTrackIndex ? trackIndex : selectedTrackIndex;

        for (int i = lowerBound; i <= upperBound; ++i)
        {
            MediaTrack *currentTrack = context->GetPage()->GetTrackNavigationManager()->GetTrackFromId(i);
            
            if (currentTrack == NULL)
                continue;
            
            if (context->GetPage()->GetTrackNavigationManager()->GetIsTrackVisible(currentTrack))
                CSurf_SetSurfaceSelected(currentTrack, CSurf_OnSelectedChange(currentTrack, 1), NULL);
        }
        
        MediaTrack *lowestTrack = context->GetPage()->GetTrackNavigationManager()->GetTrackFromId(lowerBound);
        
        if (lowestTrack != NULL)
            context->GetPage()->OnTrackSelectionBySurface(lowestTrack);
    }
};
