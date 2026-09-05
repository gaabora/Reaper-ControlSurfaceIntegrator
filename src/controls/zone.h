#pragma once
// zone.h — Zone, SubZone, ZoneInfo
#include "preamble.h"
#include "../actions/action_context.h"

enum class ZoneRuntimeTarget {
    Legacy,
    Global,
    Tracks,
    SelectedTrack,
    MasterTrack,
    FocusedFx,
    Vca,
    Folder,
    SelectedTracks,
};

class Zone
{
protected:
    ZoneManager* const zoneManager_;
    CSurfIntegrator* const csi_;
    Navigator* navigator_;
    int slotIndex_;
    string const name_;
    string const alias_;
    string const sourceFilePath_;

    bool isActive_ = false;
    bool usesExactEventFallback_ = false;
    bool deactivatesOnTrackLoss_ = false;
    Zone* parentZone_ = nullptr;
    ZoneRuntimeTarget runtimeTarget_ = ZoneRuntimeTarget::Legacy;

    // these do not own the widgets, ultimately the ControlSurface contains the list of widgets
    vector<Widget*> widgets_;

    vector<unique_ptr<ActionContext>> emptyContexts_;
    map<Widget*, int> currentActionContextModifiers_;
    map<Widget*, map<int, vector<unique_ptr<ActionContext>>>> actionContextDictionary_;

    vector<unique_ptr<Zone>> includedZones_;
    vector<unique_ptr<Zone>> zoneLayers_;
    vector<unique_ptr<Zone>> subZones_;

    void UpdateCurrentActionContextModifier(Widget* widget);
    bool UsesWidgetForCurrentEvent(Widget* widget);
    bool UsesWidgetForCurrentFeedback(Widget* widget);
    bool MatchesRuntimeTarget(ZoneRuntimeTarget target, const char* legacyName) const;

public:
    Zone(CSurfIntegrator* const csi, ZoneManager* const zoneManager, Navigator* navigator, int slotIndex, const string& name, const string& alias, const string& sourceFilePath)
        : csi_(csi), zoneManager_(zoneManager), navigator_(navigator), slotIndex_(slotIndex), name_(name), alias_(alias), sourceFilePath_(sourceFilePath) {}

    virtual ~Zone() {
        includedZones_.clear();
        zoneLayers_.clear();
        subZones_.clear();
        actionContextDictionary_.clear();
    }

    void InitSubZones(const vector<string>& subZones, const char* widgetSuffix);
    int GetSlotIndex();
    void SetXTouchDisplayColors(const char* colors);
    void RestoreXTouchDisplayColors();
    void UpdateCurrentActionContextModifiers();

    const vector<unique_ptr<ActionContext>>& GetActionContexts(Widget* widget);
    ActionContext* AddActionContext(Widget* widget, int modifier, Zone* zone, const char* actionName, vector<string>& params, Navigator* navigator = nullptr, int slotIndexOverride = -1);
    void ClearActionContexts(Widget* widget);

    void AddWidget(Widget* widget);
    void Activate();
    void Deactivate();
    void DoAction(Widget* widget, bool& isUsed, double value);
    void DoRelativeAction(Widget* widget, bool& isUsed, double delta);
    void DoRelativeAction(Widget* widget, bool& isUsed, int accelerationIndex, double delta);
    void DoTouch(Widget* widget, const char* widgetName, bool& isUsed, double value);
    void RequestUpdate();
    const vector<Widget*>& GetWidgets() { return widgets_; }

    const char* GetSourceFilePath() { return sourceFilePath_.c_str(); }
    vector<unique_ptr<Zone>>& GetIncludedZones() { return includedZones_; }
    vector<unique_ptr<Zone>>& GetZoneLayers() { return zoneLayers_; }
    void AddIncludedZone(unique_ptr<Zone> zone) { this->includedZones_.push_back(std::move(zone)); }
    void AddZoneLayer(unique_ptr<Zone> zone) { this->zoneLayers_.push_back(std::move(zone)); }

    Navigator* GetNavigator() { return navigator_; }
    void SetNavigator(Navigator* navigator) { navigator_ = navigator; }
    void SetSlotIndex(int index) { slotIndex_ = index; }
    void ConfigureFormat2Runtime(ZoneRuntimeTarget target, bool deactivatesOnTrackLoss = false, Zone* parentZone = nullptr) {
        this->runtimeTarget_ = target;
        this->deactivatesOnTrackLoss_ = deactivatesOnTrackLoss;
        this->parentZone_ = parentZone;
        this->usesExactEventFallback_ = true;
    }
    bool GetIsActive() { return isActive_; }
    ZoneRuntimeTarget GetRuntimeTarget() const { return this->runtimeTarget_; }
    bool DeactivatesOnTrackLoss() const { return this->deactivatesOnTrackLoss_; }
    bool UsesPageActivation() const { return this->runtimeTarget_ == ZoneRuntimeTarget::Vca || this->runtimeTarget_ == ZoneRuntimeTarget::Folder || this->runtimeTarget_ == ZoneRuntimeTarget::SelectedTracks; }

    void Toggle() {
        if (isActive_) Deactivate();
        else Activate();
    }

    const char* GetName() { return name_.c_str(); }

    const char* GetAlias() { return (alias_.size() > 0) ? alias_.c_str() : name_.c_str(); }

    const vector<unique_ptr<ActionContext>>& GetActionContexts(Widget* widget, int modifier) {
        if (actionContextDictionary_.count(widget) > 0 && actionContextDictionary_[widget].count(modifier) > 0)
            return actionContextDictionary_[widget][modifier];
        else
            return emptyContexts_;
    }

    // Fills out with all (modifier → &contexts) entries defined for this widget in this zone.
    // Returns true if the widget was found with at least one non-empty modifier entry.
    bool GetAllModifierContexts(Widget* widget, map<int, const vector<unique_ptr<ActionContext>>*>& out) {
        auto it = actionContextDictionary_.find(widget);
        if (it == actionContextDictionary_.end()) return false;
        bool found = false;
        for (auto& [mod, ctxs] : it->second) {
            if (!ctxs.empty()) {
                out[mod] = &ctxs;
                found = true;
            }
        }
        return found;
    }

    void OnTrackDeselection() {
        if (this->runtimeTarget_ == ZoneRuntimeTarget::Legacy) {
            this->isActive_ = true;
            for (auto& includedZone : this->includedZones_) includedZone->Activate();
            return;
        }
        if (this->deactivatesOnTrackLoss_) {
            this->Deactivate();
            return;
        }
        isActive_ = true;
        for (auto& includedZone : includedZones_) includedZone->OnTrackDeselection();
        for (auto& zoneLayer : this->zoneLayers_) if (zoneLayer->GetIsActive()) zoneLayer->OnTrackDeselection();
    }

    void OnTrackSelection() {
        for (auto& includedZone : this->includedZones_) {
            if (includedZone->DeactivatesOnTrackLoss() && !includedZone->GetIsActive()) includedZone->Activate();
            else if (includedZone->GetIsActive()) includedZone->OnTrackSelection();
        }
        for (auto& zoneLayer : this->zoneLayers_) if (zoneLayer->GetIsActive()) zoneLayer->OnTrackSelection();
    }

    void RequestUpdateWidget(Widget* widget) {
        for (auto& actionContext : GetActionContexts(widget)) {
            actionContext->RunDeferredActions();
            actionContext->RequestUpdate();
        }
    }

    virtual void GoSubZone(const char* subZoneName) {
        for (auto& subZone : subZones_) {
            if (IsSameString(subZone->GetName(), subZoneName)) {
                subZone->SetSlotIndex(GetSlotIndex());
                subZone->Activate();
            } else
                subZone->Deactivate();
        }
    }

    void EnterZoneLayer(const char* zoneLayerName) {
        for (auto& zoneLayer : this->zoneLayers_) {
            if (IsSameString(zoneLayer->GetName(), zoneLayerName)) {
                if (!zoneLayer->GetIsActive()) zoneLayer->Activate();
            }
            else zoneLayer->Deactivate();
        }
    }

    void ExitZoneLayer() {
        if (!this->parentZone_) return;
        this->Deactivate();
        this->parentZone_->UpdateCurrentActionContextModifiers();
    }
};

class SubZone : public Zone
{
private:
    Zone* const enclosingZone_;

public:
    SubZone(CSurfIntegrator* const csi, ZoneManager* const zoneManager, Navigator* navigator, int slotIndex, const string& name, const string& alias, const string& sourceFilePath, Zone* enclosingZone)
        : Zone(csi, zoneManager, navigator, slotIndex, name, alias, sourceFilePath), enclosingZone_(enclosingZone) {}

    virtual ~SubZone() {}

    virtual void GoSubZone(const char* subZoneName) override {
        enclosingZone_->GoSubZone(subZoneName);
    }
};

struct ZoneInfo {
    bool isLoaded = false;
    bool isReferenced = false;
    bool isSubZone = false;
    bool isFxZone = false;
    bool isUserZone = false;
    string navigator;
    string alias;
    string filePath;
};
