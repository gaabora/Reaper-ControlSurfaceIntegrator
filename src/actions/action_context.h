#pragma once
//
//  action_context.h — ActionContext class
//
#include "../controls/preamble.h"
#include "../actions/action.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ActionContext
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    CSurfIntegrator *const csi_;
    Action *action_;
    Widget  *const widget_;
    Zone  *const zone_;

    int intParam_ = 0;
    
    string stringParam_;
    vector<string> sourceParams_;
    int paramIndex_ = 0;
    string fxParamDisplayName_;
    
    string actionTitle_ = "";
    int commandId_ = 0;
    const char* commandText_;
    bool needsReloadAfterRun_ = false;
    
    double rangeMinimum_ = 0.0;
    double rangeMaximum_ = 1.0;
    
    vector<double> steppedValues_;
    int steppedValuesIndex_= 0;
    
    double deltaValue_ = 0.0;
    vector<double> acceleratedDeltaValues_;
    vector<int> acceleratedTickValues_;
    int accumulatedIncTicks_ = 0;
    int accumulatedDecTicks_ = 0;
    
    bool isValueInverted_ = false;
    bool isFeedbackInverted_ = false;
    
    bool isDoublePress_ = false;
    DWORD doublePressStartTs_ = 0;

    int holdDelayMs_ = 0;
    int holdRepeatIntervalMs_ = 0;
    DWORD lastHoldRepeatTs_ = 0;
    DWORD lastHoldStartTs_ = 0;
    bool holdActive_= false;
    bool holdRepeatActive_ = false;
    double deferredValue_ = 0.0;
    
    int runCount_ = 1;
    
    bool supportsColor_ = false;
    vector<rgba_color> colorValues_;
    int currentColorIndex_ = 0;

    bool blinkSet_ = false;
    bool blinkActive_ = true;
    int blinkIntervalMs_ = 0;
    DWORD lastBlinkTs_ = 0;

    bool supportsTrackColor_ = false;

    bool provideFeedback_= true;

    char meterMode_[64] = "";

    string m_freeFormText;

    osd_data osdData_;
    
    PropertyList widgetProperties_;
        
    void UpdateTrackColor();
    void GetSteppedValues(Widget *widget, Action *action,  Zone *zone, int paramNumber, const vector<string> &params, const PropertyList &widgetProperties, double &deltaValue, vector<double> &acceleratedDeltaValues, double &rangeMinimum, double &rangeMaximum, vector<double> &steppedValues, vector<int> &acceleratedTickValues);
    void SetColor(const vector<string> &params, bool &supportsColor, bool &supportsTrackColor, vector<rgba_color> &colorValues);
    void GetColorValues(vector<rgba_color> &colorValues, const vector<string> &colors);
    void ProcessActionTitle(string fallbackName);
    void LogAction(double value);
    void LogMessage(const std::string& msg, DebugLevel debugLevel = DEBUG_LEVEL_WARNING);
    void ProcessOSD(double value, bool fromFeedback);
    bool OsdIgnoresButtonRelease();
    
    bool UpdateBlinkState() {
        DWORD now = GetTickCount();
        int blinkIntervalMs = GetBlinkInterval();
        if (now > lastBlinkTs_ + blinkIntervalMs) {
            blinkActive_ = !blinkActive_;
            lastBlinkTs_ = now;
        }
        return blinkActive_;
    }

    MediaTrack* track_;
    string trackName_;
    string fxParamDescription_;
    int lastTrackNum_ = -1, fxSlotNum_ = -1, fxParamNum_ = -1;
    double lastValue_ = 0.0;

public:
    static int constexpr INHERIT_VALUE = -1;
    static double constexpr BUTTON_RELEASE_MESSAGE_VALUE = 0.0;
    ActionContext(CSurfIntegrator *const csi, Action *action, Widget *widget, Zone *zone, int paramIndex, const vector<string> &params);

    virtual ~ActionContext() {}
    
    CSurfIntegrator *GetCSI() { return csi_; }
    
    Action *GetAction() { return action_; }
    Widget *GetWidget() { return widget_; }
    Zone *GetZone() { return zone_; }
    int GetSlotIndex();
    const char *GetName();

    int GetIntParam() { return intParam_; }
    int GetCommandId() { return commandId_; }
    const char* GetCommandText() { return commandText_; }
    bool NeedsReloadAfterRun() { return needsReloadAfterRun_; }
    
    const char *GetFXParamDisplayName() { return fxParamDisplayName_.c_str(); }
    
    MediaTrack *GetTrack();
    vector<MediaTrack *> GetSelectedTracks(bool includeMaster = false);
    
    void DoRangeBoundAction(double value);
    void DoSteppedValueAction(double value);
    void DoAcceleratedSteppedValueAction(int accelerationIndex, double value);
    void DoAcceleratedDeltaValueAction(int accelerationIndex, double value);
    
    Page *GetPage();
    ControlSurface *GetSurface();
    int GetParamIndex() { return paramIndex_; }
    void SetParamIndex(int paramIndex) { paramIndex_ = paramIndex; }
      
    PropertyList &GetWidgetProperties() { return widgetProperties_; }
    
    void SetIsValueInverted() { isValueInverted_ = true; }
    void SetIsFeedbackInverted() { isFeedbackInverted_ = true; }

    void SetBlinkInterval(int value) { blinkSet_ = true; blinkIntervalMs_ = value; }
    int GetBlinkInterval();

    void SetDoublePress() { isDoublePress_ = true; }
    bool IsDoublePress() { return isDoublePress_; }

    void SetHoldDelay(int value) { holdDelayMs_ = value; }
    int GetHoldDelay();

    void SetAction(Action *action) { action_ = action; RequestUpdate(); }
    void DoAction(double value);
    void PerformAction(double value);
    void DoRelativeAction(double value);
    void DoRelativeAction(int accelerationIndex, double value);
    
    void RequestUpdate();
    void RunDeferredActions();
    void ClearWidget();
    void UpdateWidgetValue(double value); // note: if passing the constant 0, must be 0.0 to avoid ambiguous type vs pointer
    void UpdateWidgetValue(const char *value);
    void ForceWidgetValue(const char *value);
    void UpdateJSFXWidgetSteppedValue(double value);
    void UpdateColorValue(double value);

    const char *GetStringParam() { return stringParam_.c_str(); }
    const   vector<double> &GetAcceleratedDeltaValues() { return acceleratedDeltaValues_; }
    void    SetAccelerationValues(const vector<double> &acceleratedDeltaValues) { acceleratedDeltaValues_ = acceleratedDeltaValues; }
    const   vector<int> &GetAcceleratedTickCounts() { return acceleratedTickValues_; }
    void    SetTickCounts(const vector<int> &acceleratedTickValues) { acceleratedTickValues_ = acceleratedTickValues; }
    int     GetNumberOfSteppedValues() { return (int)steppedValues_.size(); }
    const   vector<double> &GetSteppedValues() { return steppedValues_; }
    double  GetDeltaValue() { return deltaValue_; }
    void    SetDeltaValue(double deltaValue) { deltaValue_ = deltaValue; }
    double  GetRangeMinimum() const { return rangeMinimum_; }
    void    SetRangeMinimum(double rangeMinimum) { rangeMinimum_ = rangeMinimum; }
    double  GetRangeMaximum() const { return rangeMaximum_; }
    void    SetRangeMaximum(double rangeMaximum) { rangeMaximum_ = rangeMaximum; }
    bool    GetProvideFeedback() { return provideFeedback_; }
       
    void SetStringParam(const char *stringParam) 
    {
        stringParam_ = stringParam;
        RequestUpdate();
    }

    void SetStepValues(const vector<double> &steppedValues) 
    {
        steppedValues_ = steppedValues;
        if (steppedValuesIndex_ >= steppedValues.size())
            steppedValuesIndex_ = 0;
        RequestUpdate();
    }

    void DoTouch(double value)
    {
        action_->Touch(this, value);
    }

    void SetRange(const vector<double> &range)
    {
        if (range.size() != 2)
            return;
        
        rangeMinimum_ = range[0];
        rangeMaximum_ = range[1];
    }
        
    void SetSteppedValueIndex(double value)
    {
        int index = 0;
        double delta = 100000000.0;
        
        for (int i = 0; i < steppedValues_.size(); ++i)
            if (fabs(steppedValues_[i] - value) < delta)
            {
                delta = fabs(steppedValues_[i] - value);
                index = i;
            }
        
        steppedValuesIndex_ = index;
    }

    char *GetPanValueString(double panVal, const char *dualPan, char *buf, int bufsz) const
    {
        bool left = false;
        
        if (panVal < 0)
        {
            left = true;
            panVal = -panVal;
        }
        
        int panIntVal = int(panVal  *100.0);
        
        if (left)
        {
            const char *prefix;
            if (panIntVal == 100)
                prefix = "";
            else if (panIntVal < 100 && panIntVal > 9)
                prefix = " ";
            else
                prefix = "  ";
            
            snprintf(buf, bufsz, "<%s%d%s", prefix, panIntVal, dualPan ? dualPan : "");
        }
        else
        {
            const char *suffix;
            
            if (panIntVal == 100)
                suffix = "";
            else if (panIntVal < 100 && panIntVal > 9)
                suffix = " ";
            else
                suffix = "  ";

            snprintf(buf, bufsz, "  %s%d%s>", dualPan && *dualPan ? dualPan : " ", panIntVal, suffix);
        }
        
        if (panIntVal == 0)
        {
            if (!dualPan || !dualPan[0])
                lstrcpyn_safe(buf, "  <C>  ", bufsz);
            else if (IsSameString(dualPan, "L"))
                lstrcpyn_safe(buf, " L<C>  ", bufsz);
            else if (IsSameString(dualPan, "R"))
                lstrcpyn_safe(buf, " <C>R  ", bufsz);
        }

        return buf;
    }
    
    char *GetPanWidthValueString(double widthVal, char *buf, int bufsz) const
    {
        bool reversed = false;
        
        if (widthVal < 0)
        {
            reversed = true;
            widthVal = -widthVal;
        }
        
        int widthIntVal = int(widthVal  *100.0);
        
        if (widthIntVal == 0)
            lstrcpyn_safe(buf, "<Mono> ", bufsz);
        else
            snprintf(buf, bufsz, "%s %d", reversed ? "Rev" : "Wid", widthIntVal);
        
        return buf;
    }

    const char* GetFreeFormText() const { return m_freeFormText.c_str(); }
    void SetFreeFormText(const char* text) { m_freeFormText = (text ? text : ""); }

    const char* GetActionTitle() { return actionTitle_.c_str(); }

    int ClampValueWithWarning(int value, int min, int max);

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

    double GetTrackVolumeValue() {
        return DAW::GetTrackVolumeValue(track_);
    }
    void SetTrackVolumeValue(double value) {
        DAW::SetTrackVolumeValue(track_, value);
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
        if (fxSlotNum_ != fxSlotNum || fxParamNum_ != fxParamNum ) {
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
            if (lastTrackNum_ != trackNum || fxSlotNum_ != fxSlotNum || fxParamNum_ != fxParamNum ) {
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
             if (fxSlotNum_ != fxSlotNum || fxParamNum_ != fxParamNum ) {
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
