#include "button_gesture.h"

#include <cstdlib>
#include <iostream>
#include <limits>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

static void Expect(const std::vector<ButtonGestureDispatch>& actual, const std::vector<ButtonGestureDispatch>& expected, const char* message) {
    Require(actual.size() == expected.size(), message);
    for (std::size_t idx = 0; idx < expected.size(); idx++) {
        Require(actual[idx].bindingIndex == expected[idx].bindingIndex && actual[idx].inputEvent == expected[idx].inputEvent && actual[idx].value == expected[idx].value, message);
    }
}

static void TestPressReleaseAndTap() {
    ButtonGestureRecognizer recognizer;
    recognizer.Configure({{ActionInputEvent::Press}, {ActionInputEvent::Release}, {ActionInputEvent::Tap}}, 400, true);
    Expect(recognizer.ProcessInput(0, 90), {}, "ignore unmatched release");
    Expect(recognizer.ProcessInput(0.75, 100), {{0, ActionInputEvent::Press, 0.75}}, "press immediately");
    Expect(recognizer.ProcessInput(0, 120), {{1, ActionInputEvent::Release, 0.75}, {2, ActionInputEvent::Tap, 0.75}}, "release and tap preserve press value");
    Require(!recognizer.HasActiveState(), "simple tap completes");
    Expect(recognizer.ProcessInput(0, 121), {}, "ignore duplicate release");
}

static void TestHoldMilestones() {
    ButtonGestureRecognizer recognizer;
    recognizer.Configure({{ActionInputEvent::LongHold, ActionModifierMode::Legacy, 2000}, {ActionInputEvent::Hold, ActionModifierMode::Legacy, 1000}, {ActionInputEvent::Tap}, {ActionInputEvent::Release}}, 400, true);
    Expect(recognizer.ProcessInput(1, 100), {}, "hold waits");
    Expect(recognizer.Poll(1099), {}, "hold before threshold");
    Expect(recognizer.Poll(1100), {{1, ActionInputEvent::Hold, 1}}, "hold at threshold");
    Expect(recognizer.Poll(2100), {{0, ActionInputEvent::LongHold, 1}}, "long hold at threshold");
    Expect(recognizer.Poll(2200), {}, "milestones fire once");
    Expect(recognizer.ProcessInput(0, 2201), {{3, ActionInputEvent::Release, 1}}, "hold suppresses tap but not release");
    recognizer.ProcessInput(1, 3000);
    Expect(recognizer.ProcessInput(0, 5000), {{1, ActionInputEvent::Hold, 1}, {0, ActionInputEvent::LongHold, 1}, {3, ActionInputEvent::Release, 1}}, "late poll orders milestones by time before release");
}

static void TestDoublePress() {
    for (bool exclusive : {false, true}) {
        ButtonGestureRecognizer recognizer;
        recognizer.Configure({{ActionInputEvent::Tap}, {ActionInputEvent::DoublePress}, {ActionInputEvent::Press}, {ActionInputEvent::Release}}, 400, exclusive);
        Expect(recognizer.ProcessInput(1, 100), {{2, ActionInputEvent::Press, 1}}, "first press");
        std::vector<ButtonGestureDispatch> release = {{3, ActionInputEvent::Release, 1}};
        if (!exclusive) release.push_back({0, ActionInputEvent::Tap, 1});
        Expect(recognizer.ProcessInput(0, 120), release, "first release follows policy");
        Expect(recognizer.ProcessInput(1, 499), {{2, ActionInputEvent::Press, 1}, {1, ActionInputEvent::DoublePress, 1}}, "second press inside window");
        Expect(recognizer.ProcessInput(0, 510), release, "second release follows policy");
        Expect(recognizer.Poll(900), {}, "no extra tap after double");
        Require(!recognizer.HasActiveState(), "double gesture completes");
    }
}

static void TestDoublePressDeadline() {
    for (bool withTap : {false, true}) {
        ButtonGestureRecognizer recognizer;
        std::vector<ButtonGestureBinding> bindings = {{ActionInputEvent::DoublePress}};
        if (withTap) bindings.push_back({ActionInputEvent::Tap});
        recognizer.Configure(bindings, 400, true);
        recognizer.ProcessInput(1, 100);
        recognizer.ProcessInput(0, 120);
        Expect(recognizer.Poll(499), {}, "exclusive tap waits for deadline");
        std::vector<ButtonGestureDispatch> expiredTap;
        if (withTap) expiredTap.push_back({1, ActionInputEvent::Tap, 1});
        Expect(recognizer.ProcessInput(1, 500), expiredTap, "deadline expires before new press with or without tap");
        recognizer.ProcessInput(0, 520);
        Expect(recognizer.Poll(900), expiredTap, "new gesture has its own deadline");
        Require(!recognizer.HasActiveState(), "expired window clears state");
    }
}

static void TestHoldWithDoublePress() {
    ButtonGestureRecognizer recognizer;
    recognizer.Configure({{ActionInputEvent::Tap}, {ActionInputEvent::DoublePress}, {ActionInputEvent::Hold, ActionModifierMode::Legacy, 200}}, 400, true);
    recognizer.ProcessInput(1, 100);
    Expect(recognizer.ProcessInput(0, 300), {{2, ActionInputEvent::Hold, 1}}, "first hold consumes tap and double candidate");
    Expect(recognizer.ProcessInput(1, 350), {}, "press after hold starts new gesture");
    recognizer.ProcessInput(0, 360);
    Expect(recognizer.ProcessInput(1, 400), {{1, ActionInputEvent::DoublePress, 1}}, "short first press permits double");
    Expect(recognizer.Poll(600), {{2, ActionInputEvent::Hold, 1}}, "second press can also reach hold");
    Expect(recognizer.ProcessInput(0, 610), {}, "second hold suppresses tap");
}

static void TestRepeatsAndClockWrap() {
    for (std::uint32_t start : {std::uint32_t(100), (std::numeric_limits<std::uint32_t>::max)() - 249}) {
        ButtonGestureRecognizer recognizer;
        recognizer.Configure({{ActionInputEvent::Hold, ActionModifierMode::Legacy, 200, 50}}, 400, true);
        recognizer.ProcessInput(1, start);
        Expect(recognizer.Poll(start + 200), {{0, ActionInputEvent::Hold, 1}}, "repeat starts at hold");
        Expect(recognizer.Poll(start + 249), {}, "repeat waits across clock wrap");
        Expect(recognizer.Poll(start + 250), {{0, ActionInputEvent::Hold, 1}}, "repeat fires at interval including timestamp zero");
        Expect(recognizer.Poll(start + 250), {}, "same poll does not repeat twice");
        Expect(recognizer.Poll(start + 1000), {{0, ActionInputEvent::Hold, 1}}, "late poll emits one repeat without burst");
        recognizer.ProcessInput(0, start + 1001);
        Expect(recognizer.Poll(start + 2000), {}, "release stops repeat");
    }
}

static void TestModifierModes() {
    for (ActionModifierMode mode : {ActionModifierMode::Momentary, ActionModifierMode::Latch, ActionModifierMode::Hybrid}) {
        for (int duration : {99, 100}) {
            ButtonGestureRecognizer recognizer;
            recognizer.Configure({{ActionInputEvent::Modifier, mode, 0, 0, 100}}, 400, true);
            std::vector<ButtonGestureDispatch> press;
            if (mode != ActionModifierMode::Latch) press.push_back({0, ActionInputEvent::Press, 1});
            Expect(recognizer.ProcessInput(1, 1000), press, "modifier press follows mode");
            const bool tap = mode == ActionModifierMode::Latch || (mode == ActionModifierMode::Hybrid && duration < 100);
            Expect(recognizer.ProcessInput(0, 1000 + duration), {{0, tap ? ActionInputEvent::Tap : ActionInputEvent::Release, 1}}, "modifier release follows mode and threshold");
        }
    }
    ButtonGestureRecognizer recognizer;
    recognizer.Configure({{ActionInputEvent::Modifier, ActionModifierMode::Latch}, {ActionInputEvent::Hold, ActionModifierMode::Legacy, 200}}, 400, true);
    recognizer.ProcessInput(1, 100);
    Expect(recognizer.ProcessInput(0, 300), {{1, ActionInputEvent::Hold, 1}}, "hold does not toggle latch");
}

static void TestLostReleaseAndReset() {
    ButtonGestureRecognizer recognizer;
    recognizer.Configure({{ActionInputEvent::Modifier, ActionModifierMode::Momentary}, {ActionInputEvent::Release}}, 400, true);
    recognizer.ProcessInput(0.5, 100);
    Expect(recognizer.ProcessInput(1, 150), {{1, ActionInputEvent::Release, 0.5}, {0, ActionInputEvent::Release, 0.5}, {0, ActionInputEvent::Press, 1}}, "new press releases captured old modifier and action");
    for (int phase = 0; phase < 4; phase++) {
        recognizer.Configure({{ActionInputEvent::Tap}, {ActionInputEvent::DoublePress}, {ActionInputEvent::Hold, ActionModifierMode::Legacy, 500, 50}}, 400, true);
        recognizer.ProcessInput(1, 100);
        if (phase == 1 || phase == 2) recognizer.ProcessInput(0, 120);
        if (phase == 2) recognizer.ProcessInput(1, 150);
        if (phase == 3) recognizer.Poll(600);
        recognizer.Reset();
        Require(!recognizer.HasActiveState(), "reset clears pressed, pending tap, double and repeat phases");
        Expect(recognizer.Poll(1000), {}, "reset cancels deferred events");
        Expect(recognizer.ProcessInput(0, 1100), {}, "reset ignores old release");
    }
}

int main() {
    TestPressReleaseAndTap();
    TestHoldMilestones();
    TestDoublePress();
    TestDoublePressDeadline();
    TestHoldWithDoublePress();
    TestRepeatsAndClockWrap();
    TestModifierModes();
    TestLostReleaseAndReset();
    std::cout << "Button gesture tests passed\n";
}
