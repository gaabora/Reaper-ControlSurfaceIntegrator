#pragma once
//
//  page_interface.h — IPageContext: pure-virtual interface for what ControlSurface
//  and Navigator need from their owning Page context. Introducing this breaks the
//  header-level circular dependency: control_surface.h / navigator.h only need this
//  thin header, not the full page.h (which #includes control_surface.h).
//

#include "fwd.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPageContext
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~IPageContext() {}

    // Identity
    virtual const char *GetName() = 0;

    // Sub-manager accessors
    virtual TrackNavigationManager *GetTrackNavigationManager() = 0;
    virtual ModifierManager *GetModifierManager() = 0;

    // Modifier propagation (called by ModifierManager when owned by Page)
    virtual void UpdateCurrentActionContextModifiers() = 0;

    // Track colour / display refresh
    virtual void ForceClearTrack(int trackNum) = 0;
    virtual void ForceUpdateTrackColors() = 0;

    // Track selection coordination
    virtual void OnTrackSelection(MediaTrack *track) = 0;
    virtual void OnTrackSelectionBySurface(MediaTrack *track) = 0;

    // Zone navigation
    virtual void GoHome() = 0;
    virtual void GoZone(const char *name) = 0;
    virtual void AdjustBank(const char *zoneName, int amount) = 0;

    // Transport coordination (broadcast to all surfaces on the page)
    virtual void SignalStop() = 0;
    virtual void SignalPlay() = 0;
    virtual void SignalRecord() = 0;
};
