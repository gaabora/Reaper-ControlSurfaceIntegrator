#include "integrator.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_FeedbackProcessor::SendMidiSysExMessage(MIDI_event_ex_t* midiMessage) {
    surface_->SendMidiSysExMessage(midiMessage);
}

void Midi_FeedbackProcessor::SendMidiMessage(int first, int second, int third) {
    if (first != lastMessageSent_.midi_message[0] || second != lastMessageSent_.midi_message[1] || third != lastMessageSent_.midi_message[2]) {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%02x %02x %02x", first, second, third);
        this->LogMessage(buffer);
        ForceMidiMessage(first, second, third);
    }
}

void Midi_FeedbackProcessor::ForceMidiMessage(int first, int second, int third) {
    lastMessageSent_.midi_message[0] = first;
    lastMessageSent_.midi_message[1] = second;
    lastMessageSent_.midi_message[2] = third;
    surface_->SendMidiMessage(first, second, third);
}

void Midi_FeedbackProcessor::LogMessage(char* value) {
    if (g_surfaceOutDisplay) LogToConsole("[DEBUG] @S:'%s' [W:'%s'] MIDI: %s\n", surface_->GetName(), widget_->GetName(), value);
}
