#include "format2_action_metadata.h"

std::optional<Format2ZoneNavigationKind> GetFormat2ActionZoneNavigationKind(const std::string& actionName) {
    if (actionName == "GoZone") return Format2ZoneNavigationKind::IndependentZone;
    if (actionName == "EnterZoneLayer") return Format2ZoneNavigationKind::ZoneLayer;
    return std::nullopt;
}

Format2ActionArgumentKind GetFormat2ActionArgumentKind(const std::string& actionName) {
    if (actionName == "ToggleSelectedTrackFX" || actionName == "ClearLastTouchedFXParam" || actionName == "ClearFocusedFX" || actionName == "ClearSelectedTrackFX" || actionName == "ClearFXSlot") return Format2ActionArgumentKind::None;
    if (actionName == "GoZone" || actionName == "EnterZoneLayer") return Format2ActionArgumentKind::ZoneId;
    if (actionName == "Bank") return Format2ActionArgumentKind::SignedInteger;
    return Format2ActionArgumentKind::Unspecified;
}

bool IsFormat2ZoneLayerOnlyAction(const std::string& actionName) {
    return actionName == "ExitZoneLayer";
}

bool Format2ActionChangesContext(const std::string& actionName) {
    return actionName == "NextPage" || actionName == "GoPage" || actionName == "GoHome" || actionName == "AllSurfacesGoHome" || actionName == "GoZone" || actionName == "EnterZoneLayer" || actionName == "ExitZoneLayer" || actionName == "GoFXSlot" || actionName == "ToggleSelectedTrackFX" || actionName == "ClearLastTouchedFXParam" || actionName == "ClearFocusedFX" || actionName == "ClearSelectedTrackFX" || actionName == "ClearFXSlot";
}
