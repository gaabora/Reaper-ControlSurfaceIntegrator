#pragma once
// widget_factory.h — MIDI widget type registry.
//
// Each widget type (e.g. "Press", "FB_TwoState") registers a handler lambda that receives a MidiWidgetContext and performs whatever insertions/side-effects that type requires.
//  Usage:
//    1. Call MidiWidgetRegistry::EnsureRegistered() once (e.g. from Midi_ControlSurface constructor) to populate the registry.
//    2. Build a MidiWidgetContext for each token line inside a Widget block.
//    3. Call MidiWidgetRegistry::Dispatch(widgetType, ctx).


#include "../preamble.h"

// ---------------------------------------------------------------------------
// All data for a single widget-type definition line inside a Widget block.
// Pre-parsing MIDI bytes and lookup keys here keeps each handler concise.
// ---------------------------------------------------------------------------
struct MidiWidgetContext {
    CSurfIntegrator* csi = nullptr;
    Midi_ControlSurface* surface = nullptr;
    Widget* widget = nullptr;

    string widgetClass; // from the "Widget <name> <class>" line
    vector<string> tokens; // full token line — tokens[0] == widgetType
    int size = 0; // (int)tokens.size()
    bool suppressWhileTouched = false;
    int echoGuardMs = 0;

    // Pre-parsed three-byte MIDI messages (only valid when size >= 4 / >= 7).
    MIDI_event_ex_t message1 {};
    MIDI_event_ex_t message2 {};

    // Pre-computed lookup keys for MessageGeneratorsByMessage_.
    string oneByteKey; // valid when size >= 4
    string twoByteKey; // valid when size >= 4
    string threeByteKey; // valid when size >= 4
    string threeByteKeyMsg2; // valid when size >= 7
};

// ---------------------------------------------------------------------------
// Handler signature.
// A handler receives the parsed context and is fully responsible for
// inserting message generators and/or feedback processors plus any required
// side-effects (e.g. AddTrackColorFeedbackProcessor, SetHasMCUMeters).
// ---------------------------------------------------------------------------
using MidiWidgetHandler = std::function<void(const MidiWidgetContext&)>;

class MidiWidgetRegistry
{
    static map<string, MidiWidgetHandler>& GetRegistry();

public:
    // Register a handler for a widget-type string (e.g. "Press", "FB_TwoState").
    // Returns true so it can be used as a static-initializer value.
    static bool Register(const string& type, MidiWidgetHandler handler);

    // Dispatch to the registered handler for 'type'.
    // Returns false when the type is unknown (caller may log a warning).
    static bool Dispatch(const string& type, const MidiWidgetContext& ctx);

    // Populate the registry with all built-in widget types.
    // Protected by a static flag so repeated calls are free.
    // Must be called before the first surface file is parsed.
    static void EnsureRegistered();
};
