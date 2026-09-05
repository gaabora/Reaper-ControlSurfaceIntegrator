#include "button_gesture.h"

bool ButtonGestureRecognizer::HasDoublePressBinding() const {
    for (const ButtonGestureBinding& binding : this->bindings_) if (binding.inputEvent == ActionInputEvent::DoublePress) return true;
    return false;
}

bool ButtonGestureRecognizer::HasTapBinding() const {
    for (const ButtonGestureBinding& binding : this->bindings_) if (binding.inputEvent == ActionInputEvent::Tap) return true;
    return false;
}

void ButtonGestureRecognizer::AppendEventDispatches(ActionInputEvent inputEvent, double value, std::vector<ButtonGestureDispatch>& dispatches) const {
    for (std::size_t bindingIdx = 0; bindingIdx < this->bindings_.size(); bindingIdx++) {
        const ButtonGestureBinding& binding = this->bindings_[bindingIdx];
        bool matches = binding.inputEvent == inputEvent;
        if (binding.inputEvent == ActionInputEvent::Modifier) matches = inputEvent == ActionInputEvent::Press && binding.modifierMode != ActionModifierMode::Latch;
        if (matches) dispatches.push_back({bindingIdx, inputEvent, value});
    }
}

void ButtonGestureRecognizer::AppendModifierReleaseDispatches(DWORD heldTimeMs, bool allowTap, std::vector<ButtonGestureDispatch>& dispatches) const {
    for (std::size_t bindingIdx = 0; bindingIdx < this->bindings_.size(); bindingIdx++) {
        const ButtonGestureBinding& binding = this->bindings_[bindingIdx];
        if (binding.inputEvent != ActionInputEvent::Modifier) continue;
        if (binding.modifierMode == ActionModifierMode::Momentary) {
            dispatches.push_back({bindingIdx, ActionInputEvent::Release, this->pressValue_});
        } else if (binding.modifierMode == ActionModifierMode::Latch && allowTap) {
            dispatches.push_back({bindingIdx, ActionInputEvent::Tap, this->pressValue_});
        } else if (binding.modifierMode == ActionModifierMode::Hybrid) {
            dispatches.push_back({bindingIdx, allowTap && heldTimeMs < (DWORD) binding.modifierTapWindowMs ? ActionInputEvent::Tap : ActionInputEvent::Release, this->pressValue_});
        }
    }
}

void ButtonGestureRecognizer::AppendDueMilestones(DWORD nowTs, std::vector<ButtonGestureDispatch>& dispatches) {
    if (!this->isPressed_) return;

    std::vector<std::size_t> dueBindings;
    for (std::size_t bindingIdx = 0; bindingIdx < this->bindings_.size(); bindingIdx++) {
        const ButtonGestureBinding& binding = this->bindings_[bindingIdx];
        if (binding.inputEvent != ActionInputEvent::Hold && binding.inputEvent != ActionInputEvent::LongHold) continue;
        if (!this->milestoneFired_[bindingIdx] && nowTs - this->pressTs_ >= (DWORD) binding.delayMs) dueBindings.push_back(bindingIdx);
    }
    std::stable_sort(dueBindings.begin(), dueBindings.end(), [this](std::size_t leftIdx, std::size_t rightIdx) {
        if (this->bindings_[leftIdx].delayMs != this->bindings_[rightIdx].delayMs) return this->bindings_[leftIdx].delayMs < this->bindings_[rightIdx].delayMs;
        return leftIdx < rightIdx;
    });
    for (std::size_t bindingIdx : dueBindings) {
        const ButtonGestureBinding& binding = this->bindings_[bindingIdx];
        this->milestoneFired_[bindingIdx] = true;
        this->milestoneWasFired_ = true;
        if (binding.repeatIntervalMs > 0) this->nextRepeatTs_[bindingIdx] = nowTs + (DWORD) binding.repeatIntervalMs;
        dispatches.push_back({bindingIdx, binding.inputEvent, this->pressValue_});
    }

    for (std::size_t bindingIdx = 0; bindingIdx < this->bindings_.size(); bindingIdx++) {
        const ButtonGestureBinding& binding = this->bindings_[bindingIdx];
        if (!this->milestoneFired_[bindingIdx] || binding.repeatIntervalMs <= 0 || this->nextRepeatTs_[bindingIdx] == 0 || nowTs < this->nextRepeatTs_[bindingIdx]) continue;
        this->nextRepeatTs_[bindingIdx] = nowTs + (DWORD) binding.repeatIntervalMs;
        dispatches.push_back({bindingIdx, binding.inputEvent, this->pressValue_});
    }
}

void ButtonGestureRecognizer::AppendDueTap(DWORD nowTs, std::vector<ButtonGestureDispatch>& dispatches) {
    if (!this->pendingTap_ || nowTs - this->firstPressTs_ < (DWORD) this->doublePressWindowMs_) return;
    this->pendingTap_ = false;
    this->awaitingSecondPress_ = false;
    this->AppendEventDispatches(ActionInputEvent::Tap, this->pressValue_, dispatches);
}

void ButtonGestureRecognizer::Configure(const std::vector<ButtonGestureBinding>& bindings, int doublePressWindowMs, bool exclusiveDoublePress) {
    this->bindings_ = bindings;
    this->doublePressWindowMs_ = doublePressWindowMs;
    this->exclusiveDoublePress_ = exclusiveDoublePress;
    this->Reset();
}

std::vector<ButtonGestureDispatch> ButtonGestureRecognizer::ProcessInput(double value, DWORD nowTs) {
    std::vector<ButtonGestureDispatch> dispatches = this->Poll(nowTs);
    const bool isRelease = value == 0.0;

    if (!isRelease) {
        if (this->isPressed_) {
            this->AppendEventDispatches(ActionInputEvent::Release, this->pressValue_, dispatches);
            this->AppendModifierReleaseDispatches(nowTs - this->pressTs_, false, dispatches);
            this->isPressed_ = false;
            this->awaitingSecondPress_ = false;
            this->pendingTap_ = false;
            this->milestoneWasFired_ = false;
        }
        this->currentPressIsDouble_ = this->awaitingSecondPress_ && nowTs - this->firstPressTs_ <= (DWORD) this->doublePressWindowMs_;
        if (this->currentPressIsDouble_) {
            this->pendingTap_ = false;
            this->awaitingSecondPress_ = false;
        } else if (this->HasDoublePressBinding()) {
            this->awaitingSecondPress_ = true;
            this->firstPressTs_ = nowTs;
        }
        this->isPressed_ = true;
        this->pressTs_ = nowTs;
        this->pressValue_ = value;
        this->milestoneWasFired_ = false;
        std::fill(this->milestoneFired_.begin(), this->milestoneFired_.end(), false);
        std::fill(this->nextRepeatTs_.begin(), this->nextRepeatTs_.end(), 0);
        this->AppendEventDispatches(ActionInputEvent::Press, value, dispatches);
        if (this->currentPressIsDouble_) this->AppendEventDispatches(ActionInputEvent::DoublePress, value, dispatches);
        return dispatches;
    }

    if (!this->isPressed_) return dispatches;
    this->AppendDueMilestones(nowTs, dispatches);
    this->AppendEventDispatches(ActionInputEvent::Release, this->pressValue_, dispatches);
    this->AppendModifierReleaseDispatches(nowTs - this->pressTs_, !this->milestoneWasFired_, dispatches);
    if (!this->milestoneWasFired_) {
        if (!this->currentPressIsDouble_ || !this->exclusiveDoublePress_) {
            if (this->exclusiveDoublePress_ && this->HasDoublePressBinding()) {
                this->pendingTap_ = this->HasTapBinding();
                this->AppendDueTap(nowTs, dispatches);
            } else {
                this->AppendEventDispatches(ActionInputEvent::Tap, this->pressValue_, dispatches);
            }
        }
    } else {
        this->pendingTap_ = false;
        this->awaitingSecondPress_ = false;
    }
    this->isPressed_ = false;
    this->currentPressIsDouble_ = false;
    return dispatches;
}

std::vector<ButtonGestureDispatch> ButtonGestureRecognizer::Poll(DWORD nowTs) {
    std::vector<ButtonGestureDispatch> dispatches;
    this->AppendDueMilestones(nowTs, dispatches);
    this->AppendDueTap(nowTs, dispatches);
    if (!this->isPressed_ && this->awaitingSecondPress_ && nowTs - this->firstPressTs_ >= (DWORD) this->doublePressWindowMs_) this->awaitingSecondPress_ = false;
    return dispatches;
}

void ButtonGestureRecognizer::Reset() {
    this->milestoneFired_.assign(this->bindings_.size(), false);
    this->nextRepeatTs_.assign(this->bindings_.size(), 0);
    this->isPressed_ = false;
    this->awaitingSecondPress_ = false;
    this->pendingTap_ = false;
    this->currentPressIsDouble_ = false;
    this->milestoneWasFired_ = false;
    this->pressTs_ = 0;
    this->firstPressTs_ = 0;
    this->pressValue_ = 0.0;
}
