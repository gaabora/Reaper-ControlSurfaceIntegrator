#pragma once
//
//  action.h — Action base class
//
#include "preamble.h"

class Action
{
public:
    virtual ~Action() {}

    virtual const char* GetName() const { return TypeToName(GetType()); }
    virtual ActionType GetType() const { return ActionType::Abstract; }

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
