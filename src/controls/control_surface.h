#pragma once
//
//  control_surface.h — ControlSurface base class
//
#include "preamble.h"
#include "page_interface.h"
#include "zone_manager.h"
#include "modifier_manager.h"
#include "message_generator.h"

class ControlSurface
{
    friend class SurfaceTemplateParser; // surface_parser.h — parses surface template files

private:
    int* scrubModePtr_ = NULL;
    int configScrubMode_ = 0;

    bool isRewinding_ = false;
    bool isFastForwarding_ = false;

    bool isTextLengthRestricted_ = false;
    int restrictedTextLength_ = 6;

    bool usesLocalModifiers_ = false;
    bool listensToModifiers_ = false;

    int latchTime_ = 100;
    int doublePressTime_ = 400;

    bool isOsdEnabled_ = false;
    bool isOskEnabled_ = false;

    // OSK layout data parsed from Surface.txt
    struct OskWidgetInfo {
        string name;
        string shape = "";
        float width = 1.0f;
        float height = 1.0f;
        float top = 0.0f;
        string group;
        string label;
        bool hidden = false;
        string color; // optional default active color (RRGGBB)
        string widgetClass;
        string role;
        string input;
        string feedback;
        string pressTarget;
        string scrollTarget;
        string valueTarget;
        string touchTarget;
        string rotaryStyle;
    };

    struct OskCell {
        bool isSpacer = false;
        float spacerWidth = 0.0f;
        OskWidgetInfo widget;
    };

    struct OskRow {
        vector<OskCell> cells;
    };

    struct ColorCalibrationConfig {
        bool enabled = false;
        int inputMax = 0;
        int outputMax = 0;
        int neutralTolerancePercent = 0;
        float redScale = 1.0f;
        float greenScale = 1.0f;
        float blueScale = 1.0f;
        float neutralRedScale = 1.0f;
        float neutralGreenScale = 1.0f;
        float neutralBlueScale = 1.0f;
        float neutralCurve = 1.0f;
    };

    vector<OskRow> oskLayout_;
    string cachedOskLayoutString_;
    string cachedOskStateString_;
    string cachedOskLabelsString_;
    string cachedOskLabelMapString_;
    string surfaceFilePath_;
    ColorCalibrationConfig colorCalibration_;
    map<string, string> oskConfigZoneNamesByWidget_;
    map<string, string> oskConfigZonePathsByWidget_;
    int oskRunCounter_ = 0;

    void ParseOskProperties(const string& propsPart, OskWidgetInfo& info);
    void ApplyOskWidgetMetadata(OskWidgetInfo& info);
    void ApplyGroupedOskTargets(const vector<OskWidgetInfo>& hiddenWidgets);
    void BuildCachedLayoutString();
    bool GetModifierState(bool (ModifierManager::*getter)());
    void ApplyToBroadcastModifierListeners(void (ModifierManager::*method)());
    void ApplyToBroadcastModifierListeners(void (ModifierManager::*method)(const char*), const char* argument);
    void SetModifier(void (ModifierManager::*setter)(bool, int), bool value);

    vector<FeedbackProcessor*> trackColorFeedbackProcessors_; // does not own pointers

    vector<ChannelTouch> channelTouches_;
    vector<ChannelToggle> channelToggles_;

protected:
    map<const string, double> stepSize_;

    map<const string, map<int, int>> accelerationValuesForDecrement_;
    map<const string, map<int, int>> accelerationValuesForIncrement_;
    map<int, int> emptyAccelerationMap_;

    map<const string, vector<double>> accelerationValues_;
    vector<double> emptyAccelerationValues_;

    void ProcessValues(const vector<vector<string>>& lines);

    CSurfIntegrator* const csi_;
    IPageContext* const page_;
    string const name_;
    unique_ptr<ZoneManager> zoneManager_;
    unique_ptr<ModifierManager> modifierManager_;

    int const numChannels_;
    int const channelOffset_;

    int blinkTimeMs_ = 500;
    int holdTimeMs_ = 1000;
    int osdTimeMs_ = 3000;

    vector<Widget*> widgets_; // owns list
    map<const string, unique_ptr<Widget>> widgetsByName_;
    map<const string, unique_ptr<MessageGenerator>> MessageGeneratorsByMessage_;

    bool speedX5_ = false;

    ControlSurface(CSurfIntegrator* const csi, IPageContext* page, const string& name, int numChannels, int channelOffset)
        : csi_(csi), page_(page), name_(name), numChannels_(numChannels), channelOffset_(channelOffset)
        , modifierManager_(make_unique<ModifierManager>(csi_, nullptr, this)
    ) {
        int size = 0;
        scrubModePtr_ = (int*) get_config_var("scrubmode", &size);

        for (int i = 1; i <= numChannels; ++i) {
            ChannelTouch channelTouch;
            channelTouch.channelNum = i;
            channelTouches_.push_back(channelTouch);

            ChannelToggle channelToggle;
            channelToggle.channelNum = i;
            channelToggles_.push_back(channelToggle);
        }
        LoadOskEnabledSetting();
    }

    void InitZoneManager(CSurfIntegrator* const csi, ControlSurface* surface, const string& zoneFolder, const string& fxZoneFolder) {
        zoneManager_ = make_unique<ZoneManager>(csi_, this, zoneFolder, fxZoneFolder);
        zoneManager_->Initialize();
    }

    void StopRewinding() {
        isRewinding_ = false;
        *scrubModePtr_ = configScrubMode_;

        speedX5_ = false;
    }

    void StopFastForwarding() {
        isFastForwarding_ = false;
        *scrubModePtr_ = configScrubMode_;

        speedX5_ = false;
    }

    void CancelRewindAndFastForward() {
        if (isRewinding_) StopRewinding();
        else if (isFastForwarding_) StopFastForwarding();
    }

    virtual void InitHardwiredWidgets(ControlSurface* surface) {
        for (const std::string& name : Widget::VIRTUAL_TRIGGERS)
            AddWidget(surface, name.c_str());
    }

    void DoWidgetAction(const string& widgetName) {
        if (widgetsByName_.count(widgetName) > 0)
            zoneManager_->DoAction(widgetsByName_[widgetName].get(), 1.0);
    }

public:
    virtual ~ControlSurface() {
        widgets_.clear();
        widgetsByName_.clear();
        MessageGeneratorsByMessage_.clear();
    }

    // Used by widget-type handlers in widget_registrations.cpp to insert message generators.
    void AddMessageGenerator(const string& key, unique_ptr<MessageGenerator> gen) {
        MessageGeneratorsByMessage_.insert(make_pair(key, std::move(gen)));
    }

    void Stop();
    void Play();
    void Record();

    bool GetShift();
    bool GetOption();
    bool GetControl();
    bool GetAlt();
    bool GetFlip();
    bool GetGlobal();
    bool GetMarker();
    bool GetNudge();
    bool GetZoom();
    bool GetScrub();

    void SetModifierValue(int value);
    void SetShift(bool value);
    void SetOption(bool value);
    void SetControl(bool value);
    void SetAlt(bool value);
    void SetFlip(bool value);
    void SetGlobal(bool value);
    void SetMarker(bool value);
    void SetNudge(bool value);
    void SetZoom(bool value);
    void SetScrub(bool value);

    const vector<int>& GetModifiers();
    void ClearModifiers();
    void ClearModifier(const char* modifier);

    virtual void RequestUpdate();
    void ForceClearTrack(int trackNum);
    void ForceUpdateTrackColors();
    void OnTrackSelection(MediaTrack* track);
    virtual void SendOSCMessage(const char* zoneName) {}
    virtual void SendOSCMessage(const char* zoneName, int value) {}
    virtual void SendOSCMessage(const char* zoneName, double value) {}
    virtual void SendOSCMessage(const char* zoneName, const char* value) {}

    virtual void HandleExternalInput() {}
    virtual void UpdateTimeDisplay() {}
    virtual void FlushIO() {}

    virtual void SendMidiSysExMessage(MIDI_event_ex_t* midiMessage) {}
    virtual void SendMidiMessage(int first, int second, int third) {}

    ModifierManager* GetModifierManager() { return modifierManager_.get(); }
    ZoneManager* GetZoneManager() { return zoneManager_.get(); }
    IPageContext* GetPage() { return page_; }
    const char* GetName() { return name_.c_str(); }

    int GetNumChannels() { return numChannels_; }
    int GetChannelOffset() { return channelOffset_; }
    rgba_color GetTrackColorForChannel(int channel);
    rgba_color GetDeviceFeedbackColor(const rgba_color& color, int defaultOutputMax = 255, float brightnessScale = 1.0f) const;

    bool GetIsRewinding() { return isRewinding_; }
    bool GetIsFastForwarding() { return isFastForwarding_; }

    bool GetUsesLocalModifiers() { return usesLocalModifiers_; }
    void ToggleUseLocalModifiers() { usesLocalModifiers_ = !usesLocalModifiers_; }
    bool GetListensToModifiers() { return listensToModifiers_; }
    void SetListensToModifiers() { listensToModifiers_ = true; }

    void SetLatchTime(int value) { latchTime_ = value; }
    int GetLatchTime() { return latchTime_; }

    void SetHoldTime(int value) { holdTimeMs_ = value; }
    int GetHoldTime() { return holdTimeMs_; }

    void SetDoublePressTime(int value) { doublePressTime_ = value; }
    int GetDoublePressTime() { return doublePressTime_; }

    void SetBlinkTime(int value) { blinkTimeMs_ = value; }
    int GetBlinkTime() { return blinkTimeMs_; }

    // Global blink phase: deterministic, all buttons with the same interval blink in sync.
    // Returns true during the "lit" half of the cycle, false during the "dim" half.
    bool IsBlinkLit(int intervalMs) {
        if (intervalMs <= 0) intervalMs = blinkTimeMs_;
        return (GetTickCount() / static_cast<DWORD>(intervalMs)) % 2 == 0;
    }

    void SetOSDTime(int value) { osdTimeMs_ = value; }
    int GetOSDTime() { return osdTimeMs_; }

    void UpdateCurrentActionContextModifiers() { if (!usesLocalModifiers_) GetZoneManager()->UpdateCurrentActionContextModifiers(); }

    double GetStepSize(const char* const widgetClass) {
        if (stepSize_.find(widgetClass) != stepSize_.end())
            return stepSize_[widgetClass];
        else
            return 0;
    }

    const vector<double> GetAccelerationValues(const char* const widgetClass) {
        if (accelerationValues_.find(widgetClass) != accelerationValues_.end())
            return accelerationValues_[widgetClass];
        else
            return emptyAccelerationValues_;
    }

    map<int, int>& GetAccelerationValuesForDecrement(const char* const widgetClass) {
        if (accelerationValuesForDecrement_.count(widgetClass) > 0)
            return accelerationValuesForDecrement_[widgetClass];
        else
            return emptyAccelerationMap_;
    }

    map<int, int>& GetAccelerationValuesForIncrement(const char* const widgetClass) {
        if (accelerationValuesForIncrement_.count(widgetClass) > 0)
            return accelerationValuesForIncrement_[widgetClass];
        else
            return emptyAccelerationMap_;
    }

    void TouchChannel(int channelNum, bool isTouched) {
        for (auto& channelTouch : channelTouches_)
            if (channelTouch.channelNum == channelNum) {
                channelTouch.isTouched = isTouched;
                break;
            }
    }

    bool GetIsChannelTouched(int channelNum) {
        for (auto& channelTouch : channelTouches_)
            if (channelTouch.channelNum == channelNum)
                return channelTouch.isTouched;
        return false;
    }

    void ToggleChannel(int channelNum) {
        for (auto& channelToggle : channelToggles_)
            if (channelToggle.channelNum == channelNum) {
                channelToggle.isToggled = !channelToggle.isToggled;
                break;
            }
    }

    bool GetIsChannelToggled(int channelNum) {
        for (auto& channelToggle : channelToggles_)
            if (channelToggle.channelNum == channelNum)
                return channelToggle.isToggled;

        return false;
    }

    void ToggleRestrictTextLength(int length) {
        isTextLengthRestricted_ = !isTextLengthRestricted_;
        restrictedTextLength_ = length;
    }

    const char* GetRestrictedLengthText(const char* textc, char* buf, int bufsz) { //TODO: review, may return textc if not restricted
        if (isTextLengthRestricted_ && strlen(textc) > restrictedTextLength_ && restrictedTextLength_ >= 0) {
            static const char* const filter_lists[3] = {
                " \t\r\n",
                " \t\r\n`~!@#$%^&*:()_|=?;:'\",",
                " \t\r\n`~!@#$%^&*:()_|=?;:'\",aeiou"
            };

            for (int pass = 0; pass < 3; ++pass) {
                const char* rd = textc;
                int l = 0;
                while (*rd && l < bufsz - 1 && l <= restrictedTextLength_) {
                    if (!l || !strchr(filter_lists[pass], *rd)) buf[l++] = *rd;
                    rd++;
                }
                if (pass < 2 && l > restrictedTextLength_) continue; // keep filtering

                buf[wdl_min(restrictedTextLength_, l)] = 0;
                return buf;
            }
        }
        return textc;
    }

    void AddTrackColorFeedbackProcessor(FeedbackProcessor* feedbackProcessor) { //TODO: review does not own this pointer
        if (feedbackProcessor != NULL)
            trackColorFeedbackProcessors_.push_back(feedbackProcessor);
    }

    void ForceClear() {
        for (auto widget : widgets_)
            widget->ForceClear();
        FlushIO();
    }

    void TrackFXListChanged(MediaTrack* track) { OnTrackSelection(track); }

    void HandleStop() {
        DoWidgetAction("OnRecordStop");
        DoWidgetAction("OnPlayStop");
    }

    void HandlePlay() { DoWidgetAction("OnPlayStart"); }
    void HandleRecord() { DoWidgetAction("OnRecordStart"); }
    void StartRewinding() {
        if (isFastForwarding_) StopFastForwarding();

        if (isRewinding_) {
            speedX5_ = !speedX5_; // on 2nd, 3rd, etc. press
            return;
        }
        int playState = GetPlayState();
        if (playState == PLAYSTATE_PLAYING || playState == PLAYSTATE_PAUSED || playState == PLAYSTATE_RECORDING || playState == PLAYSTATE_PAUSED_WHILE_RECORDING)
            SetEditCurPos(GetPlayPosition(), true, false);

        CSurf_OnStop();
        isRewinding_ = true;
        configScrubMode_ = *scrubModePtr_;
        *scrubModePtr_ = 2;
    }

    void StartFastForwarding() {
        if (isRewinding_) StopRewinding();

        if (isFastForwarding_) {
            speedX5_ = !speedX5_; // on 2nd, 3rd, etc. press
            return;
        }

        int playState = GetPlayState();
        if (playState == PLAYSTATE_PLAYING || playState == PLAYSTATE_PAUSED || playState == PLAYSTATE_RECORDING || playState == PLAYSTATE_PAUSED_WHILE_RECORDING)
            SetEditCurPos(GetPlayPosition(), true, false);

        CSurf_OnStop();

        isFastForwarding_ = true;
        configScrubMode_ = *scrubModePtr_;
        *scrubModePtr_ = 2;
    }

    void AddWidget(ControlSurface* surface, const char* widgetName) {
        if (widgetsByName_.count(string(widgetName)) == 0) {
            widgetsByName_.insert(make_pair(widgetName, make_unique<Widget>(csi_, surface, widgetName)));
            if (widgetsByName_.count(widgetName) > 0)
                widgets_.push_back(GetWidgetByName(widgetName));
        }
    }

    Widget* GetWidgetByName(const string& widgetName) {
        if (widgetsByName_.count(widgetName.c_str()) > 0) return widgetsByName_[widgetName].get();
        else return NULL;
    }

    void OnPageEnter() {
        ForceClear();
        DoWidgetAction("OnPageEnter");
    }

    void OnPageLeave() {
        ForceClear();
        DoWidgetAction("OnPageLeave");
    }

    void OnInitialization() { DoWidgetAction("OnInitialization"); }
    bool IsOsdEnabled() { return isOsdEnabled_; }
    void SetOsdEnabled(bool value) { isOsdEnabled_ = value; }

    bool GetOskEnabled() const { return isOskEnabled_; }
    void LoadOskEnabledSetting();
    void SetOskEnabled(bool value);
    void ParseOSKLayout(const string& surfaceFilePath);
    void PublishOSKLayout();
    void PublishOSKState();
    void PublishOSKLabels();
    void PublishOSKLabelMap();
    const string& GetSurfaceFilePath() const { return surfaceFilePath_; }

    void InjectOSKPressDown(const string& widgetName);
    void InjectOSKPressUp(const string& widgetName);
    void InjectOSKScroll(const string& widgetName, int accelerationIndex, double delta, int eventCount);
    void InjectOSKValue(const string& widgetName, double value);
    void InjectOSKTouch(const string& widgetName, double value);
    void HandleOSKConfigQuery(const string& widgetName);
    void HandleOSKConfigApplyLive(const string& widgetName, const string& bindingData);
    void HandleOSKConfigSave(const string& widgetName);
    void HandleOSKConfigRevert(const string& widgetName);
    void HandleOSKZoneCreate(const string& scaffoldType, const string& zoneName, const string& alias, const string& navigator);
};
