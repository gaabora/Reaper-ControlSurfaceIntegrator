// track volume/pan/mute/solo/select/arm actions.
#pragma once

//! @action TrackVolume
//!
//! @brief Controls the volume fader for the mapped track.
//!
//! @zone_usage  Fader|    TrackVolume
//!
//! @feedback Continuous — sends normalized volume (0.0–1.0) to a motorized fader or knob.
//!
//! @notes Supports touch for automation write. Inherits full get/set/touch from VolumeAction base.
//!
//! @see SoftTakeover7BitTrackVolume, SoftTakeover14BitTrackVolume, TrackVolumeDB
class TrackVolume : public VolumeAction
{
public:
    ActionType GetType() const override { return ActionType::TrackVolume; }
};

//! @action SoftTakeover7BitTrackVolume
//!
//! @brief Volume fader with 7-bit soft-takeover to prevent value jumps when physical fader doesn't match track volume.
//!
//! @zone_usage  Fader|    SoftTakeover7BitTrackVolume
//!
//! @feedback Continuous — sends normalized volume (0.0–1.0).
//!
//! @notes Only applies the change when fader is within 2.5% of the actual track volume. For 7-bit (0–127) MIDI controllers.
class SoftTakeover7BitTrackVolume : public VolumeAction
{
public:
    ActionType GetType() const override { return ActionType::SoftTakeover7BitTrackVolume; }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            double trackVolume = volToNormalized(DAW::GetTrackVolume(track));
            if (fabs(value - trackVolume) < 0.025) // GAW -- Magic number -- ne touche pas
                CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
    }
};

//! @action SoftTakeover14BitTrackVolume
//!
//! @brief Volume fader with 14-bit soft-takeover. Tighter tolerance than 7-bit variant.
//!
//! @zone_usage  Fader|    SoftTakeover14BitTrackVolume
//!
//! @feedback Continuous — sends normalized volume (0.0–1.0).
//!
//! @notes Uses TRACK_VOLUME_TOLERANCE (0.0025) for matching. For 14-bit (0–16383) MIDI controllers.
class SoftTakeover14BitTrackVolume : public VolumeAction
{
public:
    ActionType GetType() const override { return ActionType::SoftTakeover14BitTrackVolume; }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            double trackVolume = volToNormalized(DAW::GetTrackVolume(track));
            if (fabs(value - trackVolume) < TRACK_VOLUME_TOLERANCE)
                CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
    }
};

//! @action TrackVolumeDB
//!
//! @brief Track volume in decibels (dB) for OSC displays or dB-native widgets.
//!
//! @zone_usage  WidgetName    TrackVolumeDB
//!
//! @feedback Continuous — sends volume as dB value (e.g. -inf to +12.0).
//!
//! @notes Touch writes automation in dB. Different from TrackVolume which uses normalized 0–1 range.
class TrackVolumeDB : public VolumeAction
{
public:
    ActionType GetType() const override { return ActionType::TrackVolumeDB; }

    virtual double GetCurrentDBValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            return VAL2DB(DAW::GetTrackVolume(track));
        } else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(GetCurrentDBValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, DB2VAL(value), false), NULL);
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsVolumeTouched(value != 0);
        if (MediaTrack* track = context->GetTrack())
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, DB2VAL(GetCurrentDBValue(context)), false), NULL);
    }
};

//! @action TrackPan
//!
//! @brief Controls the track pan knob (standard or balance mode, not dual pan).
//!
//! @zone_usage  Rotary|    TrackPan
//!
//! @feedback Continuous — sends normalized pan (0.0=left, 0.5=center, 1.0=right).
//!
//! @notes Skipped when pan mode is dual pan. Touch writes automation.
//!
//! @see TrackPanL, TrackPanR, TrackPanAutoLeft, TrackPanAutoRight
class TrackPan : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPan; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                return panToNormalized(DAW::GetTrackPan(track));
        }
        return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(value), false), NULL);
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false), NULL);
        }
    }
};

//! @action TrackPanPercent
//!
//! @brief Track pan as a percentage value (-100 to +100) for OSC displays.
//!
//! @zone_usage  WidgetName    TrackPanPercent
//!
//! @feedback Continuous — sends pan as percentage (-100.0 to +100.0). Skipped in dual pan mode.
class TrackPanPercent : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanPercent; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(DAW::GetTrackPan(track) * 100.0);
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, value / 100.0, false), NULL);
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, DAW::GetTrackPan(track), false), NULL);
        }
    }
};

//! @action TrackPanWidth
//!
//! @brief Controls the track stereo width. Only active when NOT in dual pan mode.
//!
//! @zone_usage  Rotary|    TrackPanWidth
//!
//! @feedback Continuous — sends normalized width (0.0–1.0).
//!
//! @see TrackPanWidthPercent
class TrackPanWidth : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanWidth; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_WIDTH"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_OnWidthChange(track, normalizedToPan(value), false);
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
        if (MediaTrack* track = context->GetTrack())
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_OnWidthChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false);
    }
};

//! @action TrackPanWidthPercent
//!
//! @brief Track stereo width as percentage (-100 to +100) for OSC displays. Only active when NOT in dual pan mode.
//!
//! @zone_usage  WidgetName    TrackPanWidthPercent
//!
//! @feedback Continuous — sends width as percentage.
class TrackPanWidthPercent : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanWidthPercent; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_WIDTH") * 100.0);
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_OnWidthChange(track, value / 100.0, false);
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) != DAW::PANMODE_DUAL)
                CSurf_OnWidthChange(track, GetMediaTrackInfo_Value(track, "D_WIDTH"), false);
        }
    }
};

//! @action TrackPanL
//!
//! @brief Controls the left channel pan in dual pan mode. Only active when pan mode IS dual pan.
//!
//! @zone_usage  Rotary|    TrackPanL
//!
//! @feedback Continuous — sends normalized left pan (0.0–1.0).
//!
//! @see TrackPanR, TrackPanAutoLeft
class TrackPanL : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanL; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANL", &pan);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
    }
};

//! @action TrackPanLPercent
//!
//! @brief Left channel pan as percentage in dual pan mode. Only active when pan mode IS dual pan.
//!
//! @zone_usage  WidgetName    TrackPanLPercent
//!
//! @feedback Continuous — sends left pan as percentage.
class TrackPanLPercent : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanLPercent; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_DUALPANL") * 100.0);
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double panFromPercent = value / 100.0;
                GetSetMediaTrackInfo(track, "D_DUALPANL", &panFromPercent);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double panL = GetMediaTrackInfo_Value(track, "D_DUALPANL");
                GetSetMediaTrackInfo(track, "D_DUALPANL", &panL);
            }
        }
    }
};

//! @action TrackPanR
//!
//! @brief Controls the right channel pan in dual pan mode. Only active when pan mode IS dual pan.
//!
//! @zone_usage  Rotary|    TrackPanR
//!
//! @feedback Continuous — sends normalized right pan (0.0–1.0).
//!
//! @see TrackPanL, TrackPanAutoRight
class TrackPanR : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanR; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR"));
        else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANR", &pan);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
    }
};

//! @action TrackPanRPercent
//!
//! @brief Right channel pan as percentage in dual pan mode. Only active when pan mode IS dual pan.
//!
//! @zone_usage  WidgetName    TrackPanRPercent
//!
//! @feedback Continuous — sends right pan as percentage.
class TrackPanRPercent : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanRPercent; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(GetMediaTrackInfo_Value(track, "D_DUALPANR") * 100.0);
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double panFromPercent = value / 100.0;
                GetSetMediaTrackInfo(track, "D_DUALPANR", &panFromPercent);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double panL = GetMediaTrackInfo_Value(track, "D_DUALPANR");
                GetSetMediaTrackInfo(track, "D_DUALPANR", &panL);
            }
        }
    }
};

//! @action TrackPanAutoLeft
//!
//! @brief Auto-switching left pan: uses pan in standard mode, left dual-pan channel in dual pan mode.
//!
//! @zone_usage  Rotary|    TrackPanAutoLeft
//!
//! @feedback Continuous — sends normalized pan value. Automatically picks the correct parameter based on pan mode.
//!
//! @see TrackPanAutoRight, TrackPan, TrackPanL
class TrackPanAutoLeft : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanAutoLeft; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL"));
            else
                return panToNormalized(DAW::GetTrackPan(track));
        } else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANL")));
            else
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANL", &pan);
            } else
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(value), false), NULL);
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->GetZone()->GetNavigator()->SetIsPanLeftTouched(value != 0);
            else {
                context->GetZone()->GetNavigator()->SetIsPanTouched(value != 0);
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false), NULL);
            }
        }
    }
};

//! @action TrackPanAutoRight
//!
//! @brief Auto-switching right control: uses width in standard mode, right dual-pan channel in dual pan mode.
//!
//! @zone_usage  Rotary|    TrackPanAutoRight
//!
//! @feedback Continuous — sends normalized value. Automatically picks width or right pan based on pan mode.
//!
//! @see TrackPanAutoLeft, TrackPanWidth, TrackPanR
class TrackPanAutoRight : public PanAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanAutoRight; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR"));
            else
                return panToNormalized(GetMediaTrackInfo_Value(track, "D_WIDTH"));
        } else
            return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->UpdateWidgetValue(panToNormalized(GetMediaTrackInfo_Value(track, "D_DUALPANR")));
            else
                context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        } else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double pan = normalizedToPan(value);
                GetSetMediaTrackInfo(track, "D_DUALPANR", &pan);
            } else
                CSurf_OnWidthChange(track, normalizedToPan(value), false);
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetPanMode(track) == DAW::PANMODE_DUAL)
                context->GetZone()->GetNavigator()->SetIsPanRightTouched(value != 0);
            else {
                context->GetZone()->GetNavigator()->SetIsPanWidthTouched(value != 0);
                CSurf_OnWidthChange(track, normalizedToPan(GetCurrentNormalizedValue(context)), false);
            }
        }
    }
};

//! @action TrackRecordArm
//!
//! @brief Toggles record arm on/off for selected tracks. Excludes master track.
//!
//! @zone_usage  WidgetName    TrackRecordArm
//!
//! @feedback Toggle — 1.0 when armed, 0.0 when disarmed.
class TrackRecordArm : public PressOnlyTrackAction
{
protected:
    bool IncludeMasterTrack() const override { return false; }

public:
    ActionType GetType() const override { return ActionType::TrackRecordArm; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackRecordArm);
    }

    virtual void Do(ActionContext* context, double value) override {
        ToggleTrackState(context, DAW::GetTrackRecordArm, DAW::SetTrackRecordArm);
    }
};

//! @action TrackRecordArmDisplay
//!
//! @brief Displays "ARM" when the track is record-armed, empty string when not.
//!
//! @zone_usage  DisplayWidget    TrackRecordArmDisplay
//!
//! @feedback Text — "ARM" or "".
class TrackRecordArmDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackRecordArmDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double state = GetMediaTrackInfo_Value(track, "I_RECARM");

            if (state > 0.5)
                context->UpdateWidgetValue("ARM");
            else
                context->UpdateWidgetValue("");
        } else
            context->ClearWidget();
    }
};

//! @action TrackMute
//!
//! @brief Toggles mute on/off for selected tracks.
//!
//! @zone_usage  WidgetName    TrackMute
//!
//! @feedback Toggle — 1.0 when muted, 0.0 when unmuted.
class TrackMute : public PressOnlyTrackAction
{
public:
    ActionType GetType() const override { return ActionType::TrackMute; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackMute);
    }

    virtual void Do(ActionContext* context, double value) override {
        ToggleTrackState(context, DAW::GetTrackMute, DAW::SetTrackMute);
    }
};

//! @action TrackEffectsBypass
//!
//! @brief Toggles the FX bypass state for selected tracks.
//!
//! @zone_usage  WidgetName    TrackEffectsBypass
//!
//! @feedback Toggle — 1.0 when FX are bypassed, 0.0 when active.
class TrackEffectsBypass : public PressOnlyTrackAction
{
public:
    ActionType GetType() const override { return ActionType::TrackEffectsBypass; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackEffectsBypass);
    }

    virtual void Do(ActionContext* context, double value) override {
        ToggleTrackState(context, DAW::GetTrackEffectsBypass, DAW::SetTrackEffectsBypass);
    }
};

//! @action TrackSolo
//!
//! @brief Toggles solo on/off for selected tracks.
//!
//! @zone_usage  WidgetName    TrackSolo
//!
//! @feedback Toggle — 1.0 when soloed, 0.0 when not soloed.
class TrackSolo : public PressOnlyTrackAction
{
public:
    ActionType GetType() const override { return ActionType::TrackSolo; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackSolo);
    }

    virtual void Do(ActionContext* context, double value) override {
        ToggleTrackState(context, DAW::GetTrackSolo, DAW::SetTrackSolo);
    }
};

//! @action TrackInvertPolarity
//!
//! @brief Toggles phase/polarity inversion for selected tracks.
//!
//! @zone_usage  WidgetName    TrackInvertPolarity
//!
//! @feedback Toggle — 1.0 when inverted, 0.0 when normal.
class TrackInvertPolarity : public PressOnlyTrackAction //TODO: rename TrackInvertPhase
{
public:
    ActionType GetType() const override { return ActionType::TrackInvertPolarity; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return GetTrackBoolStateNormalized(context, DAW::GetTrackInvertPhase);
    }

    virtual void Do(ActionContext* context, double value) override {
        ToggleTrackState(context, DAW::GetTrackInvertPhase, DAW::SetTrackInvertPhase);
    }
};

// Shared base for TrackSelect, TrackUniqueSelect, and TrackRangeSelect:
// all three report the same "I_SELECTED" state and share identical RequestUpdate logic.
class TrackSelectBase : public PressOnlyAction
{
public:
    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return GetMediaTrackInfo_Value(track, "I_SELECTED");
        return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }
};

//! @action TrackSelect
//!
//! @brief Toggles track selection on/off. Allows multi-selection (adds/removes from current selection).
//!
//! @zone_usage  WidgetName    TrackSelect
//!
//! @feedback Toggle — 1.0 when selected, 0.0 when not.
//!
//! @see TrackUniqueSelect, TrackRangeSelect
class TrackSelect : public TrackSelectBase
{
public:
    ActionType GetType() const override { return ActionType::TrackSelect; }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            CSurf_SetSurfaceSelected(track, CSurf_OnSelectedChange(track, !GetMediaTrackInfo_Value(track, "I_SELECTED")), NULL);
            context->GetPage()->OnTrackSelectionBySurface(track);
        }
    }
};

//! @action TrackUniqueSelect
//!
//! @brief Selects this track exclusively — deselects all others first.
//!
//! @zone_usage  WidgetName    TrackUniqueSelect
//!
//! @feedback Toggle — 1.0 when selected, 0.0 when not.
//!
//! @see TrackSelect, TrackRangeSelect
class TrackUniqueSelect : public TrackSelectBase
{
public:
    ActionType GetType() const override { return ActionType::TrackUniqueSelect; }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            SetOnlyTrackSelected(track);
            context->GetPage()->OnTrackSelectionBySurface(track);
        }
    }
};

//! @action TrackRangeSelect
//!
//! @brief Selects a contiguous range of tracks from the currently selected track to this track.
//!
//! @zone_usage  WidgetName    TrackRangeSelect
//!
//! @feedback Toggle — 1.0 when selected, 0.0 when not.
//!
//! @notes Only works when exactly one track is currently selected. Selects all visible tracks between the two.
//!
//! @see TrackSelect, TrackUniqueSelect
class TrackRangeSelect : public TrackSelectBase
{
public:
    ActionType GetType() const override { return ActionType::TrackRangeSelect; }

    virtual void Do(ActionContext* context, double value) override {
        int currentlySelectedCount = 0;
        int selectedTrackIndex = 0;
        int trackIndex = 0;

        for (int i = 1; i <= context->GetTrackNavigationManager()->GetNumTracks(); ++i) {
            MediaTrack* currentTrack = context->GetTrackNavigationManager()->GetTrackFromId(i);

            if (currentTrack == NULL) continue;

            if (currentTrack == context->GetTrack())
                trackIndex = i;

            if (GetMediaTrackInfo_Value(currentTrack, "I_SELECTED")) {
                selectedTrackIndex = i;
                currentlySelectedCount++;
            }
        }

        if (currentlySelectedCount != 1) return;

        int lowerBound = trackIndex < selectedTrackIndex ? trackIndex : selectedTrackIndex;
        int upperBound = trackIndex > selectedTrackIndex ? trackIndex : selectedTrackIndex;

        for (int i = lowerBound; i <= upperBound; ++i) {
            MediaTrack* currentTrack = context->GetTrackNavigationManager()->GetTrackFromId(i);

            if (currentTrack == NULL) continue;

            if (context->GetTrackNavigationManager()->GetIsTrackVisible(currentTrack))
                CSurf_SetSurfaceSelected(currentTrack, CSurf_OnSelectedChange(currentTrack, 1), NULL);
        }

        MediaTrack* lowestTrack = context->GetTrackNavigationManager()->GetTrackFromId(lowerBound);

        if (lowestTrack != NULL)
            context->GetPage()->OnTrackSelectionBySurface(lowestTrack);
    }
};
