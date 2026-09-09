#pragma once

#include <string>

enum class ZoneProfileSourceMode {
    Vendor,
    VendorAndUser,
    User,
};

inline const char* ZoneProfileSourceModeName(ZoneProfileSourceMode mode) {
    if (mode == ZoneProfileSourceMode::Vendor) return "Vendor";
    if (mode == ZoneProfileSourceMode::User) return "User";
    return "VendorAndUser";
}

inline bool ParseZoneProfileSourceMode(const std::string& value, ZoneProfileSourceMode& mode) {
    if (value == "Vendor") mode = ZoneProfileSourceMode::Vendor;
    else if (value == "VendorAndUser") mode = ZoneProfileSourceMode::VendorAndUser;
    else if (value == "User") mode = ZoneProfileSourceMode::User;
    else return false;
    return true;
}
