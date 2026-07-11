// Meter and gain reduction actions.
#pragma once

//! @action TrackOutputMeter
//!
//! @brief Displays the track output meter level for a specific channel.
//!
//! @zone_usage  VUMeter|    TrackOutputMeter 0   (0=left, 1=right)
//!
//! @feedback Continuous — sends normalized peak level (0.0–1.0). Clears when track is not soloed and another track is.
//!
//! @params Int param: channel index (0=left, 1=right).
class TrackOutputMeter : public TrackMeterAction
{
public:
    ActionType GetType() const override { return ActionType::TrackOutputMeter; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            UpdateMeterValue(context, track, Track_GetPeakInfo(track, context->GetIntParam()));
        else
            context->ClearWidget();
    }
};

//! @action TrackOutputMeterAverageLR
//!
//! @brief Displays the average of left and right track output meter levels.
//!
//! @zone_usage  VUMeter|    TrackOutputMeterAverageLR
//!
//! @feedback Continuous — sends normalized average L+R peak level (0.0–1.0).
class TrackOutputMeterAverageLR : public TrackMeterAction
{
public:
    ActionType GetType() const override { return ActionType::TrackOutputMeterAverageLR; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double lrVol = (Track_GetPeakInfo(track, 0) + Track_GetPeakInfo(track, 1)) / 2.0;
            UpdateMeterValue(context, track, lrVol);
        } else
            context->ClearWidget();
    }
};

// Shared base for TrackVolumeWithMeterAverageLR and TrackVolumeWithMeterMaxPeakLR.
// Provides GetCurrentNormalizedValue (fader position), Do (set volume), and Touch.
class TrackVolumeWithMeterBase : public TrackMeterAction
{
public:
    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return volToNormalized(DAW::GetTrackVolume(track));
        return 0.0;
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack* track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(GetCurrentNormalizedValue(context)), false), NULL);
    }
};

//! @action TrackVolumeWithMeterAverageLR
//!
//! @brief Dual-purpose: shows volume fader position when stopped, average L+R meter when playing.
//!
//! @zone_usage  Fader|    TrackVolumeWithMeterAverageLR
//!
//! @feedback Continuous — normalized volume when stopped/paused, normalized average meter when playing.
//!
//! @notes Also supports Do/Touch for setting track volume. Combines fader position display with metering.
class TrackVolumeWithMeterAverageLR : public TrackVolumeWithMeterBase
{
public:
    ActionType GetType() const override { return ActionType::TrackVolumeWithMeterAverageLR; }

    virtual void RequestUpdate(ActionContext* context) override {
        int stopState = GetPlayState();
        if (stopState == PLAYSTATE_STOPPED || stopState == PLAYSTATE_PAUSED || stopState == PLAYSTATE_PAUSED_WHILE_RECORDING) {
            if (context->GetTrack())
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
            else
                context->ClearWidget();
        } else {
            if (MediaTrack* track = context->GetTrack()) {
                double lrVol = (Track_GetPeakInfo(track, 0) + Track_GetPeakInfo(track, 1)) / 2.0;
                UpdateMeterValue(context, track, lrVol);
            } else
                context->ClearWidget();
        }
    }
};

//! @action TrackOutputMeterMaxPeakLR
//!
//! @brief Displays the maximum of left and right track output meter levels.
//!
//! @zone_usage  VUMeter|    TrackOutputMeterMaxPeakLR
//!
//! @feedback Continuous — sends normalized max(L,R) peak level (0.0–1.0).
class TrackOutputMeterMaxPeakLR : public TrackMeterAction
{
public:
    ActionType GetType() const override { return ActionType::TrackOutputMeterMaxPeakLR; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double lrVol = std::max(Track_GetPeakInfo(track, 0), Track_GetPeakInfo(track, 1));
            UpdateMeterValue(context, track, lrVol);
        } else
            context->ClearWidget();
    }
};

//! @action TrackVolumeWithMeterMaxPeakLR
//!
//! @brief Dual-purpose: shows volume fader position when stopped, max(L,R) meter when playing.
//!
//! @zone_usage  Fader|    TrackVolumeWithMeterMaxPeakLR
//!
//! @feedback Continuous — normalized volume when stopped/paused, normalized max peak when playing.
//!
//! @notes Also supports Do/Touch for setting track volume. Combines fader position display with metering.
class TrackVolumeWithMeterMaxPeakLR : public TrackVolumeWithMeterBase
{
public:
    ActionType GetType() const override { return ActionType::TrackVolumeWithMeterMaxPeakLR; }

    virtual void RequestUpdate(ActionContext* context) override {
        int stopState = GetPlayState();
        if (stopState == PLAYSTATE_STOPPED || stopState == PLAYSTATE_PAUSED || stopState == PLAYSTATE_PAUSED_WHILE_RECORDING) {
            if (context->GetTrack())
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
            else
                context->ClearWidget();
        } else {
            if (MediaTrack* track = context->GetTrack()) {
                double lrVol = std::max(Track_GetPeakInfo(track, 0), Track_GetPeakInfo(track, 1));
                UpdateMeterValue(context, track, lrVol);
            } else
                context->ClearWidget();
        }
    }
};

//! @action FXGainReductionMeter
//!
//! @brief Displays the gain reduction meter for a compressor/limiter FX in a specific slot.
//!
//! @zone_usage  VUMeter|    FXGainReductionMeter
//!
//! @feedback Continuous — sends normalized gain reduction (0.0–1.0, derived from GainReduction_dB / -20.0).
//!
//! @notes Requires the FX to support the "GainReduction_dB" named config parameter.
class FXGainReductionMeter : public TrackMeterAction
{
public:
    ActionType GetType() const override { return ActionType::FXGainReductionMeter; }

    virtual void RequestUpdate(ActionContext* context) override {
        char buffer[MEDBUF];

        if (MediaTrack* track = context->GetTrack()) {
            if (TrackFX_GetNamedConfigParm(track, context->GetSlotIndex(), "GainReduction_dB", buffer, sizeof(buffer)))
                context->UpdateWidgetValue(-atof(buffer) / 20.0);
            else
                context->UpdateWidgetValue(0.0);
        } else
            context->ClearWidget();
    }
};
