#pragma once

#include <array>

class Format2MidiRgbFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    bool hasEnable_ = false;
    std::array<int, 3> enable_{};
    std::array<int, 2> red_{};
    std::array<int, 2> green_{};
    std::array<int, 2> blue_{};
    bool hasStateBrightness_ = false;
    bool active_ = false;
    float inactiveBrightness_ = 1.0f;
    float activeBrightness_ = 1.0f;
    rgba_color sourceColor_;

    void SendResolvedColor() {
        const float brightness = this->hasStateBrightness_ ? (this->active_ ? this->activeBrightness_ : this->inactiveBrightness_) : 1.0f;
        const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(this->sourceColor_, 255, brightness);
        if (this->hasEnable_) this->SendMidiMessage(this->enable_[0], this->enable_[1], this->enable_[2]);
        this->SendMidiMessage(this->red_[0], this->red_[1], deviceColor.r);
        this->SendMidiMessage(this->green_[0], this->green_[1], deviceColor.g);
        this->SendMidiMessage(this->blue_[0], this->blue_[1], deviceColor.b);
    }

public:
    Format2MidiRgbFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const std::array<int, 2>& red, const std::array<int, 2>& green, const std::array<int, 2>& blue, const vector<int>& enable, bool hasStateBrightness, float inactiveBrightness, float activeBrightness)
        : Midi_FeedbackProcessor(csi, surface, widget), red_(red), green_(green), blue_(blue), hasStateBrightness_(hasStateBrightness), inactiveBrightness_(inactiveBrightness), activeBrightness_(activeBrightness) {
        if (enable.size() == 3) {
            this->hasEnable_ = true;
            this->enable_ = { enable[0], enable[1], enable[2] };
        }
    }

    virtual ~Format2MidiRgbFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiRgbFeedbackProcessor"; }

    virtual void ForceClear() override {
        this->sourceColor_ = rgba_color();
        this->lastColor_ = this->sourceColor_;
        this->SendResolvedColor();
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (!this->hasStateBrightness_) return;
        const bool active = value != 0.0;
        if (active == this->active_) return;
        this->active_ = active;
        this->lastDoubleValue_ = value;
        this->SendResolvedColor();
    }

    virtual void SetColorValue(const rgba_color& color) override {
        if (color == this->lastColor_) return;
        this->sourceColor_ = color;
        this->lastColor_ = color;
        this->SendResolvedColor();
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        this->sourceColor_ = color;
        this->lastColor_ = color;
        this->SendResolvedColor();
    }
};
