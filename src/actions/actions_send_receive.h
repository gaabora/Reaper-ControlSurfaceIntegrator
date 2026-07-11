//
//  actions_send_receive.h

//
//  Phase 2.1 — Unified send/receive action templates.
//
//  Before: 13 pairs of near-identical Send/Receive classes (~1010 lines).
//  After:  13 template classes + using aliases            (~490 lines).
//
//  Each template is parameterized on SendDirection and the correct base
//  class (TrackSendAction vs TrackReceiveAction, or shared VolumeAction /
//  PanAction / TrackDisplayAction) is chosen at compile time.
//
//  Original names are preserved as `using` aliases so the rest of the
//  codebase needs zero changes.
//

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Direction tag and base-class helper
// ─────────────────────────────────────────────────────────────────────────────
enum class SendDirection { Send, Receive };

// For classes that want TrackSendAction vs TrackReceiveAction based on Dir:
template <SendDirection Dir>
using SendReceiveBase = std::conditional_t<Dir == SendDirection::Send, TrackSendAction, TrackReceiveAction>;

// ─────────────────────────────────────────────────────────────────────────────
// 1.  Volume  (VolumeAction base — inherits IsVolumeRelated)
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceiveVolume : public VolumeAction
{
public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendVolume : ActionType::TrackReceiveVolume; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHW, &vol, &pan);
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
                int numHW = GetTrackNumSends(track, 1);
                SetTrackSendUIVol(track, context->GetSlotIndex() + numHW, normalizedToVol(value), 0);
            } else {
                SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), normalizedToVol(value), 0);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                int idx = context->GetSlotIndex() + numHW;
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

// ─────────────────────────────────────────────────────────────────────────────
// 2.  VolumeDB  (VolumeAction base)
//     Both Send and Receive now use GetSlotIndex() for reads on the Receive
//     path, consistent with the non-DB TrackReceiveVolume.
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceiveVolumeDB : public VolumeAction
{
public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendVolumeDB : ActionType::TrackReceiveVolumeDB; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                GetTrackSendUIVolPan(track, context->GetParamIndex() + numHW, &vol, &pan);
            } else {
                GetTrackReceiveUIVolPan(track, context->GetSlotIndex(), &vol, &pan);
            }
            context->UpdateWidgetValue(VAL2DB(vol));
        } else {
            context->ClearWidget();
        }
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

// ─────────────────────────────────────────────────────────────────────────────
// 3.  Pan  (PanAction base — inherits IsPanRelated)
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceivePan : public PanAction
{
public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendPan : ActionType::TrackReceivePan; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHW, &vol, &pan);
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
                int numHW = GetTrackNumSends(track, 1);
                SetTrackSendUIPan(track, context->GetSlotIndex() + numHW, normalizedToPan(value), 0);
            } else {
                SetTrackSendUIPan(track, -(context->GetSlotIndex() + 1), normalizedToPan(value), 0);
            }
        }
    }

    virtual void Touch(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            double vol, pan = 0.0;
            if constexpr (Dir == SendDirection::Send) {
                int numHW = GetTrackNumSends(track, 1);
                int idx = context->GetSlotIndex() + numHW;
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

// ─────────────────────────────────────────────────────────────────────────────
// 4.  PanPercent  (PanAction base)
// ─────────────────────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────────────────────
// 5.  Mute  (TrackSendAction / TrackReceiveAction base — fixes §2.3 issue)
// ─────────────────────────────────────────────────────────────────────────────
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
                int numHW = GetTrackNumSends(track, 1);
                GetTrackSendUIMute(track, context->GetSlotIndex() + numHW, &mute);
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
                ToggleTrackSendUIMute(track, context->GetSlotIndex() + GetTrackNumSends(track, 1));
            } else {
                bool isMuted = !GetTrackSendInfo_Value(track, -1, context->GetSlotIndex(), "B_MUTE");
                GetSetTrackSendInfo(track, -1, context->GetSlotIndex(), "B_MUTE", &isMuted);
            }
        }
    }
};

using TrackSendMute = TrackSendReceiveMute<SendDirection::Send>;
using TrackReceiveMute = TrackSendReceiveMute<SendDirection::Receive>;

// ─────────────────────────────────────────────────────────────────────────────
// 6.  InvertPolarity  (fixes §2.3: was Action, now TrackSend/ReceiveAction)
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceiveInvertPolarity : public SendReceiveBase<Dir>
{
    static constexpr int Cat = Dir == (SendDirection::Send) ? 0 : -1;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendInvertPolarity : ActionType::TrackReceiveInvertPolarity; }
    bool IgnoresRelease() const override { return true; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return GetTrackSendInfo_Value(track, Cat, context->GetSlotIndex(), "B_PHASE");
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
            bool reversed = !GetTrackSendInfo_Value(track, Cat, context->GetSlotIndex(), "B_PHASE");
            GetSetTrackSendInfo(track, Cat, context->GetSlotIndex(), "B_PHASE", &reversed);
        }
    }
};

using TrackSendInvertPolarity = TrackSendReceiveInvertPolarity<SendDirection::Send>;
using TrackReceiveInvertPolarity = TrackSendReceiveInvertPolarity<SendDirection::Receive>;

// ─────────────────────────────────────────────────────────────────────────────
// 7.  StereoMonoToggle  (fixes §2.3)
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceiveStereoMonoToggle : public SendReceiveBase<Dir>
{
    static constexpr int Cat = Dir == (SendDirection::Send) ? 0 : -1;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendStereoMonoToggle : ActionType::TrackReceiveStereoMonoToggle; }
    bool IgnoresRelease() const override { return true; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack())
            return GetTrackSendInfo_Value(track, Cat, context->GetSlotIndex(), "B_MONO");
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
            bool mono = !GetTrackSendInfo_Value(track, Cat, context->GetSlotIndex(), "B_MONO");
            GetSetTrackSendInfo(track, Cat, context->GetSlotIndex(), "B_MONO", &mono);
        }
    }
};

using TrackSendStereoMonoToggle = TrackSendReceiveStereoMonoToggle<SendDirection::Send>;
using TrackReceiveStereoMonoToggle = TrackSendReceiveStereoMonoToggle<SendDirection::Receive>;

// ─────────────────────────────────────────────────────────────────────────────
// 8.  PrePost  (fixes §2.3)
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceivePrePost : public SendReceiveBase<Dir>
{
    static constexpr int Cat = Dir == (SendDirection::Send) ? 0 : -1;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendPrePost : ActionType::TrackReceivePrePost; }
    bool IgnoresRelease() const override { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    virtual void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack()) {
            int mode = (int) GetTrackSendInfo_Value(track, Cat, context->GetSlotIndex(), "I_SENDMODE");

            if (mode == 0)
                mode = 1; // switch to pre FX
            else if (mode == 1)
                mode = 3; // switch to post FX
            else
                mode = 0; // switch to post pan

            GetSetTrackSendInfo(track, Cat, context->GetSlotIndex(), "I_SENDMODE", &mode);
        }
    }
};

using TrackSendPrePost = TrackSendReceivePrePost<SendDirection::Send>;
using TrackReceivePrePost = TrackSendReceivePrePost<SendDirection::Receive>;

// ─────────────────────────────────────────────────────────────────────────────
// 9.  NameDisplay  (TrackDisplayAction base)
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceiveNameDisplay : public TrackDisplayAction
{
    static constexpr int Cat = Dir == (SendDirection::Send) ? 0 : -1;
    static constexpr const char* TrackKey = (Dir == SendDirection::Send) ? "P_DESTTRACK" : "P_SRCTRACK";

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendNameDisplay : ActionType::TrackReceiveNameDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, Cat, context->GetSlotIndex(), TrackKey, 0);
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

// ─────────────────────────────────────────────────────────────────────────────
// 10.  VolumeDisplay  (TrackDisplayAction base)
//      Send:    GetTrackSendUIVolPan + numHardwareSends → VAL2DB
//      Receive: GetTrackSendInfo_Value "D_VOL" → VAL2DB
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceiveVolumeDisplay : public TrackDisplayAction
{
    static constexpr int Cat = Dir == (SendDirection::Send) ? 0 : -1;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendVolumeDisplay : ActionType::TrackReceiveVolumeDisplay; }
    virtual bool IsVolumeRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, Cat, context->GetSlotIndex(), Dir == (SendDirection::Send) ? "P_DESTTRACK" : "P_SRCTRACK", 0);
            if (linkedTrack) {
                char buf[128];
                if constexpr (Dir == SendDirection::Send) {
                    int numHW = GetTrackNumSends(track, 1);
                    double vol, pan = 0.0;
                    GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHW, &vol, &pan);
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

// ─────────────────────────────────────────────────────────────────────────────
// 11.  PanDisplay  (TrackDisplayAction base)
//      Send:    GetTrackSendUIVolPan + numHardwareSends → GetPanValueString
//      Receive: GetTrackSendInfo_Value "D_PAN"         → GetPanValueString
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceivePanDisplay : public TrackDisplayAction
{
    static constexpr int Cat = Dir == (SendDirection::Send) ? 0 : -1;

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendPanDisplay : ActionType::TrackReceivePanDisplay; }
    virtual bool IsPanRelated() { return true; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, Cat, context->GetSlotIndex(), Dir == (SendDirection::Send) ? "P_DESTTRACK" : "P_SRCTRACK", 0);
            if (linkedTrack) {
                double panVal = 0.0;
                if constexpr (Dir == SendDirection::Send) {
                    int numHW = GetTrackNumSends(track, 1);
                    double vol = 0.0;
                    GetTrackSendUIVolPan(track, context->GetSlotIndex() + numHW, &vol, &panVal);
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

// ─────────────────────────────────────────────────────────────────────────────
// 12.  StereoMonoDisplay  (TrackDisplayAction — fixes §2.3: was Action)
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceiveStereoMonoDisplay : public TrackDisplayAction
{
    static constexpr int Cat = Dir == (SendDirection::Send) ? 0 : -1;
    static constexpr const char* TrackKey = (Dir == SendDirection::Send) ? "P_DESTTRACK" : "P_SRCTRACK";

public:
    ActionType GetType() const override { return Dir == (SendDirection::Send) ? ActionType::TrackSendStereoMonoDisplay : ActionType::TrackReceiveStereoMonoDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, Cat, context->GetSlotIndex(), TrackKey, 0);
            if (linkedTrack) {
                context->UpdateWidgetValue((GetTrackSendInfo_Value(track, Cat, context->GetSlotIndex(), "B_MONO")) ? "mono" : "stereo");
            } else
                context->ClearWidget();
        } else
            context->ClearWidget();
    }
};

using TrackSendStereoMonoDisplay = TrackSendReceiveStereoMonoDisplay<SendDirection::Send>;
using TrackReceiveStereoMonoDisplay = TrackSendReceiveStereoMonoDisplay<SendDirection::Receive>;

// ─────────────────────────────────────────────────────────────────────────────
// 13.  PrePostDisplay  (TrackDisplayAction base)
// ─────────────────────────────────────────────────────────────────────────────
template <SendDirection Dir>
class TrackSendReceivePrePostDisplay : public TrackDisplayAction
{
    static constexpr int Cat = Dir == (SendDirection::Send) ? 0 : -1;
    static constexpr const char* TrackKey = (Dir == SendDirection::Send) ? "P_DESTTRACK" : "P_SRCTRACK";

public:
    ActionType GetType() const override { return (Dir == SendDirection::Send) ? ActionType::TrackSendPrePostDisplay : ActionType::TrackReceivePrePostDisplay; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (MediaTrack* track = context->GetTrack()) {
            MediaTrack* linkedTrack = (MediaTrack*) GetSetTrackSendInfo(track, Cat, context->GetSlotIndex(), TrackKey, 0);
            if (linkedTrack) {
                // I_SENDMODE: 0=post-fader, 1=pre-fx, 2=post-fx (deprecated), 3=post-fx
                double prePostVal = GetTrackSendInfo_Value(track, Cat, context->GetSlotIndex(), "I_SENDMODE");

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
