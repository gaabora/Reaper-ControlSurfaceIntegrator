#pragma once

#include <cstdint>
#include <string>

class SettingsValues;

std::string ResolveAnimationPresetName(const char* value);
std::string AnimationPresetForLegacyInterval(int intervalMs);
double SampleAnimationPreset(const std::string& animationName, const SettingsValues& settings, int normalIntervalMs, std::uint32_t nowMs);
