#pragma once

#include <optional>
#include <string>

enum class Format2ZoneNavigationKind {
    IndependentZone,
    ZoneLayer,
};

enum class Format2ActionArgumentKind {
    Unspecified,
    ZoneId,
    SignedInteger,
};

std::optional<Format2ZoneNavigationKind> GetFormat2ActionZoneNavigationKind(const std::string& actionName);
Format2ActionArgumentKind GetFormat2ActionArgumentKind(const std::string& actionName);
bool IsFormat2ZoneLayerOnlyAction(const std::string& actionName);
