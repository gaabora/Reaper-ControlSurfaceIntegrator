#include "integrator.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Widget
////////////////////////////////////////////////////////////////////////////////////////////////////////
ZoneManager* Widget::GetZoneManager() { return surface_->GetZoneManager(); }

void Widget::Configure(const vector<unique_ptr<ActionContext>>& contexts) {
    for (auto& feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->Configure(contexts);
}

void Widget::UpdateValue(const PropertyList& properties, double value) {
    this->lastFeedbackValue_ = value;
    for (auto& feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetValue(properties, value);
}

void Widget::UpdateValue(const PropertyList& properties, const char* const& value) {
    for (auto& feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetValue(properties, value);
}

void Widget::ForceValue(const PropertyList& properties, const char* const& value) {
    for (auto& feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->ForceValue(properties, value);
}

void Widget::UpdateColorValue(const rgba_color& color) {
    this->lastFeedbackColor_ = color;
    for (auto& feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetColorValue(color);
}

void Widget::SetXTouchDisplayColors(const char* colors) {
    for (auto& feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->SetXTouchDisplayColors(colors);
}

void Widget::RestoreXTouchDisplayColors() {
    for (auto& feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->RestoreXTouchDisplayColors();
}

void Widget::ForceClear() {
    for (auto& feedbackProcessor : feedbackProcessors_)
        feedbackProcessor->ForceClear();
}

void Widget::LogInput(double value) {
    if (g_surfaceInDisplay) LogToConsole("[DEBUG] wIN <- %s %s %f\n", GetSurface()->GetName(), GetName(), value);
}
