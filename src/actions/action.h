#pragma once
//
//  action.h — Action base class
//
#include "../controls/preamble.h"

//! @action (abstract base)
//!
//! @brief Root base class for all actions. Provides virtual Do/RequestUpdate/Touch/GetCurrentNormalizedValue interface.
//!
//! @feedback None (subclasses override RequestUpdate to send widget feedback).
//!
//! @notes Actions are singletons registered by name. ActionContext binds an Action to a Widget+Zone+params. Subclasses override Do() for press, RequestUpdate() for LED/display feedback, Touch() for fader touch automation.
class Action
{
public:
    virtual ~Action() {}

    virtual const char* GetName() const { return TypeToName(GetType()); }
    virtual ActionType GetType() const { return ActionType::Abstract; }

    virtual PropertyList::FeedbackShape GetFeedbackShape() const {
        switch (this->GetType()) {
            case ActionType::TrackVolume:
            case ActionType::TrackSendVolume:
            case ActionType::TrackReceiveVolume:
            case ActionType::TrackOutputMeter:
            case ActionType::TrackOutputMeterAverageLR:
            case ActionType::TrackOutputMeterMaxPeakLR:
            case ActionType::TrackVolumeWithMeterAverageLR:
            case ActionType::TrackVolumeWithMeterMaxPeakLR:
            case ActionType::FXGainReductionMeter:
                return PropertyList::FeedbackShape::Level;
            case ActionType::TrackPan:
            case ActionType::TrackPanL:
            case ActionType::TrackPanR:
            case ActionType::TrackPanAutoLeft:
            case ActionType::TrackSendPan:
            case ActionType::TrackReceivePan:
                return PropertyList::FeedbackShape::Centered;
            case ActionType::TrackPanWidth:
                return PropertyList::FeedbackShape::Spread;
            default:
                return PropertyList::FeedbackShape::None;
        }
    }

    static const char* TypeToName(ActionType type) {
        switch (type) {
          #define X(enumName, strName) case ActionType::enumName: return strName;
            ACTION_TYPE_LIST(X)
          #undef X
            default: return "Unknown";
        }
    }

    static ActionType NameToType(const std::string& name) {
      #define X(enumName, strName) if (name == strName)  return ActionType::enumName;
        ACTION_TYPE_LIST(X)
      #undef X
        return ActionType::Invalid;
    }

    static std::vector<std::string> GetSupportedNames() {
        std::vector<std::string> names;
      #define X(enumName, strName) names.push_back(strName);
        ACTION_TYPE_LIST(X)
      #undef X
        return names;
    }

    virtual bool IsModifier() { return false; }
    virtual bool IsSwitch() { return false; }
    virtual bool IsDisplayRelated() { return false; }
    virtual bool IsMeterRelated() { return false; }
    virtual bool IsVolumeRelated() { return false; }
    virtual bool IsPanRelated() { return false; }
    virtual bool IsFxRelated() { return false; }
    virtual bool IsSettingsRelated() { return false; }
    virtual bool IsTransportRelated() { return false; }
    virtual bool IsTrackRelated() { return false; }
    virtual bool IsTrackSendRelated() { return false; }
    virtual bool IsTrackReceiveRelated() { return false; }
    virtual bool IgnoresRelease() const { return false; }

    virtual void Touch(ActionContext* context, double value) {}
    virtual void RequestUpdate(ActionContext* context) {}
    virtual void Do(ActionContext* context, double value) {}
    virtual double GetCurrentNormalizedValue(ActionContext* context) { return 0.0; }
    virtual double GetCurrentDBValue(ActionContext* context) { return 0.0; }

    int GetPanMode(MediaTrack* track) {
        double pan1, pan2 = 0.0;
        int panMode = 0;
        GetTrackUIPan(track, &pan1, &pan2, &panMode);
        return panMode;
    }
};
