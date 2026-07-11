#include "integrator.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_FeedbackProcessor::SendMidiSysExMessage(MIDI_event_ex_t* midiMessage) {
    surface_->SendMidiSysExMessage(midiMessage);
}

void Midi_FeedbackProcessor::SendMidiMessage(int first, int second, int third) {
    bool updateMeters = surface_->GetHasMCUMeters() && first == 0xd0; // MUST UPDATE METERS REGARDLESS OF LAST MESSAGE SENT AS METERS ON THE WILL DECAY TO OFF IF NOT UPDATE REGULARLY

    if (updateMeters || first != lastMessageSent_.midi_message[0] || second != lastMessageSent_.midi_message[1] || third != lastMessageSent_.midi_message[2]) {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%02x %02x %02x", first, second, third);

        if (updateMeters) snprintf(buffer, sizeof(buffer), "%02x %02x", first, second);

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
    if (g_surfaceOutDisplay) LogToConsole("@S:'%s' [W:'%s'] MIDI: %s\n", surface_->GetName(), widget_->GetName(), value);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_FeedbackProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////
void OSC_FeedbackProcessor::SetColorValue(const rgba_color& color) {
    if (lastColor_ != color) {
        lastColor_ = color;
        char tmp[32];
        surface_->SendOSCMessage(this, (oscAddress_ + "/Color").c_str(), color.rgba_to_string(tmp));
    }
}

void OSC_FeedbackProcessor::ForceValue(const PropertyList& properties, double value) {
    if ((GetTickCount() - GetWidget()->GetLastIncomingMessageTime()) < 50) return; //FIXME: hardcoded to const or setting.  adjust the 50 millisecond value to give you smooth behaviour without making updates sluggish  
    lastDoubleValue_ = value;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), value);
}

void OSC_FeedbackProcessor::ForceValue(const PropertyList& properties, const char* const& value) {
    lastStringValue_ = value;
    char tmp[MEDBUF];
    surface_->SendOSCMessage(this, oscAddress_.c_str(), GetWidget()->GetSurface()->GetRestrictedLengthText(value, tmp, sizeof(tmp)));
}

void OSC_FeedbackProcessor::ForceClear() {
    lastDoubleValue_ = 0.0;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), 0.0);
    lastStringValue_ = "";
    surface_->SendOSCMessage(this, oscAddress_.c_str(), "");
}

void OSC_IntFeedbackProcessor::ForceClear() {
    lastDoubleValue_ = 0.0;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), (int) 0);
}

void OSC_IntFeedbackProcessor::ForceValue(const PropertyList& properties, double value) {
    lastDoubleValue_ = value;
    surface_->SendOSCMessage(this, oscAddress_.c_str(), (int) value);
}
