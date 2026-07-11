#pragma once
// navigator.h — Navigator (strategy pattern) and factory helpers.
// All five former subclasses (TrackNavigator, FixedTrackNavigator, MasterTrackNavigator, SelectedTrackNavigator, FocusedFXNavigator) have been collapsed into the single Navigator class. 
// "how to resolve the track" logic is supplied at construction time via a TrackResolver callable.
// Factory helpers (CreateMasterTrackNavigator, etc.) recreate each flavour.
// CreateTrackNavigator requires TrackNavigationManager to be complete, so it lives at the bottom of track_nav_manager.h instead.
#include "page_interface.h"
#include "preamble.h"

class Navigator
{
public:
    // Strategy type: given the navigator itself, return the current MediaTrack.
    using TrackResolver = std::function<MediaTrack*(Navigator&)>;

protected:
    CSurfIntegrator* const csi_;
    IPageContext* const page_;

    // Touch state — unchanged from before.
    bool isVolumeTouched_ = false;
    bool isPanTouched_ = false;
    bool isPanWidthTouched_ = false;
    bool isPanLeftTouched_ = false;
    bool isPanRightTouched_ = false;
    bool isMCUTrackPanWidth_ = false;

private:
    NavigatorType type_;
    int channelNum_; // meaningful only for TrackNavigator flavour; 0 otherwise
    TrackResolver resolver_;

public:
    Navigator(CSurfIntegrator* const csi, IPageContext* page, NavigatorType type, TrackResolver resolver, int channelNum = 0)
        : csi_(csi), page_(page), type_(type), channelNum_(channelNum), resolver_(std::move(resolver)) {}

    virtual ~Navigator() {}

    // ----- identity -----
    NavigatorType GetType() const { return type_; }
    const char* GetName() const { return TypeToName(type_); }
    int GetChannelNum() const { return channelNum_; }

    // ----- track resolution (delegates to the injected strategy) -----
    MediaTrack* GetTrack() { return resolver_ ? resolver_(*this) : nullptr; }

    // ----- type ↔ name conversions (static helpers, unchanged) -----
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

    // ----- touch state (unchanged interface) -----
    bool GetIsNavigatorTouched() const {
        return isVolumeTouched_ || isPanTouched_ || isPanWidthTouched_ || isPanLeftTouched_ || isPanRightTouched_;
    }

    void SetIsVolumeTouched(bool v) { isVolumeTouched_ = v; }
    bool GetIsVolumeTouched() const { return isVolumeTouched_; }

    void SetIsPanTouched(bool v) { isPanTouched_ = v; }
    bool GetIsPanTouched() const { return isPanTouched_; }

    void SetIsPanWidthTouched(bool v) { isPanWidthTouched_ = v; }
    bool GetIsPanWidthTouched() const { return isPanWidthTouched_; }

    void SetIsPanLeftTouched(bool v) { isPanLeftTouched_ = v; }
    bool GetIsPanLeftTouched() const { return isPanLeftTouched_; }

    void SetIsPanRightTouched(bool v) { isPanRightTouched_ = v; }
    bool GetIsPanRightTouched() const { return isPanRightTouched_; }
};

// =============================================================================
// Factory helpers
// (CreateTrackNavigator is in track_nav_manager.h — it needs that class complete)
// =============================================================================

inline std::unique_ptr<Navigator>
CreateMasterTrackNavigator(CSurfIntegrator* csi, IPageContext* page) {
    return std::make_unique<Navigator>(csi, page, NavigatorType::MasterTrackNavigator, [](Navigator&) -> MediaTrack* { return GetMasterTrack(nullptr); });
}

inline std::unique_ptr<Navigator>
CreateFocusedFXNavigator(CSurfIntegrator* csi, IPageContext* page) {
    return std::make_unique<Navigator>(csi, page, NavigatorType::FocusedFXNavigator, [](Navigator&) -> MediaTrack* {
        int trackNumber = 0, itemNumber = 0, takeNumber = 0, fxIndex = 0, paramIndex = 0;
        int retVal = GetTouchedOrFocusedFX(1, &trackNumber, &itemNumber, &takeNumber, &fxIndex, &paramIndex);
        trackNumber++; // REAPER returns 0-based; convert to 1-based

        if (retVal && !(paramIndex & 0x01)) { // Track FX (not item FX)
            if (trackNumber > 0) return DAW::GetTrack(trackNumber);
            else if (trackNumber == 0) return GetMasterTrack(nullptr);
            else return nullptr;
        }
        return nullptr;
    });
}

inline std::unique_ptr<Navigator>
CreateFixedTrackNavigator(CSurfIntegrator* csi, IPageContext* page, MediaTrack* track) {
    return std::make_unique<Navigator>(csi, page, NavigatorType::FixedTrackNavigator, [track](Navigator&) -> MediaTrack* { return track; });
}
