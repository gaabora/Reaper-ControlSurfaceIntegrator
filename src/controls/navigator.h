#pragma once
//
//  navigator.h — Navigator base class and all concrete Navigator subclasses
//
#include "preamble.h"
#include "page_interface.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    const CSurfIntegrator *const csi_;
    IPageContext *const page_;
    bool isVolumeTouched_ = false;
    bool isPanTouched_ = false;
    bool isPanWidthTouched_ = false;
    bool isPanLeftTouched_ = false;
    bool isPanRightTouched_ = false;
    bool isMCUTrackPanWidth_ = false;

    Navigator(const CSurfIntegrator *const csi, IPageContext * page) : csi_(csi), page_(page) {}

public:
    virtual ~Navigator() {}

    virtual const char* GetName() const { return TypeToName(GetType()); }
    virtual NavigatorType GetType() const { return NavigatorType::Abstract; }

    static const char* TypeToName(NavigatorType type) {
        switch (type) {
          #define X(enumName, strName) case NavigatorType::enumName: return strName;
            NAVIGATOR_TYPE_LIST(X)
          #undef X
            default: return "Unknown";
        }
    }
    
    static NavigatorType NameToType(const std::string& name) {
      #define X(enumName, strName) if (name == strName) return NavigatorType::enumName;
        NAVIGATOR_TYPE_LIST(X)
      #undef X
        return NavigatorType::Invalid;
    }

    static std::vector<std::string> GetSupportedNames() {
        std::vector<std::string> names;
      #define X(enumName, strName) names.push_back(strName);
        NAVIGATOR_TYPE_LIST(X)
      #undef X
        return names;
    }

    virtual MediaTrack *GetTrack() { return NULL; }
    virtual int GetChannelNum() { return 0; }

    bool GetIsNavigatorTouched() { return isVolumeTouched_ || isPanTouched_ || isPanWidthTouched_ || isPanLeftTouched_ || isPanRightTouched_; }
    
    void SetIsVolumeTouched(bool isVolumeTouched) { isVolumeTouched_ = isVolumeTouched;  }
    bool GetIsVolumeTouched() { return isVolumeTouched_;  }
    
    void SetIsPanTouched(bool isPanTouched) { isPanTouched_ = isPanTouched; }
    bool GetIsPanTouched() { return isPanTouched_;  }
    
    void SetIsPanWidthTouched(bool isPanWidthTouched) { isPanWidthTouched_ = isPanWidthTouched; }
    bool GetIsPanWidthTouched() { return isPanWidthTouched_;  }
    
    void SetIsPanLeftTouched(bool isPanLeftTouched) { isPanLeftTouched_ = isPanLeftTouched; }
    bool GetIsPanLeftTouched() { return isPanLeftTouched_;  }
    
    void SetIsPanRightTouched(bool isPanRightTouched) { isPanRightTouched_ = isPanRightTouched; }
    bool GetIsPanRightTouched() { return isPanRightTouched_;  }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TrackNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int const channelNum_;
    
protected:
    TrackNavigationManager *const trackNavigationManager_;

public:
    TrackNavigator(CSurfIntegrator *const csi, IPageContext *page, TrackNavigationManager *trackNavigationManager, int channelNum) : Navigator(csi, page), trackNavigationManager_(trackNavigationManager), channelNum_(channelNum) {}
    virtual ~TrackNavigator() {}
    
    NavigatorType GetType() const override { return NavigatorType::TrackNavigator; }
   
    virtual MediaTrack *GetTrack() override;
    
    virtual int GetChannelNum() override { return channelNum_; }

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FixedTrackNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    MediaTrack *const track_;
    
public:
    FixedTrackNavigator(CSurfIntegrator *const csi, IPageContext *page, MediaTrack *const track) : Navigator(csi, page), track_(track) {}
    virtual ~FixedTrackNavigator() {}
    
    NavigatorType GetType() const override { return NavigatorType::FixedTrackNavigator; }
   
    virtual MediaTrack *GetTrack() override { return track_; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MasterTrackNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    MasterTrackNavigator(CSurfIntegrator *const csi, IPageContext * page) : Navigator(csi, page) {}
    virtual ~MasterTrackNavigator() {}
    
    NavigatorType GetType() const override { return NavigatorType::MasterTrackNavigator; }
    
    virtual MediaTrack *GetTrack() override;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SelectedTrackNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    SelectedTrackNavigator(CSurfIntegrator *const csi, IPageContext * page) : Navigator(csi, page) {}
    virtual ~SelectedTrackNavigator() {}
    
    NavigatorType GetType() const override { return NavigatorType::SelectedTrackNavigator; }
    
    virtual MediaTrack *GetTrack() override;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FocusedFXNavigator : public Navigator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    FocusedFXNavigator(CSurfIntegrator *const csi, IPageContext * page) : Navigator(csi, page) {}
    virtual ~FocusedFXNavigator() {}
    
    NavigatorType GetType() const override { return NavigatorType::FocusedFXNavigator; }
    
    virtual MediaTrack *GetTrack() override;
};
