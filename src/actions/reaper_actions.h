//
//  control_surface_Reaper_actions.h
//  reaper_csurf_integrator
//
//

#ifndef control_surface_Reaper_actions_h
#define control_surface_Reaper_actions_h

#include "action_base.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
class FXParam : public FXAction
//////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FXParam; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class JSFXParam : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::JSFXParam; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TCPFXParam : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TCPFXParam; }

    virtual bool CheckCurrentContext(ActionContext *context) {
        return context->CheckCurrentTcpFxContext();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LastTouchedFXParam : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::LastTouchedFXParam; }

    bool CheckCurrentContext(ActionContext* context) {
        return context->CheckLastTouchedFxContext();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleFXBypass : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::ToggleFXBypass; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (DAW::GetTrackBypass(track))
                context->UpdateWidgetValue(0.0);
            else if (TrackFX_GetCount(track) > context->GetSlotIndex())
            {
                if (TrackFX_GetEnabled(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue(1.0);
                else
                    context->UpdateWidgetValue(0.0);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
            TrackFX_SetEnabled(track, context->GetSlotIndex(), ! TrackFX_GetEnabled(track, context->GetSlotIndex()));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXBypassDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FXBypassDisplay; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (DAW::GetTrackBypass(track))
                context->UpdateWidgetValue("Bypassed");
            else if (TrackFX_GetCount(track) > context->GetSlotIndex())
            {
                if (TrackFX_GetEnabled(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue("Enabled");
                else
                    context->UpdateWidgetValue("Bypassed");
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleFXOffline : public FXAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::ToggleFXOffline; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (TrackFX_GetCount(track) > context->GetSlotIndex())
            {
                if (TrackFX_GetOffline(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue(0.0);
                else
                    context->UpdateWidgetValue(1.0);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
            TrackFX_SetOffline(track, context->GetSlotIndex(), ! TrackFX_GetOffline(track, context->GetSlotIndex()));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXOfflineDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FXOfflineDisplay; }
   
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (TrackFX_GetCount(track) > context->GetSlotIndex())
            {
                if (TrackFX_GetOffline(track, context->GetSlotIndex()))
                    context->UpdateWidgetValue("Offline");
                else
                    context->UpdateWidgetValue("Online");
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

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
        
       
        for (int i = 1; i <= context->GetPage()->GetNumTracks(); ++i)
        {
            MediaTrack *currentTrack = context->GetPage()->GetTrackFromId(i);
           
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
            MediaTrack *currentTrack = context->GetPage()->GetTrackFromId(i);
            
            if (currentTrack == NULL)
                continue;
            
            if (context->GetPage()->GetIsTrackVisible(currentTrack))
                CSurf_SetSurfaceSelected(currentTrack, CSurf_OnSelectedChange(currentTrack, 1), NULL);
        }
        
        MediaTrack *lowestTrack = context->GetPage()->GetTrackFromId(lowerBound);
        
        if (lowestTrack != NULL)
            context->GetPage()->OnTrackSelectionBySurface(lowestTrack);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendVolume : public VolumeAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendVolume; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);
            return volToNormalized(vol);
        }
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
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIVol(track, context->GetSlotIndex() + numHardwareSends, normalizedToVol(value), 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIVol(track, context->GetSlotIndex() + numHardwareSends, vol, 1);
            else
                SetTrackSendUIVol(track, context->GetSlotIndex() + numHardwareSends, vol, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendVolumeDB : public VolumeAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendVolumeDB; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetParamIndex() + numHardwareSends, &vol, &pan);
            context->UpdateWidgetValue(VAL2DB(vol));
        }
        else
            context->ClearWidget();
        
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIVol(track, context->GetParamIndex() + numHardwareSends, DB2VAL(value), 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetParamIndex() + numHardwareSends, &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIVol(track, context->GetParamIndex() + numHardwareSends, vol, 1);
            else
                SetTrackSendUIVol(track, context->GetParamIndex() + numHardwareSends, vol, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPan : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendPan; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);
            return panToNormalized(pan);
        }
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
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIPan(track, context->GetSlotIndex() + numHardwareSends, normalizedToPan(value), 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIPan(track, context->GetSlotIndex() + numHardwareSends, pan, 1);
            else
                SetTrackSendUIPan(track, context->GetSlotIndex() + numHardwareSends, pan, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPanPercent : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendPanPercent; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetParamIndex() + numHardwareSends, &vol, &pan);
            context->UpdateWidgetValue(pan  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIPan(track, context->GetParamIndex() + numHardwareSends, value / 100.0, 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            double vol, pan = 0.0;
            GetTrackSendUIVolPan(track, context->GetParamIndex() + numHardwareSends, &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIPan(track, context->GetParamIndex() + numHardwareSends, pan, 1);
            else
                SetTrackSendUIPan(track, context->GetParamIndex() + numHardwareSends, pan, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendMute : public TrackSendAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendMute; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int numHardwareSends = GetTrackNumSends(track, 1);
            bool mute = false;
            GetTrackSendUIMute(track, context->GetSlotIndex() + numHardwareSends, &mute);
            return mute;
        }
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
            ToggleTrackSendUIMute(track, context->GetSlotIndex() + GetTrackNumSends(track, 1));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendInvertPolarity : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendInvertPolarity; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_PHASE");
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
            bool reversed = ! GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_PHASE");
            
            GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "B_PHASE", &reversed);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendStereoMonoToggle : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendStereoMonoToggle; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_MONO");
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
            bool mono = ! GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_MONO");
            
            GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "B_MONO", &mono);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPrePost : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendPrePost; }
       
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            int mode = (int)GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "I_SENDMODE");
            
            if (mode == 0)
                mode = 1; // switch to pre FX
            else if (mode == 1)
                mode = 3; // switch to post FX
            else
                mode = 0; // switch to post pan
            
            GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "I_SENDMODE", &mode);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveVolume : public VolumeAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceiveVolume; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            return volToNormalized(vol);
        }
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
        if (MediaTrack *track = context->GetTrack())
            SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), normalizedToVol(value), 0);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, 1);
            else
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveVolumeDB : public VolumeAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceiveVolumeDB; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            context->UpdateWidgetValue(VAL2DB(vol));
        }
        else
            context->ClearWidget();
        
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), DB2VAL(value), 0);
        }
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, 1);
            else
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePan : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceivePan; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            return panToNormalized(pan);
        }
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
        if (MediaTrack *track = context->GetTrack())
            SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), normalizedToPan(value), 0);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, 1);
            else
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePanPercent : public PanAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceivePanPercent; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            context->UpdateWidgetValue(pan  *100.0);
        }
        else
            context->ClearWidget();
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), value / 100.0, 0);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            
            if (value == 0)
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, 1);
            else
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, 0);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveMute : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceiveMute; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            bool mute = false;
            GetTrackReceiveUIMute(track, context->GetSlotIndex(), &mute);
            return mute;
        }
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
            bool isMuted = ! GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MUTE");
            
            GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "B_MUTE", &isMuted);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveInvertPolarity : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceiveInvertPolarity; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_PHASE");
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
            bool reversed = ! GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_PHASE");
            
            GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "B_PHASE", &reversed);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveStereoMonoToggle : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceiveStereoMonoToggle; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            return GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MONO");
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
            bool mono = ! GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MONO");
            
            GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "B_MONO", &mono);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePrePost : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceivePrePost; }
        
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
        {
            int mode = (int)GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "I_SENDMODE");
            
            if (mode == 0)
                mode = 1; // switch to pre FX
            else if (mode == 1)
                mode = 3; // switch to post FX
            else
                mode = 0; // switch to post pan

            GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "I_SENDMODE", &mode);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXNameDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FXNameDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetName());
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXMenuNameDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FXMenuNameDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char alias[MEDBUF];
            alias[0] = 0;
            
            if (context->GetSlotIndex() < TrackFX_GetCount(track))
            {
                context->GetSurface()->GetZoneManager()->GetName(track, context->GetSlotIndex(), alias, sizeof(alias));
                context->UpdateWidgetValue(alias);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SpeakFXMenuName : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::SpeakFXMenuName; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            char alias[MEDBUF];
            alias[0] = 0;
            
            if (context->GetSlotIndex() < TrackFX_GetCount(track))
            {
                context->GetSurface()->GetZoneManager()->GetName(track, context->GetSlotIndex(), alias, sizeof(alias));
                context->GetCSI()->Speak(alias);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXParamNameDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FXParamNameDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (context->GetFXParamDisplayName()[0])
                context->UpdateWidgetValue(context->GetFXParamDisplayName());
            else
            {
                char tmp[MEDBUF];
                TrackFX_GetParamName(track, context->GetSlotIndex(), context->GetParamIndex(), tmp, sizeof(tmp));
                context->UpdateWidgetValue(tmp);
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TCPFXParamNameDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TCPFXParamNameDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int index = context->GetIntParam();
            
            if (CountTCPFXParms(NULL, track) > index)
            {
                int fxIndex = 0;
                int paramIndex = 0;
                
                char tmp[MEDBUF];
                if (GetTCPFXParm(NULL, track, index, &fxIndex, &paramIndex))
                    context->UpdateWidgetValue(context->GetCSI()->GetTCPFXParamName(track, fxIndex, paramIndex, tmp, sizeof(tmp)));
                else
                    context->ClearWidget();
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXParamValueDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FXParamValueDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char fxParamValue[128];
            TrackFX_GetFormattedParamValue(track, context->GetSlotIndex(), context->GetParamIndex(), fxParamValue, sizeof(fxParamValue));
            context->UpdateWidgetValue(fxParamValue);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TCPFXParamValueDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TCPFXParamValueDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            int index = context->GetIntParam();
            
            if (CountTCPFXParms(NULL, track) > index)
            {
                int fxIndex = 0;
                int paramIndex = 0;
                
                if (GetTCPFXParm(NULL, track, index, &fxIndex, &paramIndex))
                {
                    char fxParamValue[128];
                    TrackFX_GetFormattedParamValue(track, fxIndex, paramIndex, fxParamValue, sizeof(fxParamValue));
                    context->UpdateWidgetValue(fxParamValue);
                }
                else
                    context->ClearWidget();
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LastTouchedFXParamNameDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::LastTouchedFXParamNameDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        int trackNum = 0;
        int fxSlotNum = 0;
        int fxParamNum = 0;
        
        if (GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum))
        {
            if (MediaTrack *track = DAW::GetTrack(trackNum))
            {
                char tmp[MEDBUF];
                TrackFX_GetParamName(track, fxSlotNum, fxParamNum, tmp, sizeof(tmp));
                context->UpdateWidgetValue(tmp);
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LastTouchedFXParamValueDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::LastTouchedFXParamValueDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        int trackNum = 0;
        int fxSlotNum = 0;
        int fxParamNum = 0;
        
        if (GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum))
        {
            if (MediaTrack *track = DAW::GetTrack(trackNum))
            {
                char fxParamValue[128];
                TrackFX_GetFormattedParamValue(track, fxSlotNum, fxParamNum, fxParamValue, sizeof(fxParamValue));
                context->UpdateWidgetValue(fxParamValue);
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendNameDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendNameDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            const char *sendTrackName = "";
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
                sendTrackName = (const char *)GetSetMediaTrackInfo(destTrack, "P_NAME", NULL);
            context->UpdateWidgetValue(sendTrackName);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SpeakTrackSendDestination : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::SpeakTrackSendDestination; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
            {
                const char *sendTrackName = (const char *)GetSetMediaTrackInfo(destTrack, "P_NAME", NULL);
                char tmp[MEDBUF];
                snprintf(tmp, sizeof(tmp), "Track %d%s%s",
                    context->GetPage()->GetIdFromTrack(destTrack),
                    sendTrackName && *sendTrackName ? " " : "",
                    sendTrackName ? sendTrackName : "");
                context->GetCSI()->Speak(tmp);
            }
            else
                context->GetCSI()->Speak("No Send Track");
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendVolumeDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendVolumeDisplay; }
    virtual bool IsVolumeRelated() { return true; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
            {
                int numHardwareSends = GetTrackNumSends(track, 1);
                double vol, pan = 0.0;
                GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);

                char trackVolume[128];
                snprintf(trackVolume, sizeof(trackVolume), "%7.2lf", VAL2DB(vol));
                context->UpdateWidgetValue(trackVolume);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPanDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendPanDisplay; }
    virtual bool IsPanRelated() { return true; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
            {
                int numHardwareSends = GetTrackNumSends(track, 1);
                double vol, pan = 0.0;
                GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHardwareSends, &vol, &pan);

                char tmp[MEDBUF];
                context->UpdateWidgetValue(context->GetPanValueString(pan, "", tmp, sizeof(tmp)));
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendStereoMonoDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendStereoMonoDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);
            if (destTrack)
            {
                if (GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "B_MONO"))
                    context->UpdateWidgetValue("mono");
                else
                    context->UpdateWidgetValue("stereo");
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackSendPrePostDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackSendPrePostDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *destTrack = (MediaTrack *)GetSetTrackSendInfo(track, 0, context->GetSlotIndex(), "P_DESTTRACK", 0);;
            if (destTrack)
            {
                // I_SENDMODE : returns int *, 0=post-fader, 1=pre-fx, 2=post-fx (deprecated), 3=post-fx
                
                double prePostVal = GetTrackSendInfo_Value(track, 0, context->GetSlotIndex(), "I_SENDMODE");
                
                const char *prePostValueString = "";
                
                if (prePostVal == 0)
                    prePostValueString = "PostPan";
                else if (prePostVal == 1)
                    prePostValueString = "PreFX";
                else if (prePostVal == 2 || prePostVal == 3)
                    prePostValueString = "PostFX";
                
                context->UpdateWidgetValue(prePostValueString);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveNameDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceiveNameDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                const char *receiveTrackName = (const char *)GetSetMediaTrackInfo(srcTrack, "P_NAME", NULL);
                context->UpdateWidgetValue(receiveTrackName);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SpeakTrackReceiveSource : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::SpeakTrackReceiveSource; }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                const char *receiveTrackName = (const char *)GetSetMediaTrackInfo(srcTrack, "P_NAME", NULL);
                char tmp[MEDBUF];
                snprintf(tmp, sizeof(tmp), "Track %d%s%s", context->GetPage()->GetIdFromTrack(srcTrack),
                    receiveTrackName && *receiveTrackName ? " " : "",
                    receiveTrackName ? receiveTrackName : "");
                context->GetCSI()->Speak(tmp);
            }
            else
                context->GetCSI()->Speak("No Receive Track");
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveVolumeDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceiveVolumeDisplay; }
    virtual bool IsVolumeRelated() { return true; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                char trackVolume[128];
                snprintf(trackVolume, sizeof(trackVolume), "%7.2lf", VAL2DB(GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "D_VOL")));
                context->UpdateWidgetValue(trackVolume);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePanDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceivePanDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                double panVal = GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "D_PAN");
                
                char tmp[MEDBUF];
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "", tmp, sizeof(tmp)));
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceiveStereoMonoDisplay : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceiveStereoMonoDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                if (GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MONO"))
                    context->UpdateWidgetValue("mono");
                else
                    context->UpdateWidgetValue("stereo");
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackReceivePrePostDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackReceivePrePostDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            MediaTrack *srcTrack = (MediaTrack *)GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "P_SRCTRACK", 0);
            if (srcTrack)
            {
                // I_SENDMODE : returns int *, 0=post-fader, 1=pre-fx, 2=post-fx (deprecated), 3=post-fx
                
                double prePostVal = GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "I_SENDMODE");
                
                const char *prePostValueString = "";
                
                if (prePostVal == 0)
                    prePostValueString = "PostPan";
                else if (prePostVal == 1)
                    prePostValueString = "PreFX";
                else if (prePostVal == 2 || prePostVal == 3)
                    prePostValueString = "PostFX";
                
                context->UpdateWidgetValue(prePostValueString);
            }
            else
                context->ClearWidget();
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FixedTextDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FixedTextDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetStringParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FixedRGBColorDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FixedRGBColorDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(0.0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackNameDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackNameDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char buf[MEDBUF];
            
            GetTrackName(track, buf, sizeof(buf));
            
            context->UpdateWidgetValue(buf);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackNumberDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackNumberDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double index = GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER");
            char idx[128];
            snprintf(idx, sizeof(idx), "%d", (int)index);

            context->UpdateWidgetValue(idx);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackRecordInputDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackRecordInputDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            /*
            // I_RECINPUT : int  *: record input, <0=no input.
            if 4096 set, input is MIDI and low 5 bits represent channel (0=all, 1-16=only chan), next 6 bits represent physical input (63=all, 62=VKB).
            If 4096 is not set, low 10 bits (0..1023) are input start channel (ReaRoute/Loopback start at 512).
            If 2048 is set, input is multichannel input (using track channel count).
            If 1024 is set, input is stereo input, otherwise input is mono.
            */
            
            char inputDisplay[MEDBUF];
            
            int input = (int)GetMediaTrackInfo_Value(track, "I_RECINPUT");

            if (input < 0)
                lstrcpyn_safe(inputDisplay, "None", sizeof(inputDisplay));
            else if (input & 4096)
            {
                int channel = input & 0x1f;
                
                if (channel == 0)
                    lstrcpyn_safe(inputDisplay, "MD All", sizeof(inputDisplay));
                else
                    snprintf(inputDisplay, sizeof(inputDisplay), "MD %d", channel);
            }
            else if (input & 2048)
            {
                lstrcpyn_safe(inputDisplay, "Multi", sizeof(inputDisplay));
            }
            else if (input & 1024)
            {
                int channels = input ^ 1024;
                
                snprintf(inputDisplay, sizeof(inputDisplay), "%d+%d", channels + 1, channels + 2);
            }
            else
            {
                snprintf(inputDisplay, sizeof(inputDisplay), "Mno %d", input + 1);
            }

            context->UpdateWidgetValue(inputDisplay);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackInvertPolarityDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackInvertPolarityDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetMediaTrackInfo_Value(track, "B_PHASE") == 0)
                context->UpdateWidgetValue("Normal");
            else
                context->UpdateWidgetValue("Invert");
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolumeDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackVolumeDisplay; }
    virtual bool IsVolumeRelated() { return true; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);

            char trackVolume[128];
            snprintf(trackVolume, sizeof(trackVolume), "%7.2lf", VAL2DB(vol));
            context->UpdateWidgetValue(trackVolume);
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);

            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(pan, "", tmp, sizeof(tmp)));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanWidthDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanWidthDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double widthVal = GetMediaTrackInfo_Value(track, "D_WIDTH");
            
            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanWidthValueString(widthVal, tmp, sizeof(tmp)));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanLeftDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanLeftDisplay; }
    virtual bool IsPanRelated() { return true; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANL");
            
            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(panVal, "L", tmp, sizeof(tmp)));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanRightDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanRightDisplay; }
    virtual bool IsPanRelated() { return true; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANR");
            
            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(panVal, "R", tmp, sizeof(tmp)));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanAutoLeftDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanAutoLeftDisplay; }
    virtual bool IsPanRelated() { return true; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char tmp[MEDBUF];
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANL");
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "L", tmp, sizeof(tmp)));
            }
            else
            {
                double vol, pan = 0.0;
                GetTrackUIVolPan(track, &vol, &pan);
                context->UpdateWidgetValue(context->GetPanValueString(pan, "", tmp, sizeof(tmp)));
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackPanAutoRightDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackPanAutoRightDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            char tmp[MEDBUF];
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
            {
                double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANR");
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "R", tmp, sizeof(tmp)));
            }
            else
            {
                double widthVal = GetMediaTrackInfo_Value(track, "D_WIDTH");
                context->UpdateWidgetValue(context->GetPanWidthValueString(widthVal, tmp, sizeof(tmp)));
            }
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Rewind : public TransportAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::Rewind; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetSurface()->GetIsRewinding())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        context->GetSurface()->StartRewinding();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MoveCursor : public TransportAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::MoveCursor; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return  0.5;
    }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(1.0);
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        // Below is the Reaper API call, might be worth investigating using this.
        // MoveEditCursor(double adjamt, bool dosel);
        
        const char *amount = context->GetStringParam();
        
        if (value > 0.5)
        {
            if (IsSameString(amount, "Bar")) //FIXME: replace all string- with enum- comparison and do early choice in context construction
                DAW::SendCommandMessage(41042); // move to next bar
            
            else if (IsSameString(amount, "Marker")) //FIXME: replace all string- with enum- comparison and do early choice in context construction
                DAW::SendCommandMessage(40173); // move to next marker/region
        }
        else if (value < 0.5)
        {
            if (IsSameString(amount, "Bar")) //FIXME: replace all string- with enum- comparison and do early choice in context construction
                DAW::SendCommandMessage(41043); // move to previous bar
            
            else if (IsSameString(amount, "Marker")) //FIXME: replace all string- with enum- comparison and do early choice in context construction
                DAW::SendCommandMessage(40172); // move to previous marker/region
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FastForward : public TransportAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FastForward; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (context->GetSurface()->GetIsFastForwarding())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->StartFastForwarding();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Play : public TransportAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::Play; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        int playState = GetPlayState();
        if (playState == 1 || playState == 2 || playState == 5 || playState == 6) // playing or paused or recording or paused whilst recording
            playState = 1;
        else playState = 0;

        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            playState = 0;

        return playState;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->Play();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Stop : public TransportAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::Stop; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        int stopState = GetPlayState();
        if (stopState == 0 || stopState == 2 || stopState == 6) // stopped or paused or paused whilst recording
            stopState = 1;
        else stopState = 0;
       
        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            stopState = 0;
        
        return stopState;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->Stop();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Record : public TransportAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::Record; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        int recordState = GetPlayState();
        if (recordState == 5 || recordState == 6) // recording or paused whilst recording
            recordState = 1;
        else recordState = 0;
        
        if (context->GetSurface()->GetIsRewinding() || context->GetSurface()->GetIsFastForwarding())
            recordState = 0;
        
        return recordState;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        context->GetSurface()->Record();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackToggleVCASpill : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackToggleVCASpill; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetIsVCASpilled(track));
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
            context->GetPage()->ToggleVCASpill(track);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackToggleFolderSpill : public Action //TrackAction?
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackToggleFolderSpill; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetIsFolderSpilled(track));
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack())
            context->GetPage()->ToggleFolderSpill(track);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ClearAllSolo : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::ClearAllSolo; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return AnyTrackSolo(NULL);
    }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        SoloAllTracks(0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GlobalAutoMode : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::GlobalAutoMode; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (context->GetIntParam() == GetGlobalAutomationOverride())
            return 1.0;
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        SetGlobalAutomationOverride(context->GetIntParam());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackAutoMode : public TrackAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

    virtual void Do(ActionContext* context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        int mode = context->GetIntParam();
        if (context->GetZone()->GetNavigator()->GetType() == NavigatorType::MasterTrackNavigator) {
            SetMediaTrackInfo_Value(GetMasterTrack(NULL), "I_AUTOMODE", mode);
        } else {
            const vector<MediaTrack*> &selectedTracks = context->GetSelectedTracks(true);
            for (MediaTrack* selectedTrack : selectedTracks)
                SetMediaTrackInfo_Value(selectedTrack, "I_AUTOMODE", mode);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CycleTrackAutoMode : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::CycleTrackAutoMode; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetAutoModeDisplayName((int)GetMediaTrackInfo_Value(track, "I_AUTOMODE")));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        if (MediaTrack *track = context->GetTrack()) {
            DAW::CycleTrackAutoMode(track);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CycleTimeline : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::CycleTimeline; }

    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        return GetSetRepeatEx(NULL, -1);
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        GetSetRepeatEx(NULL, ! GetSetRepeatEx(NULL, -1));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CycleTrackInputMonitor : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::CycleTrackInputMonitor; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;

        if (MediaTrack *track = context->GetTrack())
            context->GetPage()->NextInputMonitorMode(track);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackAutoModeDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackAutoModeDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetAutoModeDisplayName((int)GetMediaTrackInfo_Value(track, "I_AUTOMODE")));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVCALeaderDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackVCALeaderDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (DAW::GetTrackGroupMembership(track, "VOLUME_VCA_LEAD") != 0 || DAW::GetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD") != 0)
                context->UpdateWidgetValue("Leader");
            else
                context->UpdateWidgetValue("");
        }
        else
            context->UpdateWidgetValue("");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackFolderParentDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackFolderParentDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            if (GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") == 1)
                context->UpdateWidgetValue("Parent");
            else
                context->UpdateWidgetValue("");
        }
        else
            context->UpdateWidgetValue("");
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleFolderView : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::ToggleFolderView; }
    
    virtual void RequestUpdate(ActionContext* context) override
    {
        if (context->GetPage()->GetIsFolderViewActive())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (value == 0.0) return; // ignore button releases

        context->GetPage()->ToggleFolderView();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackEnterFolder : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackEnterFolder; }

    virtual void RequestUpdate(ActionContext* context) override
    {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (value == 0.0) return; // ignore button releases

        if (MediaTrack* track = context->GetTrack())
        {
            MediaTrack* trackToSelect = context->GetPage()->SetCurrentFolder(track);
            if (trackToSelect != nullptr)
            {
                SetOnlyTrackSelected(trackToSelect);
                context->GetPage()->OnTrackSelectionBySurface(trackToSelect);
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ExitCurrentFolder : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::ExitCurrentFolder; }

    virtual void RequestUpdate(ActionContext* context) override
    {
        if (context->GetPage()->IsAtRootFolderLevel())
            context->UpdateWidgetValue(0.0);
        else
            context->UpdateWidgetValue(1.0);
    }

    virtual void Do(ActionContext* context, double value) override
    {
        if (value == 0.0) return; // ignore button releases

        MediaTrack* trackToSelect = context->GetPage()->ExitCurrentFolder();
        if (trackToSelect != nullptr)
        {
            SetOnlyTrackSelected(trackToSelect);
            context->GetPage()->OnTrackSelectionBySurface(trackToSelect);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GlobalAutoModeDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::GlobalAutoModeDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetGlobalAutoModeDisplayName());
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackInputMonitorDisplay : public TrackDisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackInputMonitorDisplay; }
    
    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
            context->UpdateWidgetValue(context->GetPage()->GetCurrentInputMonitorMode(track));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MCUTimeDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::MCUTimeDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(0.0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OSCTimeDisplay : public DisplayAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::OSCTimeDisplay; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        char timeStr[MEDBUF];
        
        double pp=(GetPlayState()&1) ? GetPlayPosition() : GetCursorPosition();

        int *tmodeptr = context->GetCSI()->GetTimeMode2Ptr();
        
        int tmode = 0;
        
        if (tmodeptr && (*tmodeptr)>=0) tmode = *tmodeptr;
        else
        {
            tmodeptr = context->GetCSI()->GetTimeModePtr();
            if (tmodeptr)
                tmode=*tmodeptr;
        }

        if (tmode == 3) // seconds
        {
            double *toptr = context->GetCSI()->GetTimeOffsPtr();
            
            if (toptr)
                pp+=*toptr;
            
            snprintf(timeStr, sizeof(timeStr), "%d %d", (int)pp, ((int)(pp*100.0))%100);
        }
        else if (tmode==4) // samples
        {
            format_timestr_pos(pp, timeStr, sizeof(timeStr), 4);
        }
        else if (tmode == 5) // frames
        {
            format_timestr_pos(pp, timeStr, sizeof(timeStr), 5);
        }
        else if (tmode > 0)
        {
            int num_measures=0;
            int currentTimeSignatureNumerator=0;
            double beats=TimeMap2_timeToBeats(NULL,pp,&num_measures,&currentTimeSignatureNumerator,NULL,NULL)+ 0.000000000001;
            double nbeats = floor(beats);
            
            beats -= nbeats;
               
            if (num_measures <= 0 && pp < 0.0)
                --num_measures;
            
            int *measptr = context->GetCSI()->GetMeasOffsPtr();
            int subBeats = (int)(1000.0  *beats);
          
            snprintf(timeStr, sizeof(timeStr), "%d %d %03d", (num_measures+1+(measptr ? *measptr : 0)), ((int)(nbeats + 1)), subBeats);
        }
        else
        {
            double *toptr = context->GetCSI()->GetTimeOffsPtr();
            if (toptr) pp+=(*toptr);
            
            int ipp=(int)pp;
            int fr=(int)((pp-ipp)*1000.0);
            
            int hours = (int)(ipp/3600);
            int minutes = ((int)(ipp/60)) %3600;
            int seconds = ((int)ipp) %60;
            int frames =(int)fr;
            snprintf(timeStr, sizeof(timeStr), "%03d:%02d:%02d:%03d", hours, minutes, seconds, frames);
        }

        context->UpdateWidgetValue(timeStr);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackOutputMeter : public TrackMeterAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackOutputMeter; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {           
            if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                context->ClearWidget();
            else
                context->UpdateWidgetValue(volToNormalized(Track_GetPeakInfo(track, context->GetIntParam())));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackOutputMeterAverageLR : public TrackMeterAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackOutputMeterAverageLR; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double lrVol = (Track_GetPeakInfo(track, 0) + Track_GetPeakInfo(track, 1)) / 2.0;
            
            if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                context->ClearWidget();
            else
                context->UpdateWidgetValue(volToNormalized(lrVol));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolumeWithMeterAverageLR : public TrackMeterAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackVolumeWithMeterAverageLR; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            return volToNormalized(vol);
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        int stopState = GetPlayState();

        if (stopState == 0 || stopState == 2 || stopState == 6) // stopped or paused or paused whilst recording
        {
            if (context->GetTrack())
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
            else
                context->ClearWidget();
        }
        else
        {
            if (MediaTrack *track = context->GetTrack())
            {
                double lrVol = (Track_GetPeakInfo(track, 0) + Track_GetPeakInfo(track, 1)) / 2.0;
                
                if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                    context->ClearWidget();
                else
                    context->UpdateWidgetValue(volToNormalized(lrVol));
            }
            else
                context->ClearWidget();
        }
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(GetCurrentNormalizedValue(context)), false), NULL);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackOutputMeterMaxPeakLR : public TrackMeterAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackOutputMeterMaxPeakLR; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double lVol = Track_GetPeakInfo(track, 0);
            double rVol = Track_GetPeakInfo(track, 1);
            
            double lrVol =  lVol > rVol ? lVol : rVol;
            
            if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                context->ClearWidget();
            else
                context->UpdateWidgetValue(volToNormalized(lrVol));
        }
        else
            context->ClearWidget();
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackVolumeWithMeterMaxPeakLR : public TrackMeterAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::TrackVolumeWithMeterMaxPeakLR; }
    
    virtual double GetCurrentNormalizedValue(ActionContext *context) override
    {
        if (MediaTrack *track = context->GetTrack())
        {
            double vol, pan = 0.0;
            GetTrackUIVolPan(track, &vol, &pan);
            return volToNormalized(vol);
        }
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext *context) override
    {
        int stopState = GetPlayState();
        
        if (stopState == 0 || stopState == 2 || stopState == 6) // stopped or paused or paused whilst recording
        {
            if (context->GetTrack())
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
            else
                context->ClearWidget();
        }
        else
        {
            if (MediaTrack *track = context->GetTrack())
            {
                double lVol = Track_GetPeakInfo(track, 0);
                double rVol = Track_GetPeakInfo(track, 1);
                
                double lrVol =  lVol > rVol ? lVol : rVol;
                
                if (AnyTrackSolo(NULL) && ! GetMediaTrackInfo_Value(track, "I_SOLO"))
                    context->ClearWidget();
                else
                    context->UpdateWidgetValue(volToNormalized(lrVol));
            }
            else
                context->ClearWidget();
        }
    }
    
    virtual void Do(ActionContext *context, double value) override
    {
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }
    
    virtual void Touch(ActionContext *context, double value) override
    {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack *track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(GetCurrentNormalizedValue(context)), false), NULL);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FXGainReductionMeter : public TrackMeterAction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::FXGainReductionMeter; }

    virtual void RequestUpdate(ActionContext *context) override
    {
        char buffer[MEDBUF];
        
        if (MediaTrack *track = context->GetTrack())
        {
            if (TrackFX_GetNamedConfigParm(track, context->GetSlotIndex(), "GainReduction_dB", buffer, sizeof(buffer)))
                context->UpdateWidgetValue(-atof(buffer)/20.0);
            else
                context->UpdateWidgetValue(0.0);
        }
        else
            context->ClearWidget();
    }
};

#endif /* control_surface_Reaper_actions_h */
