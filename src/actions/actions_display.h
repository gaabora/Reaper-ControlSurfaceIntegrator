//
//  actions_display.h
//  reaper_csurf_integrator
//
//  Phase 2.2 — Display-only actions (FX displays + track displays).
//
#pragma once

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
                    context->GetTrackNavigationManager()->GetIdFromTrack(destTrack),
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
                snprintf(tmp, sizeof(tmp), "Track %d%s%s", context->GetTrackNavigationManager()->GetIdFromTrack(srcTrack),
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
