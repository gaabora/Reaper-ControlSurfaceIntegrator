//
//  actions_meter.h
//  reaper_csurf_integrator
//
//  Phase 2.2 — Meter and gain reduction actions.
//
#pragma once

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
