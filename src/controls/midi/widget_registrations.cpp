// widget_registrations.cpp — MidiWidgetRegistry implementation + built-in widget type registrations.
// This file owns:
//    1. The registry storage and dispatch logic (GetRegistry, Register, Dispatch).
//    2. EnsureRegistered()  — registers every built-in widget type once.
//
//  To add a new widget type, call MidiWidgetRegistry::Register() with a
//  handler lambda inside EnsureRegistered().  No other file needs to change.

#include "../integrator.h"
#include "widget_factory.h"
#include "midi_surface.h"
#include "midi_widgets.h"

// ---------------------------------------------------------------------------
// Registry storage and core dispatch
// ---------------------------------------------------------------------------

map<string, MidiWidgetHandler>& MidiWidgetRegistry::GetRegistry() {
    static map<string, MidiWidgetHandler> s_registry;
    return s_registry;
}

bool MidiWidgetRegistry::Register(const string& type, MidiWidgetHandler handler) {
    GetRegistry()[type] = std::move(handler);
    return true;
}

bool MidiWidgetRegistry::Dispatch(const string& type, const MidiWidgetContext& ctx) {
    auto it = GetRegistry().find(type);
    if (it == GetRegistry().end())
        return false;
    it->second(ctx);
    return true;
}

// ---------------------------------------------------------------------------
// Built-in registrations
// ---------------------------------------------------------------------------

void MidiWidgetRegistry::EnsureRegistered() {
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;

    // ------------------------------------------------------------------
    // Message Generators
    // ------------------------------------------------------------------

    Register("AnyPress", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4 || ctx.size == 7)
            ctx.surface->AddMessageGenerator(ctx.twoByteKey, make_unique<AnyPress_Midi_MessageGenerator>(ctx.csi, ctx.widget));
    });

    Register("Press", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) {
            ctx.surface->AddMessageGenerator(ctx.threeByteKey, make_unique<PressRelease_Midi_MessageGenerator>(ctx.csi, ctx.widget, ctx.message1));
        } else if (ctx.size == 7) {
            ctx.surface->AddMessageGenerator(ctx.threeByteKey, make_unique<PressRelease_Midi_MessageGenerator>(ctx.csi, ctx.widget, ctx.message1, ctx.message2));
            ctx.surface->AddMessageGenerator(ctx.threeByteKeyMsg2, make_unique<PressRelease_Midi_MessageGenerator>(ctx.csi, ctx.widget, ctx.message1, ctx.message2));
        }
    });

    Register("Fader14Bit", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.surface->AddMessageGenerator(ctx.oneByteKey, make_unique<Fader14Bit_Midi_MessageGenerator>(ctx.csi, ctx.widget));
    });

    Register("FaderportClassicFader14Bit", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 7) ctx.surface->AddMessageGenerator(ctx.oneByteKey, make_unique<FaderportClassicFader14Bit_Midi_MessageGenerator>(ctx.csi, ctx.widget, ctx.message1, ctx.message2));
    });

    Register("Fader7Bit", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.surface->AddMessageGenerator(ctx.twoByteKey, make_unique<Fader7Bit_Midi_MessageGenerator>(ctx.csi, ctx.widget));
    });

    Register("Encoder", [](const MidiWidgetContext& ctx) {
        if (ctx.widgetClass == "RotaryWidgetClass")
            ctx.surface->AddMessageGenerator(ctx.twoByteKey, make_unique<AcceleratedPreconfiguredEncoder_Midi_MessageGenerator>(ctx.csi, ctx.widget));
        else if (ctx.size == 4) ctx.surface->AddMessageGenerator(ctx.twoByteKey, make_unique<Encoder_Midi_MessageGenerator>(ctx.csi, ctx.widget));
    });

    Register("MFTEncoder", [](const MidiWidgetContext& ctx) {
        if (ctx.size > 4) {
            vector<string> tokens = ctx.tokens; // constructor takes non-const ref
            ctx.surface->AddMessageGenerator(ctx.twoByteKey, make_unique<MFT_AcceleratedEncoder_Midi_MessageGenerator>(ctx.csi, ctx.widget, tokens));
        }
    });

    Register("EncoderPlain", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.surface->AddMessageGenerator(ctx.twoByteKey, make_unique<EncoderPlain_Midi_MessageGenerator>(ctx.csi, ctx.widget));
    });

    Register("Encoder7Bit", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.surface->AddMessageGenerator(ctx.twoByteKey, make_unique<Encoder7Bit_Midi_MessageGenerator>(ctx.csi, ctx.widget));
    });

    Register("Touch", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 7) {
            ctx.surface->AddMessageGenerator(ctx.threeByteKey, make_unique<Touch_Midi_MessageGenerator>(ctx.csi, ctx.widget, ctx.message1, ctx.message2));
            ctx.surface->AddMessageGenerator(ctx.threeByteKeyMsg2, make_unique<Touch_Midi_MessageGenerator>(ctx.csi, ctx.widget, ctx.message1, ctx.message2));
        }
    });

    // ------------------------------------------------------------------
    // Feedback Processors
    // ------------------------------------------------------------------

    Register("FB_TwoState", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 7) ctx.widget->GetFeedbackProcessors().push_back( make_unique<TwoState_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1, ctx.message2));
    });

    Register("FB_NovationLaunchpadMiniRGB7Bit", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_MFT_RGB", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<MFT_RGB_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_AsparionRGB", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<AsparionRGB_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
            ctx.surface->AddTrackColorFeedbackProcessor(ctx.widget->GetFeedbackProcessors().back().get());
        }
    });

    Register("FB_FaderportRGB", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FaderportRGB_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_FaderportTwoStateRGB", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPTwoStateRGB_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_FaderportValueBar", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPValueBar_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_FPVUMeter", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPVUMeter_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_Fader14Bit", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<Fader14Bit_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_FaderportClassicFader14Bit", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 7) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FaderportClassicFader14Bit_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1, ctx.message2));
    });

    Register("FB_Fader7Bit", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<Fader7Bit_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_Encoder", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<Encoder_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_AsparionEncoder", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<AsparionEncoder_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_ConsoleOneVUMeter", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<ConsoleOneVUMeter_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_ConsoleOneGainReductionMeter", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<ConsoleOneGainReductionMeter_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_MCUTimeDisplay", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 1)
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCU_TimeDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget));
    });

    Register("FB_MCUAssignmentDisplay", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 1)
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<FB_MCU_AssignmentDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget));
    });

    Register("FB_QConProXMasterVUMeter", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<QConProXMasterVUMeter_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_MCUVUMeter", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCUVUMeter_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x14, atoi(ctx.tokens[1].c_str())));
            ctx.surface->SetHasMCUMeters(0x14);
        }
    });

    Register("FB_MCUXTVUMeter", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCUVUMeter_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x15, atoi(ctx.tokens[1].c_str())));
            ctx.surface->SetHasMCUMeters(0x15);
        }
    });

    Register("FB_AsparionVUMeterL", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<AsparionVUMeter_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x14, atoi(ctx.tokens[1].c_str()), false));
            ctx.surface->SetHasMCUMeters(0x14);
        }
    });

    Register("FB_AsparionVUMeterR", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<AsparionVUMeter_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x14, atoi(ctx.tokens[1].c_str()), true));
            ctx.surface->SetHasMCUMeters(0x14);
        }
    });

    // SCE24 — note: LEDButton and OLEDButton build a custom MIDI event (byte[1] += 0x60)
    // SCE24LEDButton / SCE24OLEDButton: byte[1] of the pre-parsed message1 needs a +0x60 offset.
    Register("FB_SCE24LEDButton", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) {
            MIDI_event_ex_t midiEvent = ctx.message1;
            midiEvent.midi_message[1] += 0x60;
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<SCE24TwoStateLED_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, midiEvent));
        }
    });

    Register("FB_SCE24OLEDButton", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 7) {
            MIDI_event_ex_t midiEvent = ctx.message1;
            midiEvent.midi_message[1] += 0x60;
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<SCE24OLED_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, midiEvent, atoi(ctx.tokens[4].c_str()), atoi(ctx.tokens[5].c_str()), atoi(ctx.tokens[6].c_str())));
        }
    });

    Register("FB_SCE24Encoder", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 4) ctx.widget->GetFeedbackProcessors().push_back( make_unique<SCE24Encoder_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1));
    });

    Register("FB_SCE24EncoderText", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 7) ctx.widget->GetFeedbackProcessors().push_back( make_unique<SCE24Text_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, ctx.message1, atoi(ctx.tokens[4].c_str()), atoi(ctx.tokens[5].c_str()), atoi(ctx.tokens[6].c_str())));
    });

    // MCU display family
    Register("FB_MCUDisplayUpper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCUDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0, 0x14, 0x12, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_MCUDisplayLower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCUDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 1, 0x14, 0x12, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_MCUXTDisplayUpper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCUDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0, 0x15, 0x12, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_MCUXTDisplayLower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCUDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 1, 0x15, 0x12, atoi(ctx.tokens[1].c_str())));
    });

    // Icon display family
    Register("FB_IconDisplay1Upper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<IconDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0, 0x14, 0x12, atoi(ctx.tokens[1].c_str()), 0x00, 0x66));
    });

    Register("FB_IconDisplay1Lower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<IconDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 1, 0x14, 0x12, atoi(ctx.tokens[1].c_str()), 0x00, 0x66));
    });

    Register("FB_IconDisplay2Upper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<IconDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0, 0x15, 0x13, atoi(ctx.tokens[1].c_str()), 0x02, 0x4e));
    });

    Register("FB_IconDisplay2Lower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<IconDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 1, 0x15, 0x13, atoi(ctx.tokens[1].c_str()), 0x02, 0x4e));
    });

    // Asparion display family
    Register("FB_AsparionDisplayUpper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<AsparionDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x01, 0x14, 0x1A, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_AsparionDisplayLower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<AsparionDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x02, 0x14, 0x1A, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_AsparionDisplayEncoder", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<AsparionDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x03, 0x14, 0x19, atoi(ctx.tokens[1].c_str())));
    });

    // X-Touch display family (also registers track-colour feedback processor)
    Register("FB_XTouchDisplayUpper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<XTouchDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0, 0x14, 0x12, atoi(ctx.tokens[1].c_str())));
            ctx.surface->AddTrackColorFeedbackProcessor(ctx.widget->GetFeedbackProcessors().back().get());
        }
    });

    Register("FB_XTouchDisplayLower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<XTouchDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 1, 0x14, 0x12, atoi(ctx.tokens[1].c_str())));
            ctx.surface->AddTrackColorFeedbackProcessor(ctx.widget->GetFeedbackProcessors().back().get());
        }
    });

    Register("FB_XTouchXTDisplayUpper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<XTouchDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0, 0x15, 0x12, atoi(ctx.tokens[1].c_str())));
            ctx.surface->AddTrackColorFeedbackProcessor(ctx.widget->GetFeedbackProcessors().back().get());
        }
    });

    Register("FB_XTouchXTDisplayLower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) {
            ctx.widget->GetFeedbackProcessors().push_back( make_unique<XTouchDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 1, 0x15, 0x12, atoi(ctx.tokens[1].c_str())));
            ctx.surface->AddTrackColorFeedbackProcessor(ctx.widget->GetFeedbackProcessors().back().get());
        }
    });

    // C4 display family
    Register("FB_C4DisplayUpper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 3) ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCUDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0, 0x17, atoi(ctx.tokens[1].c_str()) + 0x30, atoi(ctx.tokens[2].c_str())));
    });

    Register("FB_C4DisplayLower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 3) ctx.widget->GetFeedbackProcessors().push_back( make_unique<MCUDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 1, 0x17, atoi(ctx.tokens[1].c_str()) + 0x30, atoi(ctx.tokens[2].c_str())));
    });

    // Faderport scribble strip lines
    Register("FB_FP8ScribbleLine1", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x02, atoi(ctx.tokens[1].c_str()), 0x00));
    });

    Register("FB_FP8ScribbleLine2", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x02, atoi(ctx.tokens[1].c_str()), 0x01));
    });

    Register("FB_FP8ScribbleLine3", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x02, atoi(ctx.tokens[1].c_str()), 0x02));
    });

    Register("FB_FP8ScribbleLine4", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x02, atoi(ctx.tokens[1].c_str()), 0x03));
    });

    Register("FB_FP16ScribbleLine1", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x16, atoi(ctx.tokens[1].c_str()), 0x00));
    });

    Register("FB_FP16ScribbleLine2", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x16, atoi(ctx.tokens[1].c_str()), 0x01));
    });

    Register("FB_FP16ScribbleLine3", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x16, atoi(ctx.tokens[1].c_str()), 0x02));
    });

    Register("FB_FP16ScribbleLine4", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x16, atoi(ctx.tokens[1].c_str()), 0x03));
    });

    // Faderport scribble strip mode
    Register("FB_FP8ScribbleStripMode", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPScribbleStripMode_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x02, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_FP16ScribbleStripMode", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<FPScribbleStripMode_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0x16, atoi(ctx.tokens[1].c_str())));
    });

    // QCon Lite display family
    Register("FB_QConLiteDisplayUpper", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<QConLiteDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 0, 0x14, 0x12, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_QConLiteDisplayUpperMid", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<QConLiteDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 1, 0x14, 0x12, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_QConLiteDisplayLowerMid", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<QConLiteDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 2, 0x14, 0x12, atoi(ctx.tokens[1].c_str())));
    });

    Register("FB_QConLiteDisplayLower", [](const MidiWidgetContext& ctx) {
        if (ctx.size == 2) ctx.widget->GetFeedbackProcessors().push_back( make_unique<QConLiteDisplay_Midi_FeedbackProcessor>(ctx.csi, ctx.surface, ctx.widget, 3, 0x14, 0x12, atoi(ctx.tokens[1].c_str())));
    });
}
