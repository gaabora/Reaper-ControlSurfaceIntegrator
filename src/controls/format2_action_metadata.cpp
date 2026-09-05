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
#define X(contextActionName) if (actionName == contextActionName) return true;
    FORMAT2_CONTEXT_CHANGING_ACTION_LIST(X)
#undef X
    return false;
}

bool Format2ActionChangesModifier(const std::string& actionName) {
#define X(modifierActionName) if (actionName == modifierActionName) return true;
    FORMAT2_MODIFIER_ACTION_LIST(X)
#undef X
    return false;
}
