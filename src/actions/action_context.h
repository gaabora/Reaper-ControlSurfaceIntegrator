#pragma once
//
//  action_context.h — ActionContext class
//
#include "../controls/preamble.h"
#include "../actions/action.h"
#include "action_timing.h"
#include "action_value.h"
#include "action_color.h"
#include "../controls/blink_state.h"

class ActionContext
{
public:
    enum class TransportStepAmount {
        Unknown = 0,
        Bar,
        Marker
    };

private:
    CSurfIntegrator* const csi_;
    Action* action_;
    Widget* const widget_;
    Zone* const zone_;

    int intParam_ = 0;

    string stringParam_;
    vector<string> sourceParams_;
    int paramIndex_ = 0;
    string fxParamDisplayName_;

    string actionTitle_ = "";
    int commandId_ = 0;
    const char* commandText_;
    bool needsReloadAfterRun_ = false;

    ActionTiming timing_; ///< Hold, repeat, and double-press timing state
    BlinkState blink_; ///< LED/display blink state
    ActionColorState color_; ///< Color and track-color state
    ActionValueState value_; ///< Range, stepped-values, and acceleration state

    int runCount_ = 1;

    bool provideFeedback_ = true;

    char meterMode_[64] = "";

    string m_freeFormText;
    TransportStepAmount transportStepAmount_ = TransportStepAmount::Unknown;

    osd_data osdData_;

    PropertyList widgetProperties_;

    void UpdateTrackColor();
    void GetSteppedValues(Widget* widget, Action* action, Zone* zone, int paramNumber, const vector<string>& params, const PropertyList& widgetProperties, double& deltaValue, vector<double>& acceleratedDeltaValues, double& rangeMinimum, double& rangeMaximum, vector<double>& steppedValues, vector<int>& acceleratedTickValues);
    void ProcessActionTitle(string fallbackName);
    void LogAction(double value);
    void LogMessage(const std::string& msg, DebugLevel debugLevel = DEBUG_LEVEL_WARNING);
    void ProcessOSD(double value, bool fromFeedback);
    bool OsdIgnoresButtonRelease();

    MediaTrack* track_;
    string trackName_;
    string fxParamDescription_;
    int lastTrackNum_ = -1, fxSlotNum_ = -1, fxParamNum_ = -1;
    double lastValue_ = 0.0;

public:
    static int constexpr INHERIT_VALUE = -1;
    static double constexpr BUTTON_RELEASE_MESSAGE_VALUE = 0.0; //FIXME: review usage, possibly improve for better support
    ActionContext(CSurfIntegrator* const csi, Action* action, Widget* widget, Zone* zone, int paramIndex, const vector<string>& params);

    virtual ~ActionContext() {}

    CSurfIntegrator* GetCSI() { return csi_; }

    Action* GetAction() { return action_; }
    Widget* GetWidget() { return widget_; }
    Zone* GetZone() { return zone_; }
    int GetSlotIndex();
    const char* GetName();

    int GetIntParam() { return intParam_; }
    int GetCommandId() { return commandId_; }
    const char* GetCommandText() { return commandText_; }
    bool NeedsReloadAfterRun() { return needsReloadAfterRun_; }
    const vector<string>& GetSourceParams() const { return sourceParams_; }

    const char* GetFXParamDisplayName() { return fxParamDisplayName_.c_str(); }

    MediaTrack* GetTrack();
    vector<MediaTrack*> GetSelectedTracks(bool includeMaster = false);

    void DoRangeBoundAction(double value);
    void DoSteppedValueAction(double value);
    void DoAcceleratedSteppedValueAction(int accelerationIndex, double value);
    void DoAcceleratedDeltaValueAction(int accelerationIndex, double value);

    IPageContext* GetPage();
    ControlSurface* GetSurface();
    // Convenience shorthand: equivalent to GetPage()->GetTrackNavigationManager().
    // Reduces the repetitive 2-hop chain in action implementations.
    TrackNavigationManager* GetTrackNavigationManager();
    int GetParamIndex() { return paramIndex_; }
    void SetParamIndex(int paramIndex) { paramIndex_ = paramIndex; }

    PropertyList& GetWidgetProperties() { return widgetProperties_; }

    void SetIsValueInverted() { value_.isValueInverted = true; }
    void SetIsFeedbackInverted() { value_.isFeedbackInverted = true; }
    bool GetIsValueInverted() const { return this->value_.isValueInverted; }
    bool GetIsFeedbackInverted() const { return this->value_.isFeedbackInverted; }

    void SetBlinkInterval(int value) {
        blink_.blinkSet = true;
        blink_.blinkIntervalMs = value;
    }
    int GetBlinkInterval();

    void SetDoublePress() { timing_.isDoublePress = true; }
    bool IsDoublePress() { return timing_.isDoublePress; }

    void SetHoldDelay(int value) { timing_.holdDelayMs = value; }
    int GetHoldDelay();

    void SetAction(Action* action) {
        action_ = action;
        RequestUpdate();
    }
    void DoAction(double value);
    void PerformAction(double value);
    void DoRelativeAction(double value);
    void DoRelativeAction(int accelerationIndex, double value);

    void RequestUpdate();
    void RunDeferredActions();
    void ClearWidget();
    void UpdateWidgetValue(double value); // note: if passing the constant 0, must be 0.0 to avoid ambiguous type vs pointer
    void UpdateWidgetValue(const char* value);
    void ForceWidgetValue(const char* value);
    void UpdateJSFXWidgetSteppedValue(double value);
    void UpdateColorValue(double value);

    const char* GetStringParam() { return stringParam_.c_str(); }
    TransportStepAmount GetTransportStepAmount() const { return transportStepAmount_; }
    const   vector<double>& GetAcceleratedDeltaValues() { return value_.acceleratedDeltaValues; }
    void    SetAccelerationValues(const vector<double>& acceleratedDeltaValues) { value_.acceleratedDeltaValues = acceleratedDeltaValues; }
    const   vector<int>& GetAcceleratedTickCounts() { return value_.acceleratedTickValues; }
    void    SetTickCounts(const vector<int>& acceleratedTickValues) { value_.acceleratedTickValues = acceleratedTickValues; }
    int     GetNumberOfSteppedValues() { return (int) value_.steppedValues.size(); }
    const   vector<double>& GetSteppedValues() { return value_.steppedValues; }
    double  GetDeltaValue() { return value_.deltaValue; }
    void    SetDeltaValue(double deltaValue) { value_.deltaValue = deltaValue; }
    double  GetRangeMinimum() const { return value_.rangeMinimum; }
    void    SetRangeMinimum(double rangeMinimum) { value_.rangeMinimum = rangeMinimum; }
    double  GetRangeMaximum() const { return value_.rangeMaximum; }
    void    SetRangeMaximum(double rangeMaximum) { value_.rangeMaximum = rangeMaximum; }
    bool    GetProvideFeedback() { return provideFeedback_; }

    void SetStringParam(const char* stringParam) {
        stringParam_ = stringParam;
        RequestUpdate();
    }

    void SetStepValues(const vector<double>& steppedValues) {
        value_.steppedValues = steppedValues;
        if (value_.steppedValuesIndex >= (int) steppedValues.size())
            value_.steppedValuesIndex = 0;
        RequestUpdate();
    }

    void DoTouch(double value) { action_->Touch(this, value); }

    void SetRange(const vector<double>& range) {
        if (range.size() != 2)
            return;
        value_.rangeMinimum = range[0];
        value_.rangeMaximum = range[1];
    }

    void SetSteppedValueIndex(double value) {
        int index = 0;
        double delta = 100000000.0;

        for (int i = 0; i < (int) value_.steppedValues.size(); ++i)
            if (fabs(value_.steppedValues[i] - value) < delta) {
                delta = fabs(value_.steppedValues[i] - value);
                index = i;
            }

        value_.steppedValuesIndex = index;
    }

    char* GetPanValueString(double panVal, const char* dualPan, char* buf, int bufsz) const {
        bool left = false;

        if (panVal < 0) {
            left = true;
            panVal = -panVal;
        }

        int panIntVal = int(panVal * 100.0);

        if (left) {
            const char* prefix;
            if (panIntVal == 100)
                prefix = "";
            else if (panIntVal < 100 && panIntVal > 9)
                prefix = " ";
            else
                prefix = "  ";

            snprintf(buf, bufsz, "<%s%d%s", prefix, panIntVal, dualPan ? dualPan : "");
        } else {
            const char* suffix;

            if (panIntVal == 100)
                suffix = "";
            else if (panIntVal < 100 && panIntVal > 9)
                suffix = " ";
            else
                suffix = "  ";

            snprintf(buf, bufsz, "  %s%d%s>", dualPan && *dualPan ? dualPan : " ", panIntVal, suffix);
        }

        if (panIntVal == 0) {
            if (!dualPan || !dualPan[0])
                lstrcpyn_safe(buf, "  <C>  ", bufsz);
            else if (IsSameString(dualPan, "L"))
                lstrcpyn_safe(buf, " L<C>  ", bufsz);
            else if (IsSameString(dualPan, "R"))
                lstrcpyn_safe(buf, " <C>R  ", bufsz);
        }

        return buf;
    }

    char* GetPanWidthValueString(double widthVal, char* buf, int bufsz) const {
        bool reversed = false;

        if (widthVal < 0) {
            reversed = true;
            widthVal = -widthVal;
        }

        int widthIntVal = int(widthVal * 100.0);

        if (widthIntVal == 0)
            lstrcpyn_safe(buf, "<Mono> ", bufsz);
        else
            snprintf(buf, bufsz, "%s %d", reversed ? "Rev" : "Wid", widthIntVal);

        return buf;
    }

    const char* GetFreeFormText() const { return m_freeFormText.c_str(); }
    void SetFreeFormText(const char* text) { m_freeFormText = (text ? text : ""); }

    const char* GetActionTitle() { return actionTitle_.c_str(); }

    int ClampValueWithWarning(int value, int min_val, int max_val);

    void SetLastValue(double value) {
        lastValue_ = value;
    }

    bool IsSameAsLastValue(double value) {
        return DAW::CompareFaderValues(lastValue_, value);
    }

    double GetTrackFxParamValue() {
        return DAW::GetTrackFxParamValue(track_, fxSlotNum_, fxParamNum_);
    }
    void SetTrackFxParamValue(double value) {
        DAW::SetTrackFxParamValue(track_, fxSlotNum_, fxParamNum_, value);
    }
    void EndTrackFxParamEdit() {
        TrackFX_EndParamEdit(track_, fxSlotNum_, fxParamNum_);
    }
    string GetTrackFxParamFormattedValue() {
        return DAW::GetFxParamValue(track_, fxSlotNum_, fxParamNum_);
    }

    double GetTrackVolumeNormalized() {
        return volToNormalized(DAW::GetTrackVolume(track_));
    }
    void SetTrackVolumeNormalized(double value) {
        DAW::SetTrackVolume(track_, normalizedToVol(value));
    }

    bool CheckCurrentTrackContext() {
        MediaTrack* currentTrack = this->GetTrack();
        if (!currentTrack)
            return ClearCurrentContext();
        if (track_ != currentTrack) {
            track_ = currentTrack;
            trackName_ = DAW::GetTrackName(track_);
        }
        return true;
    }

    bool CheckCurrentFxContext() {
        MediaTrack* currentTrack = this->GetTrack();
        if (!currentTrack)
            return ClearCurrentContext();
        if (track_ != currentTrack) {
            track_ = currentTrack;
            trackName_ = DAW::GetTrackName(track_);
        }
        int fxSlotNum = this->GetSlotIndex();
        int fxParamNum = this->GetParamIndex();
        if (fxSlotNum_ != fxSlotNum || fxParamNum_ != fxParamNum) {
            fxSlotNum_ = fxSlotNum;
            fxParamNum_ = fxParamNum;
            fxParamDescription_ = DAW::GetFxParamDescription(track_, fxSlotNum_, fxParamNum_);
        }
        return true;
    }
    bool CheckLastTouchedFxContext() {
        int trackNum;
        int fxSlotNum;
        int fxParamNum;
        if (GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum)) {
            if (lastTrackNum_ != trackNum || fxSlotNum_ != fxSlotNum || fxParamNum_ != fxParamNum) {
                track_ = DAW::GetTrack(trackNum);
                if (track_) {
                    trackName_ = DAW::GetTrackName(track_);
                    lastTrackNum_ = trackNum;
                    fxSlotNum_ = fxSlotNum;
                    fxParamNum_ = fxParamNum;
                    fxParamDescription_ = DAW::GetFxParamDescription(track_, fxSlotNum_, fxParamNum_);
                } else
                    return ClearCurrentContext();
            }
            return true;
        } else
            return ClearCurrentContext();
    }
    bool CheckCurrentTcpFxContext() {
        MediaTrack* currentTrack = this->GetTrack();
        if (!currentTrack)
            this->ClearCurrentContext();
        if (track_ != currentTrack) {
            track_ = currentTrack;
            trackName_ = DAW::GetTrackName(track_);
        }
        int index = this->GetIntParam();
        if (CountTCPFXParms(NULL, track_) <= index)
            return ClearCurrentContext();
        int fxSlotNum;
        int fxParamNum;
        if (GetTCPFXParm(NULL, track_, index, &fxSlotNum, &fxParamNum)) {
            if (fxSlotNum_ != fxSlotNum || fxParamNum_ != fxParamNum) {
                fxSlotNum_ = fxSlotNum;
                fxParamNum_ = fxParamNum;
                fxParamDescription_ = DAW::GetFxParamDescription(track_, fxSlotNum_, fxParamNum_);
            }
        } else
            return ClearCurrentContext();
        return true;
    }

    bool ClearCurrentContext() {
        track_ = nullptr;
        lastTrackNum_ = -1;
        fxSlotNum_ = -1;
        fxParamNum_ = -1;
        return false;
    }
};
