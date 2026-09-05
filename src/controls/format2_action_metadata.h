#pragma once

#include <optional>
#include <string>

#define FORMAT2_CONTEXT_CHANGING_ACTION_LIST(X) \
X("NextPage") \
X("GoPage") \
X("GoHome") \
X("AllSurfacesGoHome") \
X("GoZone") \
X("EnterZoneLayer") \
X("ExitZoneLayer") \
X("GoFXSlot") \
X("ToggleSelectedTrackFX") \
X("ClearLastTouchedFXParam") \
X("ClearFocusedFX") \
X("ClearSelectedTrackFX") \
X("ClearFXSlot")

#define FORMAT2_MODIFIER_ACTION_LIST(X) \
X("Shift") \
X("Option") \
X("Control") \
X("Alt") \
X("Flip") \
X("Marker") \
X("Nudge") \
X("Scrub") \
X("Zoom") \
X("Global") \
X("ClearModifier") \
X("ClearModifiers")

enum class Format2ZoneNavigationKind {
    IndependentZone,
    ZoneLayer,
};

enum class Format2ActionArgumentKind {
    Unspecified,
    None,
    ZoneId,
    SignedInteger,
};

std::optional<Format2ZoneNavigationKind> GetFormat2ActionZoneNavigationKind(const std::string& actionName);
Format2ActionArgumentKind GetFormat2ActionArgumentKind(const std::string& actionName);
bool IsFormat2ZoneLayerOnlyAction(const std::string& actionName);
bool Format2ActionChangesContext(const std::string& actionName);
bool Format2ActionChangesModifier(const std::string& actionName);
