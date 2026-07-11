// actions_send_receive.h
// Each template is parameterized on SendDirection and the correct base class (TrackSendAction vs TrackReceiveAction, or shared VolumeAction / PanAction / TrackDisplayAction) is chosen at compile time.
//  Original names are preserved as `using` aliases

#pragma once

enum class SendDirection { Send, Receive };

// For classes that want TrackSendAction vs TrackReceiveAction based on Dir:
template <SendDirection Dir>
using SendReceiveBase = std::conditional_t<Dir == SendDirection::Send, TrackSendAction, TrackReceiveAction>;

template <SendDirection Dir>
struct SendReceiveTraits;

template <>
struct SendReceiveTraits<SendDirection::Send> {
    static constexpr int Category = 0;
    static constexpr const char* TrackKey = "P_DESTTRACK";
};

template <>
struct SendReceiveTraits<SendDirection::Receive> {
    static constexpr int Category = -1;
    static constexpr const char* TrackKey = "P_SRCTRACK";
};

//! @action TrackSendVolume / TrackReceiveVolume
//!
//! @brief Controls the volume of a track send or receive.
//!
//! @zone_usage  Fader|    TrackSendVolume   or   Fader|    TrackReceiveVolume
//!
//! @feedback Continuous — sends normalized volume (0.0–1.0).
//!
//! @notes Touch for automation. Template instantiated for Send and Receive directions.
template <SendDirection Dir>
class TrackSendReceiveVolume : public VolumeAction
{
public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendVolume : ActionType::TrackReceiveVolume; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                GetTrackSendUIVolPan(track, GetSendEffectiveIndex(track, context), &vol, &pan);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            }
            return volToNormalized(vol);
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
            if constexpr (Dir == SendDirection::Send) {
                SetTrackSendUIVol(track, GetSendEffectiveIndex(track, context), normalizedToVol(value), 0);
            } else {
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), normalizedToVol(value), 0);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int idx = GetSendEffectiveIndex(track, context);
                GetTrackSendUIVolPan(track, idx, &vol, &pan);
                SetTrackSendUIVol(track, idx, vol, value == 0 ? 1 : 0);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, value == 0 ? 1 : 0);
            }
        }
    }
};

using TrackSendVolume = TrackSendReceiveVolume<SendDirection::Send>;
using TrackReceiveVolume = TrackSendReceiveVolume<SendDirection::Receive>;

//! @action TrackSendVolumeDB / TrackReceiveVolumeDB
//!
//! @brief Controls / displays send/receive volume in decibels.
//!
//! @zone_usage  WidgetName    TrackSendVolumeDB   or   WidgetName    TrackReceiveVolumeDB
//!
//! @feedback Continuous — sends volume as dB value.
template <SendDirection Dir>
class TrackSendReceiveVolumeDB : public VolumeAction
{
public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendVolumeDB : ActionType::TrackReceiveVolumeDB; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                GetTrackSendUIVolPan(track, context->GetParamIndex() + numHW, &vol, &pan);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            }
            return volToNormalized(vol);
        }
        return 0.0;
    }

    virtual double GetCurrentDBValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                GetTrackSendUIVolPan(track, context->GetParamIndex() + numHW, &vol, &pan);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            }
            return VAL2DB(vol);
        }
        return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentDBValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                SetTrackSendUIVol(track, context->GetParamIndex() + numHW, DB2VAL(value), 0);
            } else {
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), DB2VAL(value), 0);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                int idx = context->GetParamIndex() + numHW;
                GetTrackSendUIVolPan(track, idx, &vol, &pan);
                SetTrackSendUIVol(track, idx, vol, value == 0 ? 1 : 0);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), vol, value == 0 ? 1 : 0);
            }
        }
    }
};

using TrackSendVolumeDB = TrackSendReceiveVolumeDB<SendDirection::Send>;
using TrackReceiveVolumeDB = TrackSendReceiveVolumeDB<SendDirection::Receive>;

//! @action TrackSendPan / TrackReceivePan
//!
//! @brief Controls the pan of a track send or receive.
//!
//! @zone_usage  Rotary|    TrackSendPan   or   Rotary|    TrackReceivePan
//!
//! @feedback Continuous — sends normalized pan (0.0=left, 0.5=center, 1.0=right).
template <SendDirection Dir>
class TrackSendReceivePan : public PanAction
{
public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendPan : ActionType::TrackReceivePan; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                GetTrackSendUIVolPan(track, GetSendEffectiveIndex(track, context), &vol, &pan);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            }
            return panToNormalized(pan);
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
            if constexpr (Dir == SendDirection::Send) {
                SetTrackSendUIPan(track, GetSendEffectiveIndex(track, context), normalizedToPan(value), 0);
            } else {
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), normalizedToPan(value), 0);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int idx = GetSendEffectiveIndex(track, context);
                GetTrackSendUIVolPan(track, idx, &vol, &pan);
                SetTrackSendUIPan(track, idx, pan, value == 0 ? 1 : 0);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, value == 0 ? 1 : 0);
            }
        }
    }
};

using TrackSendPan = TrackSendReceivePan<SendDirection::Send>;
using TrackReceivePan = TrackSendReceivePan<SendDirection::Receive>;

//! @action TrackSendPanPercent / TrackReceivePanPercent
//!
//! @brief Controls send/receive pan as a percentage value (-100 to +100).
//!
//! @zone_usage  WidgetName    TrackSendPanPercent   or   WidgetName    TrackReceivePanPercent
//!
//! @feedback Continuous — sends pan as percentage.
template <SendDirection Dir>
class TrackSendReceivePanPercent : public PanAction
{
public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendPanPercent :  ActionType::TrackReceivePanPercent; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                GetTrackSendUIVolPan(track, context->GetParamIndex() + numHW, &vol, &pan);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
            }
            context->UpdateWidgetValue(pan * 100.0);
        } else {
            context->ClearWidget();
        }
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                SetTrackSendUIPan(track, context->GetParamIndex() + numHW, value / 100.0, 0);
            } else {
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), value / 100.0, 0);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                int idx = context->GetParamIndex() + numHW;
                GetTrackSendUIVolPan(track, idx, &vol, &pan);
                SetTrackSendUIPan(track, idx, pan, value == 0 ? 1 : 0);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetParamIndex(), &vol, &pan);
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), pan, value == 0 ? 1 : 0);
            }
        }
    }
};

using TrackSendPanPercent = TrackSendReceivePanPercent<SendDirection::Send>;
using TrackReceivePanPercent = TrackSendReceivePanPercent<SendDirection::Receive>;

//! @action TrackSendMute / TrackReceiveMute
//!
//! @brief Toggles mute on/off for a track send or receive.
//!
//! @zone_usage  WidgetName    TrackSendMute   or   WidgetName    TrackReceiveMute
//!
//! @feedback Toggle — 1.0 when muted, 0.0 when unmuted.
template <SendDirection Dir>
class TrackSendReceiveMute : public SendReceiveBase<Dir>
{
public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendMute : ActionType::TrackReceiveMute; }
    bool IgnoresRelease() const override { return true; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            bool mute = false;
            if constexpr (Dir == SendDirection::Send) {
                GetTrackSendUIMute(track, GetSendEffectiveIndex(track, context), &mute);
            } else {
                GetTrackReceiveUIMute(track, context->GetSlotIndex(), &mute);
            }
            return mute;
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
            if constexpr (Dir == SendDirection::Send) {
                ToggleTrackSendUIMute(track, GetSendEffectiveIndex(track, context));
            } else {
                bool isMuted = !GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MUTE");
                GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "B_MUTE", &isMuted);
            }
        }
    }
};

using TrackSendMute = TrackSendReceiveMute<SendDirection::Send>;
using TrackReceiveMute = TrackSendReceiveMute<SendDirection::Receive>;

//! @action TrackSendInvertPolarity / TrackReceiveInvertPolarity
//!
//! @brief Toggles phase/polarity inversion for a track send or receive.
//!
//! @zone_usage  WidgetName    TrackSendInvertPolarity   or   WidgetName    TrackReceiveInvertPolarity
//!
//! @feedback Toggle — 1.0 when inverted, 0.0 when normal.
template <SendDirection Dir>
class TrackSendReceiveInvertPolarity : public SendReceiveBase<Dir>
{
    static constexpr int category = SendReceiveTraits<Dir>::Category;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendInvertPolarity : ActionType::TrackReceiveInvertPolarity; }
    bool IgnoresRelease() const override { return true; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return GetTrackSendInfo_Value(track, category, context->GetSlotIndex(), "B_PHASE");
        return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            DAW::ToggleSendBoolParam(track, category, context->GetSlotIndex(), "B_PHASE");
    }
};

using TrackSendInvertPolarity = TrackSendReceiveInvertPolarity<SendDirection::Send>;
using TrackReceiveInvertPolarity = TrackSendReceiveInvertPolarity<SendDirection::Receive>;

//! @action TrackSendStereoMonoToggle / TrackReceiveStereoMonoToggle
//!
//! @brief Toggles stereo/mono mode for a track send or receive.
//!
//! @zone_usage  WidgetName    TrackSendStereoMonoToggle   or   WidgetName    TrackReceiveStereoMonoToggle
//!
//! @feedback Toggle — 1.0 when mono, 0.0 when stereo.
template <SendDirection Dir>
class TrackSendReceiveStereoMonoToggle : public SendReceiveBase<Dir>
{
    static constexpr int category = SendReceiveTraits<Dir>::Category;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendStereoMonoToggle : ActionType::TrackReceiveStereoMonoToggle; }
    bool IgnoresRelease() const override { return true; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return GetTrackSendInfo_Value(track, category, context->GetSlotIndex(), "B_MONO");
        return 0.0;
    }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetTrack())
            context->UpdateWidgetValue(GetCurrentNormalizedValue(context));
        else
            context->ClearWidget();
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            DAW::ToggleSendBoolParam(track, category, context->GetSlotIndex(), "B_MONO");
    }
};

using TrackSendStereoMonoToggle = TrackSendReceiveStereoMonoToggle<SendDirection::Send>;
using TrackReceiveStereoMonoToggle = TrackSendReceiveStereoMonoToggle<SendDirection::Receive>;

//! @action TrackSendPrePost / TrackReceivePrePost
//!
//! @brief Cycles the send/receive routing mode: Post-Pan → Pre-FX → Post-FX → Post-Pan.
//!
//! @zone_usage  WidgetName    TrackSendPrePost   or   WidgetName    TrackReceivePrePost
//!
//! @feedback None (clears color only).
//!
//! @see TrackSendPrePostDisplay, TrackReceivePrePostDisplay
template <SendDirection Dir>
class TrackSendReceivePrePost : public SendReceiveBase<Dir>
{
    static constexpr int category = SendReceiveTraits<Dir>::Category;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendPrePost : ActionType::TrackReceivePrePost; }
    bool IgnoresRelease() const override { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            int mode = (int) GetTrackSendInfo_Value(track, category, context->GetSlotIndex(), "I_SENDMODE");

            if (mode == 0)
                mode = 1; // switch to pre FX
            else if (mode == 1)
                mode = 3; // switch to post FX
            else
                mode = 0; // switch to post pan

            GetSetTrackSendInfo(track, category, context->GetSlotIndex(), "I_SENDMODE", &mode);
        }
    }
};

using TrackSendPrePost = TrackSendReceivePrePost<SendDirection::Send>;
using TrackReceivePrePost = TrackSendReceivePrePost<SendDirection::Receive>;

//! @action TrackSendNameDisplay / TrackReceiveNameDisplay
//!
//! @brief Displays the destination (send) or source (receive) track name.
//!
//! @zone_usage  DisplayWidget    TrackSendNameDisplay   or   DisplayWidget    TrackReceiveNameDisplay
//!
//! @feedback Text — sends the linked track's name string.
template <SendDirection Dir>
class TrackSendReceiveNameDisplay : public TrackDisplayAction
{
    static constexpr int category = SendReceiveTraits<Dir>::Category;
    static constexpr const char* TrackKey = SendReceiveTraits<Dir>::TrackKey;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendNameDisplay : ActionType::TrackReceiveNameDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, category, context->GetSlotIndex(), TrackKey, 0);
            if (linkedTrack) {
                const char* name = (const char*) GetSetMediaTrackInfo(linkedTrack, "P_NAME", NULL);
                context->UpdateWidgetValue(name ? name : "");
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

using TrackSendNameDisplay = TrackSendReceiveNameDisplay<SendDirection::Send>;
using TrackReceiveNameDisplay = TrackSendReceiveNameDisplay<SendDirection::Receive>;

//! @action TrackSendVolumeDisplay / TrackReceiveVolumeDisplay
//!
//! @brief Displays the send/receive volume in dB as formatted text.
//!
//! @zone_usage  DisplayWidget    TrackSendVolumeDisplay   or   DisplayWidget    TrackReceiveVolumeDisplay
//!
//! @feedback Text — sends formatted dB string (e.g. "  -6.02").
template <SendDirection Dir>
class TrackSendReceiveVolumeDisplay : public TrackDisplayAction
{
    static constexpr int category = SendReceiveTraits<Dir>::Category;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendVolumeDisplay : ActionType::TrackReceiveVolumeDisplay; }
    virtual bool IsVolumeRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, category, context->GetSlotIndex(), Dir == (SendDirection::Send) ? "P_DESTTRACK" : "P_SRCTRACK", 0);
            if (linkedTrack) {
                char buf[128];
                if constexpr (Dir == SendDirection::Send) {
                    double vol, pan = 0.0;
                    GetTrackSendUIVolPan(track, GetSendEffectiveIndex(track, context), &vol, &pan);
                    snprintf(buf, sizeof(buf), "%7.2lf", VAL2DB(vol));
                } else {
                    double vol = GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "D_VOL");
                    snprintf(buf, sizeof(buf), "%7.2lf", VAL2DB(vol));
                }
                context->UpdateWidgetValue(buf);
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

using TrackSendVolumeDisplay = TrackSendReceiveVolumeDisplay<SendDirection::Send>;
using TrackReceiveVolumeDisplay = TrackSendReceiveVolumeDisplay<SendDirection::Receive>;

//! @action TrackSendPanDisplay / TrackReceivePanDisplay
//!
//! @brief Displays the send/receive pan position as formatted text.
//!
//! @zone_usage  DisplayWidget    TrackSendPanDisplay   or   DisplayWidget    TrackReceivePanDisplay
//!
//! @feedback Text — sends formatted pan string (e.g. "<50", "C", "30>").
template <SendDirection Dir>
class TrackSendReceivePanDisplay : public TrackDisplayAction
{
    static constexpr int category = SendReceiveTraits<Dir>::Category;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendPanDisplay : ActionType::TrackReceivePanDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, category, context->GetSlotIndex(), Dir == (SendDirection::Send) ? "P_DESTTRACK" : "P_SRCTRACK", 0);
            if (linkedTrack) {
                double panVal = 0.0;
                if constexpr (Dir == SendDirection::Send) {
                    double vol = 0.0;
                    GetTrackSendUIVolPan(track, GetSendEffectiveIndex(track, context), &vol, &panVal);
                } else {
                    panVal = GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "D_PAN");
                }
                char tmp[MEDBUF];
                context->UpdateWidgetValue(context->GetPanValueString(panVal, "", tmp, sizeof(tmp)));
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

using TrackSendPanDisplay = TrackSendReceivePanDisplay<SendDirection::Send>;
using TrackReceivePanDisplay = TrackSendReceivePanDisplay<SendDirection::Receive>;

//! @action TrackSendStereoMonoDisplay / TrackReceiveStereoMonoDisplay
//!
//! @brief Displays the stereo/mono state of a send or receive.
//!
//! @zone_usage  DisplayWidget    TrackSendStereoMonoDisplay   or   DisplayWidget    TrackReceiveStereoMonoDisplay
//!
//! @feedback Text — "mono" or "stereo".
template <SendDirection Dir>
class TrackSendReceiveStereoMonoDisplay : public TrackDisplayAction
{
    static constexpr int category = SendReceiveTraits<Dir>::Category;
    static constexpr const char* TrackKey = SendReceiveTraits<Dir>::TrackKey;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendStereoMonoDisplay : ActionType::TrackReceiveStereoMonoDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, category, context->GetSlotIndex(), TrackKey, 0);
            if (linkedTrack) {
                context->UpdateWidgetValue((GetTrackSendInfo_Value(track, category, context->GetSlotIndex(), "B_MONO")) ? "mono" : "stereo");
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

using TrackSendStereoMonoDisplay = TrackSendReceiveStereoMonoDisplay<SendDirection::Send>;
using TrackReceiveStereoMonoDisplay = TrackSendReceiveStereoMonoDisplay<SendDirection::Receive>;

//! @action TrackSendPrePostDisplay / TrackReceivePrePostDisplay
//!
//! @brief Displays the send/receive routing mode as text.
//!
//! @zone_usage  DisplayWidget    TrackSendPrePostDisplay   or   DisplayWidget    TrackReceivePrePostDisplay
//!
//! @feedback Text — "PostPan", "PreFX", or "PostFX".
template <SendDirection Dir>
class TrackSendReceivePrePostDisplay : public TrackDisplayAction
{
    static constexpr int category = SendReceiveTraits<Dir>::Category;
    static constexpr const char* TrackKey = SendReceiveTraits<Dir>::TrackKey;

public:
    ActionType GetType() const override { return (Dir == SendDirection::Send) ? ActionType::TrackSendPrePostDisplay : ActionType::TrackReceivePrePostDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, category, context->GetSlotIndex(), TrackKey, 0);
            if (linkedTrack) {
                // I_SENDMODE: 0=post-fader, 1=pre-fx, 2=post-fx (deprecated), 3=post-fx
                double prePostVal = GetTrackSendInfo_Value(track, category, context->GetSlotIndex(), "I_SENDMODE");

                const char* str = "";
                if (prePostVal == 0) str = "PostPan";
                else if (prePostVal == 1) str = "PreFX";
                else if (prePostVal == 2 || prePostVal == 3) str = "PostFX";

                context->UpdateWidgetValue(str);
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

using TrackSendPrePostDisplay = TrackSendReceivePrePostDisplay<SendDirection::Send>;
using TrackReceivePrePostDisplay = TrackSendReceivePrePostDisplay<SendDirection::Receive>;
