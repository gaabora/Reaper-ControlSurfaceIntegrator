#pragma once
// page.h — Page class (and out-of-line ZoneManager methods that depend on ControlSurface)

#include "preamble.h"
#include "page_interface.h"
#include "control_surface.h"
#include "modifier_manager.h"
#include "track_nav_manager.h"

// Out-of-line implementations of ZoneManager methods that depend on ControlSurface
inline void ZoneManager::GoZone(const char* zoneName) {
    ClearFXMapping();
    ResetOffsets();

    for (int i = 0; i < goZones_.size(); ++i) {
        if (IsSameString(zoneName, goZones_[i]->GetName())) {
            if (goZones_[i]->GetIsActive()) {
                if (this->format2ZoneProfile_) {
                    this->surface_->PublishOSKLabels();
                    return;
                }
                for (int j = i; j < goZones_.size(); ++j)
                    if (IsSameString(zoneName, goZones_[j]->GetName()))
                        goZones_[j]->Deactivate();

                surface_->PublishOSKLabels();
                return;
            }
        }
    }
    for (auto& goZone : goZones_)
        if (!IsSameString(zoneName, goZone->GetName()))
            goZone->Deactivate();
    for (auto& goZone : goZones_)
        if (IsSameString(zoneName, goZone->GetName()))
            goZone->Activate();
    if (!this->format2ZoneProfile_ && IsSameString(zoneName, "SelectedTrackFX"))
        GoSelectedTrackFX();
    surface_->PublishOSKLabels();
}

inline void ZoneManager::GoFormat2Zone(const char* zoneName) {
    for (auto& goZone : this->goZones_) {
        if (!IsSameString(zoneName, goZone->GetName())) continue;
        if (goZone->UsesPageActivation()) this->surface_->GetPage()->GoZone(zoneName);
        else this->GoZone(zoneName);
        return;
    }
}

inline void ZoneManager::GoFormat2Home() {
    for (auto& goZone : this->goZones_) {
        if (goZone->GetIsActive() && goZone->UsesPageActivation()) {
            this->surface_->GetPage()->GoHome();
            return;
        }
    }
    this->GoHome();
}

inline void ZoneManager::GoHome() {
    HideAllFXWindows();
    ClearFXMapping();
    ResetOffsets();
    for (auto& goZone : goZones_) goZone->Deactivate();
    homeZone_->Activate();
    surface_->PublishOSKLabels();
}

class Page : public IPageContext
{
protected:
    CSurfIntegrator* const csi_;
    string const name_;
    unique_ptr<TrackNavigationManager> trackNavigationManager_;
    unique_ptr<ModifierManager> modifierManager_;
    vector<unique_ptr<ControlSurface>> surfaces_;

public:
    Page(CSurfIntegrator* const csi, const char* name, bool followMCP, bool synchPages, bool isScrollLinkEnabled, bool isScrollSynchEnabled)
        : csi_(csi)
        , name_(name)
        , trackNavigationManager_(make_unique<TrackNavigationManager>(csi_, this, followMCP, synchPages, isScrollLinkEnabled, isScrollSynchEnabled))
        , modifierManager_(make_unique<ModifierManager>(csi_, this, (ControlSurface*) NULL)) {}

    virtual ~Page() override { surfaces_.clear(); }

    const char* GetName() override { return name_.c_str(); }

    ModifierManager* GetModifierManager() override { return modifierManager_.get(); }

    vector<unique_ptr<ControlSurface>>& GetSurfaces() { return surfaces_; }

    void UpdateCurrentActionContextModifiers() override { for (auto& surface : surfaces_) surface->UpdateCurrentActionContextModifiers(); }

    void ForceClear() { for (auto& surface : surfaces_) surface->ForceClear(); }
    void ForceClearTrack(int trackNum) override { for (auto& surface : surfaces_) surface->ForceClearTrack(trackNum); }
    void ForceUpdateTrackColors() override { for (auto& surface : surfaces_) surface->ForceUpdateTrackColors(); }
    bool GetTouchState(MediaTrack* track, int touchedControl) { return trackNavigationManager_->GetIsControlTouched(track, touchedControl); }

    void OnTrackSelection(MediaTrack* track) override {
        trackNavigationManager_->OnTrackSelection();
        for (auto& surface : surfaces_)
            surface->OnTrackSelection(track);
    }

    void OnTrackListChange() { trackNavigationManager_->OnTrackListChange(); }

    void OnTrackSelectionBySurface(MediaTrack* track) override {
        trackNavigationManager_->OnTrackSelectionBySurface(track);
        for (auto& surface : surfaces_)
            surface->OnTrackSelection(track);
    }

    void TrackFXListChanged(MediaTrack* track) { for (auto& surface : surfaces_) surface->TrackFXListChanged(track); }

    void EnterPage() {
        trackNavigationManager_->EnterPage();
        for (auto& surface : surfaces_)
            surface->OnPageEnter();
    }

    void LeavePage() {
        trackNavigationManager_->LeavePage();
        for (auto& surface : surfaces_) 
            surface->OnPageLeave();
    }

    void OnInitialization() { for (auto& surface : surfaces_) surface->OnInitialization(); }
    void SignalStop() override { for (auto& surface : surfaces_) surface->HandleStop(); }
    void SignalPlay() override { for (auto& surface : surfaces_) surface->HandlePlay(); }
    void SignalRecord() override { for (auto& surface : surfaces_) surface->HandleRecord(); }
    void GoHome() override { for (auto& surface : surfaces_) surface->GetZoneManager()->GoHome(); }
    void GoZone(const char* name) override { for (auto& surface : surfaces_) surface->GetZoneManager()->GoZone(name); }

    void AdjustBank(const char* zoneName, int amount) override {
        if (IsSameString(zoneName, "Track")) trackNavigationManager_->AdjustTrackBank(amount);
        else if (IsSameString(zoneName, "VCA")) trackNavigationManager_->AdjustVCABank(amount);
        else if (IsSameString(zoneName, "Folder")) trackNavigationManager_->AdjustFolderBank(amount);
        else if (IsSameString(zoneName, "SelectedTracks")) trackNavigationManager_->AdjustSelectedTracksBank(amount);
        else if (IsSameString(zoneName, "SelectedTrack")) trackNavigationManager_->AdjustSelectedTrackBank(amount);
        else 
            for (auto& surface : surfaces_)
                surface->GetZoneManager()->AdjustBank(zoneName, amount);
    }

    TrackNavigationManager* GetTrackNavigationManager() override { return trackNavigationManager_.get(); }

    /*
    int repeats = 0;
    
    void Run()
    {
        int start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                
        repeats++;
         
        if (repeats > 50)
        {
            repeats = 0;
            
            int totalDuration = 0;

            start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            trackNavigationManager_->RebuildTracks();
            trackNavigationManager_->RebuildVCASpill();
            trackNavigationManager_->RebuildFolderTracks();
            int duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
            totalDuration += duration;
            ShowDuration("Rebuild Track/VCA/Folder List", duration);
            
            for (int i = 0; i < surfaces_.size(); ++i)
            {
                start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                surfaces_[i]->HandleExternalInput();
                duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
                totalDuration += duration;
                ShowDuration(surfaces_[i]->GetName(), "HandleExternalInput", duration);
            }
            
            for (int i = 0; i < surfaces_.size(); ++i)
            {
                start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                surfaces_[i]->RequestUpdate();
                duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
                totalDuration += duration;
                ShowDuration(surfaces_[i]->GetName(), "Request Update", duration);
            }
            
            LogToConsole("Total duration = %d\n\n\n", totalDuration);
        }
    }
    
    
    void ShowDuration(string item, int duration)
    {
        LogToConsole("%s - %d microseconds\n", item.c_str(), duration);
    }
    
    void ShowDuration(string surface, string item, int duration)
    {
        LogToConsole("%s - %s - %d microseconds\n", surface.c_str(), item.c_str(), duration);
    }
   */

    void Run() {
        trackNavigationManager_->RebuildTracks();
        trackNavigationManager_->RebuildVCASpill();
        trackNavigationManager_->RebuildFolderTracks();
        trackNavigationManager_->RebuildSelectedTracks();

        for (auto& surface : surfaces_) 
            surface->HandleExternalInput();

        for (auto& surface : surfaces_)
            surface->RequestUpdate();

        // Publish OSK state at ~10Hz (every 3rd cycle of 30Hz)
        for (auto& surface : surfaces_)
            surface->PublishOSKState();
    }
};
