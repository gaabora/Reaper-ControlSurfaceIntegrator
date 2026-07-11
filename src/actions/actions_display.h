// actions_display.h

#pragma once

//! @action FXNameDisplay
//!
//! @brief Displays the zone alias/name for the current FX mapping.
//!
//! @zone_usage  DisplayWidget    FXNameDisplay
//!
//! @feedback Text — sends the zone alias name string.
class FXNameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::FXNameDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            context->UpdateWidgetValue(context->GetName());
        else
            context->ClearWidget();
    }
};

//! @action FXMenuNameDisplay
//!
//! @brief Displays the FX plugin name for the current slot (for FX menu navigation).
//!
//! @zone_usage  DisplayWidget    FXMenuNameDisplay
//!
//! @feedback Text — sends the FX plugin alias/name. Clears if slot is empty.
class FXMenuNameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::FXMenuNameDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            char alias[MEDBUF];
            alias[0] = 0;
            if (context->GetSlotIndex() < TrackFX_GetCount(track)) {
                context->GetSurface()->GetZoneManager()->GetName(track, context->GetSlotIndex(), alias, sizeof(alias));
                context->UpdateWidgetValue(alias);
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

//! @action SpeakFXMenuName
//!
//! @brief Speaks the FX plugin name for the current slot via OSARA screen reader.
//!
//! @zone_usage  WidgetName    SpeakFXMenuName
//!
//! @feedback None.
class SpeakFXMenuName : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SpeakFXMenuName; }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            char alias[MEDBUF];
            alias[0] = 0;
            if (context->GetSlotIndex() < TrackFX_GetCount(track)) {
                context->GetSurface()->GetZoneManager()->GetName(track, context->GetSlotIndex(), alias, sizeof(alias));
                context->GetCSI()->Speak(alias);
            }
        }
    }
};

//! @action FXParamNameDisplay
//!
//! @brief Displays the name of an FX parameter for the current slot and param index.
//!
//! @zone_usage  DisplayWidget    FXParamNameDisplay
//!
//! @feedback Text — sends the FX parameter name string. Uses custom display name if set, otherwise queries Reaper.
class FXParamNameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::FXParamNameDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (context->GetFXParamDisplayName()[0])
                context->UpdateWidgetValue(context->GetFXParamDisplayName());
            else {
                char tmp[MEDBUF];
                TrackFX_GetParamName(track, context->GetSlotIndex(), context->GetParamIndex(), tmp, sizeof(tmp));
                context->UpdateWidgetValue(tmp);
            }
        } else
            context->ClearWidget();
    }
};

//! @action TCPFXParamNameDisplay
//!
//! @brief Displays the name of a TCP-exposed FX parameter.
//!
//! @zone_usage  DisplayWidget    TCPFXParamNameDisplay 0
//!
//! @feedback Text — sends the parameter name. Clears if the TCP param index doesn't exist.
//!
//! @params Int param: TCP FX parameter index.
class TCPFXParamNameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TCPFXParamNameDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            int index = context->GetIntParam();

            if (CountTCPFXParms(NULL, track) > index) {
                int fxIndex = 0;
                int paramIndex = 0;

                char tmp[MEDBUF];
                if (GetTCPFXParm(NULL, track, index, &fxIndex, &paramIndex))
                    context->UpdateWidgetValue(context->GetCSI()->GetTCPFXParamName(track, fxIndex, paramIndex, tmp, sizeof(tmp)));
                else
                    context->ClearWidget();
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

//! @action FXParamValueDisplay
//!
//! @brief Displays the formatted value of an FX parameter for the current slot and param index.
//!
//! @zone_usage  DisplayWidget    FXParamValueDisplay
//!
//! @feedback Text — sends the formatted parameter value string (e.g. "3.5 dB", "100%").
class FXParamValueDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::FXParamValueDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            char fxParamValue[128];
            TrackFX_GetFormattedParamValue(track, context->GetSlotIndex(), context->GetParamIndex(), fxParamValue, sizeof(fxParamValue));
            context->UpdateWidgetValue(fxParamValue);
        } else
            context->ClearWidget();
    }
};

//! @action TCPFXParamValueDisplay
//!
//! @brief Displays the formatted value of a TCP-exposed FX parameter.
//!
//! @zone_usage  DisplayWidget    TCPFXParamValueDisplay 0
//!
//! @feedback Text — sends the formatted parameter value. Clears if TCP param index doesn't exist.
//!
//! @params Int param: TCP FX parameter index.
class TCPFXParamValueDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TCPFXParamValueDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            int index = context->GetIntParam();

            if (CountTCPFXParms(NULL, track) > index) {
                int fxIndex = 0;
                int paramIndex = 0;

                if (GetTCPFXParm(NULL, track, index, &fxIndex, &paramIndex)) {
                    char fxParamValue[128];
                    TrackFX_GetFormattedParamValue(track, fxIndex, paramIndex, fxParamValue, sizeof(fxParamValue));
                    context->UpdateWidgetValue(fxParamValue);
                } else
                    context->ClearWidget();
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

//! @action LastTouchedFXParamNameDisplay
//!
//! @brief Displays the name of the globally last-touched FX parameter.
//!
//! @zone_usage  DisplayWidget    LastTouchedFXParamNameDisplay
//!
//! @feedback Text — sends the parameter name string. Clears if no FX param was recently touched.
class LastTouchedFXParamNameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::LastTouchedFXParamNameDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        WithLastTouchedFX(context, [&](MediaTrack* track, int fxSlotNum, int fxParamNum) {
            char fxParamValue[MEDBUF];
            TrackFX_GetParamName(track, fxSlotNum, fxParamNum, fxParamValue, sizeof(fxParamValue));
            context->UpdateWidgetValue(fxParamValue);
        });
    }
};

//! @action LastTouchedFXParamValueDisplay
//!
//! @brief Displays the formatted value of the globally last-touched FX parameter.
//!
//! @zone_usage  DisplayWidget    LastTouchedFXParamValueDisplay
//!
//! @feedback Text — sends the formatted value string. Clears if no FX param was recently touched.
class LastTouchedFXParamValueDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::LastTouchedFXParamValueDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        WithLastTouchedFX(context, [&](MediaTrack* track, int fxSlotNum, int fxParamNum) {
            char fxParamValue[MEDBUF];
            TrackFX_GetFormattedParamValue(track, fxSlotNum, fxParamNum, fxParamValue, sizeof(fxParamValue));
            context->UpdateWidgetValue(fxParamValue);
        });
    }
};

//! @action SpeakTrackSendDestination
//!
//! @brief Speaks the send destination track name and number via OSARA screen reader.
//!
//! @zone_usage  WidgetName    SpeakTrackSendDestination
//!
//! @feedback None.
class SpeakTrackSendDestination : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SpeakTrackSendDestination; }

    virtual void Do(ActionContext* context, double value) override {
        SpeakLinkedTrack(context, 0, "P_DESTTRACK", "No Send Track");
    }
};

//! @action SpeakTrackReceiveSource
//!
//! @brief Speaks the receive source track name and number via OSARA screen reader.
//!
//! @zone_usage  WidgetName    SpeakTrackReceiveSource
//!
//! @feedback None.
class SpeakTrackReceiveSource : public PressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SpeakTrackReceiveSource; }

    virtual void Do(ActionContext* context, double value) override {
        SpeakLinkedTrack(context, -1, "P_SRCTRACK", "No Receive Track");
    }
};

//! @action FixedTextDisplay
//!
//! @brief Displays a fixed text string on a display widget (never changes).
//!
//! @zone_usage  DisplayWidget    FixedTextDisplay "My Label"
//!
//! @feedback Text — always sends the string param value.
//!
//! @params String param: the text to display.
class FixedTextDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::FixedTextDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetStringParam());
    }
};

//! @action FixedRGBColorDisplay
//!
//! @brief Sets a fixed RGB color on a color-capable widget (e.g. scribble strip background).
//!
//! @zone_usage  DisplayWidget    FixedRGBColorDisplay
//!
//! @feedback Value — sends 0.0 (color is set via the widget's color properties, not the value).
//!
//! @properties OnColor, OffColor, etc.
class FixedRGBColorDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::FixedRGBColorDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(0.0);
    }
};

//! @action TrackNameDisplay
//!
//! @brief Displays the track name for the mapped track.
//!
//! @zone_usage  DisplayWidget    TrackNameDisplay
//!
//! @feedback Text — sends the track name string.
class TrackNameDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackNameDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            char buf[MEDBUF];

            GetTrackName(track, buf, sizeof(buf));

            context->UpdateWidgetValue(buf);
        } else
            context->ClearWidget();
    }
};

//! @action TrackNumberDisplay
//!
//! @brief Displays the track number (1-based index) for the mapped track.
//!
//! @zone_usage  DisplayWidget    TrackNumberDisplay
//!
//! @feedback Text — sends the track number as a string.
class TrackNumberDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackNumberDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double index = GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER");
            char idx[128];
            snprintf(idx, sizeof(idx), "%d", (int) index);

            context->UpdateWidgetValue(idx);
        } else
            context->ClearWidget();
    }
};

//! @action TrackRecordInputDisplay
//!
//! @brief Displays the current record input configuration for the mapped track.
//!
//! @zone_usage  DisplayWidget    TrackRecordInputDisplay
//!
//! @feedback Text — sends input description (e.g. "None", "MD All", "MD 5", "1+2", "Mno 3", "Multi").
class TrackRecordInputDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackRecordInputDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            /*
            // I_RECINPUT : int  *: record input, <0=no input.
            if 4096 set, input is MIDI and low 5 bits represent channel (0=all, 1-16=only chan), next 6 bits represent physical input (63=all, 62=VKB).
            If 4096 is not set, low 10 bits (0..1023) are input start channel (ReaRoute/Loopback start at 512).
            If 2048 is set, input is multichannel input (using track channel count).
            If 1024 is set, input is stereo input, otherwise input is mono.
            */

            char inputDisplay[MEDBUF];

            int input = (int) GetMediaTrackInfo_Value(track, "I_RECINPUT");

            if (input < 0)
                lstrcpyn_safe(inputDisplay, "None", sizeof(inputDisplay));
            else if (input & 4096) {
                int channel = input & 0x1f;

                if (channel == 0)
                    lstrcpyn_safe(inputDisplay, "MD All", sizeof(inputDisplay));
                else
                    snprintf(inputDisplay, sizeof(inputDisplay), "MD %d", channel);
            } else if (input & 2048) {
                lstrcpyn_safe(inputDisplay, "Multi", sizeof(inputDisplay));
            } else if (input & 1024) {
                int channels = input ^ 1024;

                snprintf(inputDisplay, sizeof(inputDisplay), "%d+%d", channels + 1, channels + 2);
            } else {
                snprintf(inputDisplay, sizeof(inputDisplay), "Mno %d", input + 1);
            }

            context->UpdateWidgetValue(inputDisplay);
        } else
            context->ClearWidget();
    }
};

//! @action TrackInvertPolarityDisplay
//!
//! @brief Displays the polarity/phase state for the mapped track.
//!
//! @zone_usage  DisplayWidget    TrackInvertPolarityDisplay
//!
//! @feedback Text — "Normal" or "Invert".
class TrackInvertPolarityDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackInvertPolarityDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            if (GetMediaTrackInfo_Value(track, "B_PHASE") == 0)
                context->UpdateWidgetValue("Normal");
            else
                context->UpdateWidgetValue("Invert");
        } else
            context->ClearWidget();
    }
};

//! @action TrackVolumeDisplay
//!
//! @brief Displays the track volume in dB for the mapped track.
//!
//! @zone_usage  DisplayWidget    TrackVolumeDisplay
//!
//! @feedback Text — sends volume as formatted dB string (e.g. "  -6.02").
class TrackVolumeDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackVolumeDisplay; }
    virtual bool IsVolumeRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            char trackVolume[128];
            snprintf(trackVolume, sizeof(trackVolume), "%7.2lf", VAL2DB(DAW::GetTrackVolume(track)));
            context->UpdateWidgetValue(trackVolume);
        } else
            context->ClearWidget();
    }
};

//! @action TrackPanDisplay
//!
//! @brief Displays the track pan position as a formatted string.
//!
//! @zone_usage  DisplayWidget    TrackPanDisplay
//!
//! @feedback Text — sends pan value string (e.g. "<50", "C", "30>").
class TrackPanDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(DAW::GetTrackPan(track), "", tmp, sizeof(tmp)));
        } else
            context->ClearWidget();
    }
};

//! @action TrackPanWidthDisplay
//!
//! @brief Displays the track stereo width as a formatted string.
//!
//! @zone_usage  DisplayWidget    TrackPanWidthDisplay
//!
//! @feedback Text — sends width value string.
class TrackPanWidthDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanWidthDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double widthVal = GetMediaTrackInfo_Value(track, "D_WIDTH");

            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanWidthValueString(widthVal, tmp, sizeof(tmp)));
        } else
            context->ClearWidget();
    }
};

//! @action TrackPanLeftDisplay
//!
//! @brief Displays the left channel dual-pan position as a formatted string.
//!
//! @zone_usage  DisplayWidget    TrackPanLeftDisplay
//!
//! @feedback Text — sends pan L value string (e.g. "L<50", "LC", "L30>").
class TrackPanLeftDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanLeftDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANL");

            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(panVal, "L", tmp, sizeof(tmp)));
        } else
            context->ClearWidget();
    }
};

//! @action TrackPanRightDisplay
//!
//! @brief Displays the right channel dual-pan position as a formatted string.
//!
//! @zone_usage  DisplayWidget    TrackPanRightDisplay
//!
//! @feedback Text — sends pan R value string.
class TrackPanRightDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanRightDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANR");

            char tmp[MEDBUF];
            context->UpdateWidgetValue(context->GetPanValueString(panVal, "R", tmp, sizeof(tmp)));
        } else
            context->ClearWidget();
    }
};

//! @action TrackPanAutoLeftDisplay
//!
//! @brief Displays the auto-left pan value: standard pan in normal mode, left dual-pan in dual pan mode.
//!
//! @zone_usage  DisplayWidget    TrackPanAutoLeftDisplay
//!
//! @feedback Text — sends formatted pan or left-pan string depending on pan mode.
//!
//! @see TrackPanAutoRightDisplay
class TrackPanAutoLeftDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanAutoLeftDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            char tmp[MEDBUF];
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANL");
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "L", tmp, sizeof(tmp)));
            } else {
                context->UpdateWidgetValue(context->GetPanValueString(DAW::GetTrackPan(track), "", tmp, sizeof(tmp)));
            }
        } else
            context->ClearWidget();
    }
};

//! @action TrackPanAutoRightDisplay
//!
//! @brief Displays the auto-right value: width in normal mode, right dual-pan in dual pan mode.
//!
//! @zone_usage  DisplayWidget    TrackPanAutoRightDisplay
//!
//! @feedback Text — sends formatted width or right-pan string depending on pan mode.
//!
//! @see TrackPanAutoLeftDisplay
class TrackPanAutoRightDisplay : public TrackDisplayAction
{
public:
    ActionType GetType() const override { return ActionType::TrackPanAutoRightDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            char tmp[MEDBUF];
            if (GetPanMode(track) == DAW::PANMODE_DUAL) {
                double panVal = GetMediaTrackInfo_Value(track, "D_DUALPANR");
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "R", tmp, sizeof(tmp)));
            } else {
                double widthVal = GetMediaTrackInfo_Value(track, "D_WIDTH");
                context->UpdateWidgetValue(context->GetPanWidthValueString(widthVal, tmp, sizeof(tmp)));
            }
        } else
            context->ClearWidget();
    }
};
