#pragma once

#include <optional>
#include <string>

enum class Format2ZoneNavigationKind {
    IndependentZone,
    ZoneLayer,
};

std::optional<Format2ZoneNavigationKind> GetFormat2ActionZoneNavigationKind(const std::string& actionName);
