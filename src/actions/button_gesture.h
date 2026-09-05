#pragma once

#include "action_timing.h"

struct ButtonGestureBinding {
    ActionInputEvent inputEvent = ActionInputEvent::Legacy;
    ActionModifierMode modifierMode = ActionModifierMode::Legacy;
    int delayMs = 0;
    int repeatIntervalMs = 0;
    int modifierTapWindowMs = 0;
};

struct ButtonGestureDispatch {
    std::size_t bindingIndex = 0;
    ActionInputEvent inputEvent = ActionInputEvent::Legacy;
    double value = 0.0;
};

class ButtonGestureRecognizer
{
private:
    std::vector<ButtonGestureBinding> bindings_;
    std::vector<bool> milestoneFired_;
    std::vector<DWORD> nextRepeatTs_;
    int doublePressWindowMs_ = 400;
    bool exclusiveDoublePress_ = true;
    bool isPressed_ = false;
    bool awaitingSecondPress_ = false;
    bool pendingTap_ = false;
    bool currentPressIsDouble_ = false;
    bool milestoneWasFired_ = false;
    DWORD pressTs_ = 0;
    DWORD firstPressTs_ = 0;
    double pressValue_ = 0.0;

    bool HasDoublePressBinding() const;
    bool HasTapBinding() const;
    void AppendEventDispatches(ActionInputEvent inputEvent, double value, std::vector<ButtonGestureDispatch>& dispatches) const;
    void AppendModifierReleaseDispatches(DWORD heldTimeMs, bool allowTap, std::vector<ButtonGestureDispatch>& dispatches) const;
    void AppendDueMilestones(DWORD nowTs, std::vector<ButtonGestureDispatch>& dispatches);
    void AppendDueTap(DWORD nowTs, std::vector<ButtonGestureDispatch>& dispatches);

public:
    void Configure(const std::vector<ButtonGestureBinding>& bindings, int doublePressWindowMs, bool exclusiveDoublePress);
    std::vector<ButtonGestureDispatch> ProcessInput(double value, DWORD nowTs);
    std::vector<ButtonGestureDispatch> Poll(DWORD nowTs);
    void Reset();
    bool HasActiveState() const { return this->isPressed_ || this->awaitingSecondPress_ || this->pendingTap_; }
};
