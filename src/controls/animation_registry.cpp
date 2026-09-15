#include "animation_registry.h"

#include "../shared/settings_values.h"
#include "preamble.h"

enum class AnimationDuration {
    Fast,
    Normal,
    Slow,
};

struct AnimationFrame {
    double value;
    AnimationDuration duration;
};

struct AnimationDefinition {
    const char* name;
    const AnimationFrame* frames;
    std::size_t frameCount;
};

static constexpr AnimationFrame BlinkFastFrames[] = {{true, AnimationDuration::Fast}, {false, AnimationDuration::Fast}};
static constexpr AnimationFrame BlinkNormalFrames[] = {{true, AnimationDuration::Normal}, {false, AnimationDuration::Normal}};
static constexpr AnimationFrame BlinkSlowFrames[] = {{true, AnimationDuration::Slow}, {false, AnimationDuration::Slow}};
static constexpr AnimationFrame BlinkDoubleFrames[] = {{true, AnimationDuration::Fast}, {false, AnimationDuration::Fast}, {true, AnimationDuration::Fast}, {false, AnimationDuration::Slow}};
static constexpr AnimationFrame BlinkHeartbeatFrames[] = {{true, AnimationDuration::Fast}, {false, AnimationDuration::Normal}, {true, AnimationDuration::Fast}, {false, AnimationDuration::Slow}};

static constexpr AnimationDefinition AnimationDefinitions[] = {
    {"BlinkFast", BlinkFastFrames, sizeof(BlinkFastFrames) / sizeof(BlinkFastFrames[0])},
    {"BlinkNormal", BlinkNormalFrames, sizeof(BlinkNormalFrames) / sizeof(BlinkNormalFrames[0])},
    {"BlinkSlow", BlinkSlowFrames, sizeof(BlinkSlowFrames) / sizeof(BlinkSlowFrames[0])},
    {"BlinkDouble", BlinkDoubleFrames, sizeof(BlinkDoubleFrames) / sizeof(BlinkDoubleFrames[0])},
    {"BlinkHeartbeat", BlinkHeartbeatFrames, sizeof(BlinkHeartbeatFrames) / sizeof(BlinkHeartbeatFrames[0])},
};

static const AnimationDefinition& FindAnimationDefinition(const std::string& animationName) {
    for (const AnimationDefinition& definition : AnimationDefinitions) if (animationName == definition.name) return definition;
    return AnimationDefinitions[1];
}

static int ResolveAnimationDuration(AnimationDuration duration, const SettingsValues& settings, int normalIntervalMs) {
    if (duration == AnimationDuration::Fast) return (std::max)(1, settings.GetInteger("BlinkFastIntervalMs"));
    if (duration == AnimationDuration::Slow) return (std::max)(1, settings.GetInteger("BlinkSlowIntervalMs"));
    return (std::max)(1, normalIntervalMs);
}

std::string ResolveAnimationPresetName(const char* value) {
    if (value) for (const AnimationDefinition& definition : AnimationDefinitions) if (IsSameString(value, definition.name)) return definition.name;
    return "BlinkNormal";
}

std::string AnimationPresetForLegacyInterval(int intervalMs) {
    if (intervalMs <= 250) return "BlinkFast";
    if (intervalMs >= 750) return "BlinkSlow";
    return "BlinkNormal";
}

double SampleAnimationPreset(const std::string& animationName, const SettingsValues& settings, int normalIntervalMs, std::uint32_t nowMs) {
    const AnimationDefinition& definition = FindAnimationDefinition(animationName);
    std::uint32_t cycleDurationMs = 0;
    for (std::size_t frameIdx = 0; frameIdx < definition.frameCount; frameIdx++) cycleDurationMs += static_cast<std::uint32_t>(ResolveAnimationDuration(definition.frames[frameIdx].duration, settings, normalIntervalMs));
    std::uint32_t phaseMs = nowMs % cycleDurationMs;
    for (std::size_t frameIdx = 0; frameIdx < definition.frameCount; frameIdx++) {
        const std::uint32_t frameDurationMs = static_cast<std::uint32_t>(ResolveAnimationDuration(definition.frames[frameIdx].duration, settings, normalIntervalMs));
        if (phaseMs < frameDurationMs) return definition.frames[frameIdx].value;
        phaseMs -= frameDurationMs;
    }
    return 0.0;
}
