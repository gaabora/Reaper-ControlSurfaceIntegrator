#pragma once

enum class ActionInputEvent {
    Legacy,
    Press,
    Tap,
    Release,
    Hold,
    LongHold,
    DoublePress,
    Modifier,
};

enum class ActionModifierMode {
    Legacy,
    Momentary,
    Latch,
    Hybrid,
};
