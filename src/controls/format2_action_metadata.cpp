#include "format2_action_metadata.h"

std::optional<Format2ZoneNavigationKind> GetFormat2ActionZoneNavigationKind(const std::string& actionName) {
    if (actionName == "GoZone") return Format2ZoneNavigationKind::IndependentZone;
    if (actionName == "EnterZoneLayer") return Format2ZoneNavigationKind::ZoneLayer;
    return std::nullopt;
}
