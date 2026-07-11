#pragma once
//
//  widget.h — Widget class
//
#include "preamble.h"
#include "feedback.h"

class Widget
{
protected:
    CSurfIntegrator* const csi_;
    ControlSurface* const surface_;
    string const name_;
    vector<unique_ptr<FeedbackProcessor>> feedbackProcessors_; // owns the objects
    int channelNumber_ = 0;
    int lastIncomingMessageTime_ = GetTickCount() - 30000;
    double lastIncomingDelta_ = 0.0;
    double lastFeedbackValue_ = 0.0;
    rgba_color lastFeedbackColor_{};

    double stepSize_ = 0.0;
    vector<double> accelerationValues_;

    bool hasBeenUsedByUpdate_ = false;

    bool isVirtual_ = false;
    bool isTwoState_ = false;
    bool isModifier_ = false;
    bool hasDoublePressActions_ = false;
    bool hasHoldActions_ = false;
    bool holdFired_ = false;
    string oskWidgetClass_;
    bool oskPressInput_ = false;
    bool oskRelativeInput_ = false;
    bool oskAbsoluteInput_ = false;
    bool oskTouchInput_ = false;
    bool oskValueFeedback_ = false;
    bool oskToggleFeedback_ = false;
    bool oskColorFeedback_ = false;
    bool oskTextFeedback_ = false;
    bool oskMeterFeedback_ = false;

public:
    // all Widgets are owned by their ControlSurface!
    Widget(CSurfIntegrator* const csi, ControlSurface* surface, const char* name)
        : csi_(csi), surface_(surface), name_(name) {
        int suffixNumber = ExtractSuffixNumber(name);
        if (suffixNumber > 0) channelNumber_ = suffixNumber;

        isVirtual_ = find(VIRTUAL_TRIGGERS.begin(), VIRTUAL_TRIGGERS.end(), name) != VIRTUAL_TRIGGERS.end();
    }

    ~Widget() {
        feedbackProcessors_.clear();
    }

    vector<unique_ptr<FeedbackProcessor>>& GetFeedbackProcessors() { return feedbackProcessors_; }

    static inline const vector<string> VIRTUAL_TRIGGERS = {
        "OnTrackSelection",
        "OnPageEnter",
        "OnPageLeave",
        "OnInitialization",
        "OnPlayStart",
        "OnPlayStop",
        "OnRecordStart",
        "OnRecordStop",
        "OnZoneActivation",
        "OnZoneDeactivation"
    };

    void ClearHasBeenUsedByUpdate() { hasBeenUsedByUpdate_ = false; }
    void SetHasBeenUsedByUpdate() { hasBeenUsedByUpdate_ = true; }
    bool GetHasBeenUsedByUpdate() { return hasBeenUsedByUpdate_; }

    const char* GetName() { return name_.c_str(); }
    ControlSurface* GetSurface() { return surface_; }
    ZoneManager* GetZoneManager();
    bool IsVirtual() { return isVirtual_; }

    int GetChannelNumber() { return channelNumber_; }

    void SetStepSize(double stepSize) { stepSize_ = stepSize; }
    double GetStepSize() { return stepSize_; }
    bool GetIsTwoState() { return isTwoState_; }
    void SetIsTwoState() { isTwoState_ = true; }
    bool IsModifier() const { return isModifier_; }
    void SetIsModifier() { isModifier_ = true; }

    void SetOskWidgetClass(const string& widgetClass) { oskWidgetClass_ = widgetClass; }
    const string& GetOskWidgetClass() const { return oskWidgetClass_; }
    void MarkOskPressInput() { oskPressInput_ = true; }
    void MarkOskRelativeInput() { oskRelativeInput_ = true; }
    void MarkOskAbsoluteInput() { oskAbsoluteInput_ = true; }
    void MarkOskTouchInput() { oskTouchInput_ = true; }
    void MarkOskValueFeedback() { oskValueFeedback_ = true; }
    void MarkOskToggleFeedback() { oskToggleFeedback_ = true; }
    void MarkOskColorFeedback() { oskColorFeedback_ = true; }
    void MarkOskTextFeedback() { oskTextFeedback_ = true; }
    void MarkOskMeterFeedback() { oskMeterFeedback_ = true; }
    bool HasOskPressInput() const { return oskPressInput_; }
    bool HasOskRelativeInput() const { return oskRelativeInput_; }
    bool HasOskAbsoluteInput() const { return oskAbsoluteInput_; }
    bool HasOskTouchInput() const { return oskTouchInput_; }
    bool HasOskValueFeedback() const { return oskValueFeedback_; }
    bool HasOskToggleFeedback() const { return oskToggleFeedback_; }
    bool HasOskColorFeedback() const { return oskColorFeedback_; }
    bool HasOskTextFeedback() const { return oskTextFeedback_; }
    bool HasOskMeterFeedback() const { return oskMeterFeedback_; }
    string GetOskRole() const {
        if (oskAbsoluteInput_) return "fader";
        if (oskRelativeInput_ || oskWidgetClass_.find("Rotary") != string::npos || oskWidgetClass_.find("Encoder") != string::npos) return "rotary";
        if (oskMeterFeedback_) return "meter";
        if (oskTextFeedback_) return "display";
        if (oskPressInput_) return "button";
        return "unknown";
    }

    void SetAccelerationValues(const vector<double> accelerationValues) { accelerationValues_ = accelerationValues; }
    const vector<double>& GetAccelerationValues() { return accelerationValues_; }

    void SetIncomingMessageTime(int lastIncomingMessageTime) { lastIncomingMessageTime_ = lastIncomingMessageTime; }
    int GetLastIncomingMessageTime() { return lastIncomingMessageTime_; }

    void SetLastIncomingDelta(double delta) { lastIncomingDelta_ = delta; }
    double GetLastIncomingDelta() { return lastIncomingDelta_; }

    void Configure(const vector<unique_ptr<ActionContext>>& contexts);
    void UpdateValue(const PropertyList& properties, double value);
    void UpdateValue(const PropertyList& properties, const char* const& value);
    void ForceValue(const PropertyList& properties, const char* const& value);
    void UpdateColorValue(const rgba_color& color);
    void SetXTouchDisplayColors(const char* colors);
    void RestoreXTouchDisplayColors();
    void ForceClear();

    void SetHasDoublePressActions() { hasDoublePressActions_ = true; };
    bool HasDoublePressActions() { return hasDoublePressActions_; };

    void SetHasHoldActions() { hasHoldActions_ = true; };
    bool HasHoldActions() { return hasHoldActions_; };
    void SetHoldFired() { holdFired_ = true; };
    bool GetHoldFired() { return holdFired_; };
    void ClearHoldFired() { holdFired_ = false; };

    double GetLastFeedbackValue() const {
        return lastFeedbackValue_;
    }

    rgba_color GetLastFeedbackColor() const {
        return lastFeedbackColor_;
    }

    void LogInput(double value);
};
