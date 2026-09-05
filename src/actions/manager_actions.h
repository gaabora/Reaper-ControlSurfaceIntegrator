// control_surface_manager_actions.h

#ifndef control_surface_manager_actions_h
#define control_surface_manager_actions_h

//! @action (manager press-only base)
//!
//! @brief Base for manager actions that fire on press only. Most navigation and utility actions derive from this.
class ManagerPressOnlyAction : public PressOnlyAction {};

//! @action SendMIDIMessage
//!
//! @brief Sends a raw MIDI message (3-byte or SysEx) to the surface's MIDI output.
//!
//! @zone_usage  WidgetName    SendMIDIMessage "90 01 7F"   or   SendMIDIMessage "F0 7E 7F 09 01 F7"
//!
//! @feedback None.
//!
//! @params String param: space-separated hex bytes (e.g. "90 3C 7F").
//!
//! @notes 3-byte messages use SendMidiMessage(); longer messages use SendMidiSysExMessage().
class SendMIDIMessage : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SendMIDIMessage; }

    void Do(ActionContext* context, double value) override {
        vector<string> tokens;
        GetTokens(tokens, context->GetStringParam());

        if (tokens.size() == 3) {
            context->GetSurface()->SendMidiMessage(strToHex(tokens[0]), strToHex(tokens[1]), strToHex(tokens[2]));
        } else {
            struct {
                MIDI_event_ex_t evt;
                char data[128];
            } midiSysExData;

            midiSysExData.evt.frame_offset = 0;
            midiSysExData.evt.size = 0;

            const int maxSize = static_cast<int>(sizeof(midiSysExData.evt.midi_message) + sizeof(midiSysExData.data));
            for (int i = 0; i < (int)tokens.size() && midiSysExData.evt.size < maxSize; ++i)
                midiSysExData.evt.midi_message[midiSysExData.evt.size++] = strToHex(tokens[i]);

            context->GetSurface()->SendMidiSysExMessage(&midiSysExData.evt);
        }
    }
};

//! @action SendOSCMessage
//!
//! @brief Sends a raw OSC message to the surface's OSC output.
//!
//! @zone_usage  WidgetName    SendOSCMessage "/path/to/param 1.0"
//!
//! @feedback None.
//!
//! @params String param: "address [value]" — value auto-detected as int, double, or string.
class SendOSCMessage : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SendOSCMessage; }

    void Do(ActionContext* context, double value) override { //FIXME: improve implementation
        vector<string> tokens;
        GetTokens(tokens, context->GetStringParam());

        if (tokens.size() == 1) {
            context->GetSurface()->SendOSCMessage(tokens[0].c_str());
            return;
        }

        if (tokens.size() != 2) return;

        const char *t1 = tokens[1].c_str(), *t1e = NULL;

        if (strstr(t1, ".")) {
            const double dv = strtod(t1, (char**) &t1e);
            if (t1e && t1e != t1 && !*t1e) {
                context->GetSurface()->SendOSCMessage(tokens[0].c_str(), dv);
                return;
            }
        } else if (*t1) {
            const int v = (int) strtol(t1, (char**) &t1e, 10);
            if (t1e && t1e != t1 && !*t1e) {
                context->GetSurface()->SendOSCMessage(tokens[0].c_str(), v);
                return;
            }
        }

        context->GetSurface()->SendOSCMessage(tokens[0].c_str(), tokens[1].c_str());
    }
};

//! @action Speak
//!
//! @brief Speaks a text message via OSARA accessibility screen reader.
//!
//! @zone_usage  WidgetName    Speak "Hello World"
//!
//! @feedback None (clears color only).
//!
//! @params String param: the message text to speak.
class SpeakOSARAMessage : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SpeakOSARAMessage; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetCSI()->Speak(context->GetStringParam());
    }
};

//! @action SetXTouchDisplayColors
//!
//! @brief Sets the X-Touch display scribble strip colors for all widgets in the current zone.
//!
//! @zone_usage  WidgetName    SetXTouchDisplayColors "colors_string"
//!
//! @feedback None.
//!
//! @params String param: color configuration string (surface-specific format).
class SetXTouchDisplayColors : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SetXTouchDisplayColors; }

    void Do(ActionContext* context, double value) override {
        context->GetZone()->SetXTouchDisplayColors(context->GetStringParam());
    }
};

//! @action RestoreXTouchDisplayColors
//!
//! @brief Restores X-Touch display scribble strip colors to their previous state.
//!
//! @zone_usage  WidgetName    RestoreXTouchDisplayColors
//!
//! @feedback None.
class RestoreXTouchDisplayColors : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::RestoreXTouchDisplayColors; }

    void Do(ActionContext* context, double value) override {
        context->GetZone()->RestoreXTouchDisplayColors();
    }
};

//! @action SaveProject
//!
//! @brief Saves the current Reaper project if it has unsaved changes.
//!
//! @zone_usage  WidgetName    SaveProject
//!
//! @feedback Toggle — 1.0 when project is dirty (unsaved changes), 0.0 when clean.
class SaveProject : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SaveProject; }

    void RequestUpdate(ActionContext* context) override {
        if (IsProjectDirty(NULL))
            context->UpdateWidgetValue(1);
        else
            context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (IsProjectDirty(NULL))
            Main_SaveProject(NULL, false);
    }
};

//! @action Undo
//!
//! @brief Undoes the last Reaper action if undo history is available.
//!
//! @zone_usage  WidgetName    Undo
//!
//! @feedback Toggle — 1.0 when undo is available, 0.0 when not.
class Undo : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::Undo; }

    void RequestUpdate(ActionContext* context) override {
        if (DAW::CanUndo())
            context->UpdateWidgetValue(1);
        else
            context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (DAW::CanUndo())
            DAW::Undo();
    }
};

//! @action Redo
//!
//! @brief Redoes the last undone Reaper action if redo history is available.
//!
//! @zone_usage  WidgetName    Redo
//!
//! @feedback Toggle — 1.0 when redo is available, 0.0 when not.
class Redo : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::Redo; }

    void RequestUpdate(ActionContext* context) override {
        if (DAW::CanRedo())
            context->UpdateWidgetValue(1);
        else
            context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (DAW::CanRedo())
            DAW::Redo();
    }
};

//! @action ToggleSynchPageBanking
//!
//! @brief Toggles synchronized banking across pages (all surfaces bank together).
//!
//! @zone_usage  WidgetName    ToggleSynchPageBanking
//!
//! @feedback Toggle — 1.0 when synch is enabled, 0.0 when disabled.
class ToggleSynchPageBanking : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleSynchPageBanking; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetSynchPages());
    }

    void Do(ActionContext* context, double value) override {
        context->GetTrackNavigationManager()->ToggleSynchPages();
    }
};

//! @action ToggleFollowMCP
//!
//! @brief Toggles whether the surface track layout follows the Mixer Control Panel order.
//!
//! @zone_usage  WidgetName    ToggleFollowMCP
//!
//! @feedback Toggle — 1.0 when following MCP, 0.0 when not.
class ToggleFollowMCP : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleFollowMCP; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetFollowMCP());
    }

    void Do(ActionContext* context, double value) override {
        context->GetTrackNavigationManager()->ToggleFollowMCP();
    }
};

//! @action ToggleScrollLink
//!
//! @brief Toggles scroll-link: syncs Reaper's mixer scroll position with the surface's current bank.
//!
//! @zone_usage  WidgetName    ToggleScrollLink   or   WidgetName    ToggleScrollLink 1
//!
//! @feedback Toggle — 1.0 when scroll-link is enabled, 0.0 when disabled.
//!
//! @params Optional int param: link mode (0=default, 1=alternative).
class ToggleScrollLink : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleScrollLink; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetScrollLink());
    }

    void Do(ActionContext* context, double value) override {
        context->GetTrackNavigationManager()->ToggleScrollLink(context->GetIntParam());
    }
};

//! @action ToggleRestrictTextLength
//!
//! @brief Toggles text length restriction on display widgets (truncates long names to fit scribble strips).
//!
//! @zone_usage  WidgetName    ToggleRestrictTextLength 7
//!
//! @feedback None.
//!
//! @params Int param: maximum character count.
class ToggleRestrictTextLength : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleRestrictTextLength; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->ToggleRestrictTextLength(context->GetIntParam());
    }
};

//! @action CSINameDisplay
//!
//! @brief Displays the application name on a text widget.
//!
//! @zone_usage  DisplayWidget    CSINameDisplay
//!
//! @feedback Text - always sends the generated product display name.
class CSINameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::CSINameDisplay; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(s_CSIName);
    }
};

//! @action CSIVersionDisplay
//!
//! @brief Displays the current version string on a text widget.
//!
//! @zone_usage  DisplayWidget    CSIVersionDisplay
//!
//! @feedback Text — always sends the version string (e.g. "v7.0").
class CSIVersionDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::CSIVersionDisplay; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(s_CSIVersionDisplay);
    }
};

//! @action GlobalModeDisplay
//!
//! @brief Displays the Global modifier state on a text widget.
//!
//! @zone_usage  DisplayWidget    GlobalModeDisplay
//!
//! @feedback Toggle (double) — sends 1.0 when Global modifier is engaged, 0.0 when not.
class GlobalModeDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::GlobalModeDisplay; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetGlobal());
    }
};

//! @action CycleTimeDisplayModes
//!
//! @brief Cycles through time display modes (bars+beats, seconds, samples, frames, etc.).
//!
//! @zone_usage  WidgetName    CycleTimeDisplayModes
//!
//! @feedback None (clears color only).
class CycleTimeDisplayModes : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::CycleTimeDisplayModes; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetCSI()->NextTimeDisplayMode();
    }
};

//! @action NextPage
//!
//! @brief Switches to the next page in the page list.
//!
//! @zone_usage  WidgetName    NextPage
//!
//! @feedback None (clears color only).
class GoNextPage : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::GoNextPage; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetCSI()->NextPage();
    }
};

//! @action GoPage
//!
//! @brief Switches to a named page.
//!
//! @zone_usage  WidgetName    GoPage "PageName"
//!
//! @feedback None (clears color only).
//!
//! @params String param: name of the target page.
class GoPage : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::GoPage; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetCSI()->GoToPage(context->GetStringParam());
    }
};

//! @action PageNameDisplay
//!
//! @brief Displays the current page name on a text widget.
//!
//! @zone_usage  DisplayWidget    PageNameDisplay
//!
//! @feedback Text — sends the current page name string.
class PageNameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::PageNameDisplay; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetPage()->GetName());
    }
};

//! @action GoHome
//!
//! @brief Deactivates all goZones and returns to the Home zone.
//!
//! @zone_usage  WidgetName    GoHome
//!
//! @feedback Toggle — 1.0 when NOT at home (a goZone is active), 0.0 when at home.
//!
//! @see GoZone, LeaveSubZone, AllSurfacesGoHome
class GoHome : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::GoHome; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetSurface()->GetZoneManager()->GetIsHomeZoneOnlyActive())
            context->UpdateWidgetValue(0.0);
        else
            context->UpdateWidgetValue(1.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DeclareGoHome();
    }
};

//! @action AllSurfacesGoHome
//!
//! @brief Sends GoHome to all surfaces on the current page, not just the current surface.
//!
//! @zone_usage  WidgetName    AllSurfacesGoHome
//!
//! @feedback None.
//!
//! @see GoHome
class AllSurfacesGoHome : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::AllSurfacesGoHome; }

    void Do(ActionContext* context, double value) override {
        context->GetPage()->GoHome();
    }
};

//! @action GoSubZone
//!
//! @brief Activates a named sub-zone within the current zone. Deactivates other sub-zones.
//!
//! @zone_usage  WidgetName    GoSubZone "SubZoneName"
//!
//! @feedback None (always sends 0.0).
//!
//! @params String param: name of the sub-zone to activate.
//!
//! @notes Sub-zones are child zones declared via SubZones directive. Use LeaveSubZone to deactivate.
//!
//! @see LeaveSubZone, GoZone
class GoSubZone : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::GoSubZone; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetZone()->GoSubZone(context->GetStringParam());
    }
};

//! @action LeaveSubZone
//!
//! @brief Deactivates the current zone (works for both goZones and sub-zones).
//!
//! @zone_usage  WidgetName    LeaveSubZone
//!
//! @feedback Always sends 1.0 (LED stays lit while the zone is active). Supports Blink property.
//!
//! @notes Generic "deactivate myself" action — calls zone->Deactivate(). For returning to Home zone specifically, prefer GoHome.
//!
//! @see GoSubZone, GoHome
class LeaveSubZone : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::LeaveSubZone; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(1.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetZone()->Deactivate();
    }
};

//! @action EnterZoneLayer
//!
//! @brief Activates one declared format 2 zone layer and deactivates its active sibling.
//!
//! @zone_usage  WidgetName    EnterZoneLayer "ZoneLayerName"
//!
//! @feedback None.
class EnterZoneLayer : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::EnterZoneLayer; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetZone()->EnterZoneLayer(context->GetStringParam());
    }
};

//! @action ExitZoneLayer
//!
//! @brief Deactivates the current format 2 zone layer and returns input priority to its parent.
//!
//! @zone_usage  WidgetName    ExitZoneLayer
//!
//! @feedback None.
class ExitZoneLayer : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ExitZoneLayer; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetZone()->ExitZoneLayer();
    }
};

//! @action GoFXSlot
//!
//! @brief Opens the FX zone for a specific FX slot on the current track.
//!
//! @zone_usage  WidgetName    GoFXSlot
//!
//! @feedback None (clears color only).
//!
//! @notes Slot index comes from the zone's navigator. Triggers DeclareGoFXSlot on the ZoneManager.
class GoFXSlot : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::GoFXSlot; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            context->GetSurface()->GetZoneManager()->DeclareGoFXSlot(track, context->GetNavigator(), context->GetSlotIndex());
    }
};

//! @action ShowFXSlot
//!
//! @brief Opens the FX plugin GUI window for a specific slot on the current track.
//!
//! @zone_usage  WidgetName    ShowFXSlot
//!
//! @feedback None.
class ShowFXSlot : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ShowFXSlot; }

    void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            TrackFX_SetOpen(track, context->GetSlotIndex(), true);
    }
};

//! @action HideFXSlot
//!
//! @brief Closes the FX plugin GUI window for a specific slot on the current track.
//!
//! @zone_usage  WidgetName    HideFXSlot
//!
//! @feedback None.
class HideFXSlot : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::HideFXSlot; }

    void Do(ActionContext* context, double value) override {
        if (MediaTrack* track = context->GetTrack())
            TrackFX_SetOpen(track, context->GetSlotIndex(), false);
    }
};

//! @action ToggleUseLocalModifiers
//!
//! @brief Toggles whether this surface uses its own local modifier state instead of shared modifiers.
//!
//! @zone_usage  WidgetName    ToggleUseLocalModifiers
//!
//! @feedback None.
class ToggleUseLocalModifiers : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleUseLocalModifiers; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->ToggleUseLocalModifiers();
    }
};

//! @action ToggleUseLocalFXSlot
//!
//! @brief Toggles whether each surface tracks its own FX slot index independently.
//!
//! @zone_usage  WidgetName    ToggleUseLocalFXSlot
//!
//! @feedback Toggle — 1.0 when local FX slot is enabled, 0.0 when shared.
class ToggleUseLocalFXSlot : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleUseLocalFXSlot; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetToggleUseLocalFXSlot());
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->ToggleUseLocalFXSlot();
    }
};

//! @action (range-validated settings base)
//!
//! @brief Base for settings actions that clamp an integer param to a valid [min, max] range and log warnings on out-of-range values.
class RangeValidatedSettingsAction : public SettingsAction
{
protected:
    int min_ = 0;
    int max_ = (std::numeric_limits<int>::max)();
    virtual void ApplyValue(ActionContext* context, int value) = 0;

public:
    void Do(ActionContext* context, double value) override {
        int rawValue = context->GetIntParam();
        int clampedValue = context->ClampValueWithWarning(rawValue, min_, max_);
        ApplyValue(context, clampedValue);
    }
};

//! @action SetLatchTime
//!
//! @brief Sets the modifier latch time in milliseconds. When a modifier button is pressed and released faster than this, it locks.
//!
//! @zone_usage  WidgetName    SetLatchTime 500
//!
//! @params Int param: latch time in ms (50–5000).
class SetLatchTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetLatchTime; }
    SetLatchTime() { min_ = 50; max_ = 5000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetLatchTime(value); }
};

//! @action SetBlinkTime
//!
//! @brief Sets the default LED blink interval in milliseconds for this surface.
//!
//! @zone_usage  WidgetName    SetBlinkTime 500
//!
//! @params Int param: blink interval in ms (50–2000). Used when Blink property is set without an explicit interval.
class SetBlinkTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetBlinkTime; }
    SetBlinkTime() { min_ = 50; max_ = 2000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetBlinkTime(value); }
};

//! @action SetHoldTime
//!
//! @brief Sets the default hold delay in milliseconds. Hold+ actions fire after the button is held this long.
//!
//! @zone_usage  WidgetName    SetHoldTime 1000
//!
//! @params Int param: hold delay in ms (50–10000). Used when HoldDelay is not explicitly set on the action.
class SetHoldTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetHoldTime; }
    SetHoldTime() { min_ = 50; max_ = 10000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetHoldTime(value); }
};

//! @action SetDoublePressTime
//!
//! @brief Sets the double-press detection window in milliseconds.
//!
//! @zone_usage  WidgetName    SetDoublePressTime 400
//!
//! @params Int param: window in ms (100–2000). Two presses within this window trigger a DoublePress+ action.
class SetDoublePressTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetDoublePressTime; }
    SetDoublePressTime() { min_ = 100; max_ = 2000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetDoublePressTime(value); }
};

//! @action SetOSDTime
//!
//! @brief Sets the on-screen display timeout in milliseconds.
//!
//! @zone_usage  WidgetName    SetOSDTime 3000
//!
//! @params Int param: timeout in ms (100–30000). OSD messages disappear after this duration.
class SetOSDTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetOSDTime; }
    SetOSDTime() { min_ = 100; max_ = 30000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetOSDTime(value); }
};

//! @action SetDebugLevel
//!
//! @brief Sets the global debug logging level.
//!
//! @zone_usage  WidgetName    SetDebugLevel 2
//!
//! @params Int param: debug level 0-4 (0=Error, 1=Warning, 2=Notice, 3=Info, 4=Debug).
class SetDebugLevel : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetDebugLevel; }
    SetDebugLevel() { min_ = 0; max_ = 4; }
    void ApplyValue(ActionContext* context, int value) override { context->GetCSI()->SetProductDebugLevel(value); }
};

//! @action CycleDebugLevel
//!
//! @brief Cycles through debug levels 0-4 on each press.
//!
//! @zone_usage  WidgetName    CycleDebugLevel
//!
//! @feedback None.
class CycleDebugLevel : public SettingsAction
{
public:
    ActionType GetType() const override { return ActionType::CycleDebugLevel; }

    void Do(ActionContext* context, double value) override {
        constexpr int maxLevel = DEBUG_LEVEL_DEBUG;
        context->GetCSI()->SetProductDebugLevel((g_debugLevel + 1) % (maxLevel + 1));
    }
};

//! @action EnableOSD
//!
//! @brief Enables or disables the on-screen display for this surface.
//!
//! @zone_usage  WidgetName    EnableOSD   or   WidgetName    EnableOSD "No"
//!
//! @feedback None.
//!
//! @params String param: "No" to disable, anything else (or empty) to enable.
class EnableOSD : public SettingsAction
{
public:
    ActionType GetType() const override { return ActionType::EnableOSD; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetOsdEnabled(!IsSameString(context->GetStringParam(), "No"));
    }
};

//! @action ToggleEnableFocusedFXMapping
//!
//! @brief Toggles automatic mapping of the currently focused FX to the surface.
//!
//! @zone_usage  WidgetName    ToggleEnableFocusedFXMapping
//!
//! @feedback Toggle — 1.0 when focused FX mapping is enabled, 0.0 when disabled.
class ToggleEnableFocusedFXMapping : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleEnableFocusedFXMapping; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsFocusedFXMappingEnabled());
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DeclareToggleEnableFocusedFXMapping();
    }
};

//! @action DisableFocusedFXMapping
//!
//! @brief Disables focused FX mapping (one-way — use ToggleEnableFocusedFXMapping to re-enable).
//!
//! @zone_usage  WidgetName    DisableFocusedFXMapping
//!
//! @feedback Toggle — 1.0 when focused FX mapping is enabled, 0.0 when disabled.
class DisableFocusedFXMapping : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::DisableFocusedFXMapping; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsFocusedFXMappingEnabled());
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DisableFocusedFXMapping();
    }
};

//! @action ToggleEnableLastTouchedFXParamMapping
//!
//! @brief Toggles automatic mapping of the last-touched FX parameter to the surface.
//!
//! @zone_usage  WidgetName    ToggleEnableLastTouchedFXParamMapping
//!
//! @feedback Toggle — 1.0 when enabled, 0.0 when disabled.
class ToggleEnableLastTouchedFXParamMapping : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleEnableLastTouchedFXParamMapping; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsLastTouchedFXParamMappingEnabled());
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DeclareToggleEnableLastTouchedFXParamMapping();
    }
};

//! @action DisableLastTouchedFXParamMapping
//!
//! @brief Disables last-touched FX param mapping (one-way).
//!
//! @zone_usage  WidgetName    DisableLastTouchedFXParamMapping
//!
//! @feedback Toggle — 1.0 when enabled, 0.0 when disabled.
class DisableLastTouchedFXParamMapping : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::DisableLastTouchedFXParamMapping; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsLastTouchedFXParamMappingEnabled());
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DisableLastTouchedFXParamMapping();
    }
};

//! @action LearnFocusedFX
//!
//! @brief Opens the FX zone learning dialog for the currently focused FX.
//!
//! @zone_usage  WidgetName    LearnFocusedFX
//!
//! @feedback None.
class LearnFocusedFX : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::LearnFocusedFX; }

    void Do(ActionContext* context, double value) override {
        RequestFocusedFXDialog(context->GetSurface()->GetZoneManager());
    }
};

//! @action GoZone
//!
//! @brief Activates a named top-level zone and deactivates other top-level zones. Repeating it keeps a format 2 zone active; legacy zones retain their old toggle behavior.
//!
//! @zone_usage  WidgetName    GoZone "MasterTrack"   or   GoZone "SelectedTrackFX"
//!
//! @feedback Toggle — 1.0 when the named zone is active, 0.0 when not.
//!
//! @params String param: zone name. Format 2 routing comes from the target zone metadata. Legacy special names retain their old routing.
//!
//! @see GoHome, GoSubZone
class GoZone : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::GoZone; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetSurface()->GetZoneManager()->GetIsGoZoneActive(context->GetStringParam()))
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        const char* name = context->GetStringParam();
        if (context->GetSurface()->GetZoneManager()->UsesFormat2ZoneProfile()) {
            context->GetSurface()->GetZoneManager()->DeclareGoZone(name);
            return;
        }
        if (IsSameString(name, "Folder") || IsSameString(name, "VCA") || IsSameString(name, "TrackSend") || IsSameString(name, "TrackReceive") || IsSameString(name, "MasterTrackFXMenu") || IsSameString(name, "TrackFXMenu"))
            context->GetPage()->GoZone(name);
        else
            context->GetSurface()->GetZoneManager()->DeclareGoZone(name);
    }
};

//! @action ToggleSelectedTrackFX
//!
//! @brief Opens mapped FX zones for the selected track, or closes them when they are already open.
//!
//! @zone_usage  WidgetName ToggleSelectedTrackFX
//!
//! @feedback Toggle. 1.0 while at least one selected-track FX zone is active.
class ToggleSelectedTrackFX : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleSelectedTrackFX; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->IsSelectedTrackFXControlActive() ? 1.0 : 0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DeclareToggleSelectedTrackFX();
    }
};

//! @action ClearLastTouchedFXParam
//!
//! @brief Clears the last-touched FX parameter mapping zone.
//!
//! @zone_usage  WidgetName    ClearLastTouchedFXParam
//!
//! @feedback None.
class ClearLastTouchedFXParam : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ClearLastTouchedFXParam; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DeclareClearLastTouchedFXParam();
    }
};

//! @action ClearFocusedFX
//!
//! @brief Clears the focused FX mapping zone.
//!
//! @zone_usage  WidgetName    ClearFocusedFX
//!
//! @feedback None.
class ClearFocusedFX : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ClearFocusedFX; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DeclareClearFocusedFX();
    }
};

//! @action ClearSelectedTrackFX
//!
//! @brief Clears the selected track FX mapping zone.
//!
//! @zone_usage  WidgetName    ClearSelectedTrackFX
//!
//! @feedback None.
class ClearSelectedTrackFX : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ClearSelectedTrackFX; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DeclareClearSelectedTrackFX();
    }
};

//! @action ClearFXSlot
//!
//! @brief Clears the FX slot mapping zone.
//!
//! @zone_usage  WidgetName    ClearFXSlot
//!
//! @feedback None.
class ClearFXSlot : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ClearFXSlot; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->GetZoneManager()->DeclareClearFXSlot();
    }
};

//! @action Bank
//!
//! @brief Adjusts the track bank offset (scrolls channels left/right on the surface).
//!
//! @zone_usage  Legacy: WidgetName Bank "TrackNavigator" 8. Format 2: WidgetName Bank 8, using the current zone Target and BankTarget.
//!
//! @feedback None (clears color only).
//!
//! @params Legacy uses a navigator name and step size. Format 2 accepts only a step size and derives the bank from zone metadata. Positive moves right and negative moves left.
class Bank : public Action
{
public:
    ActionType GetType() const override { return ActionType::Bank; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        const bool shouldAdjust = (value < 0 && context->GetRangeMinimum() < 0) || (value > 0 && context->GetRangeMinimum() >= 0);
        if (!shouldAdjust) return;
        if (context->GetZone()->UsesFormat2Runtime()) {
            context->GetCSI()->AdjustFormat2Bank(context->GetPage(), context->GetZone()->GetRuntimeTarget(), context->GetZone()->GetRuntimeBankTarget(), context->GetIntParam());
            return;
        }
        context->GetCSI()->AdjustBank(context->GetPage(), context->GetStringParam(), context->GetIntParam());
    }
};

//! @action Shift
//!
//! @brief Engages/disengages the Shift modifier. Supports latch (quick tap locks, hold+release unlocks).
//!
//! @zone_usage  WidgetName    Shift
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged. Supports Blink for visual latch indication.
//!
//! @see SetOption, SetControl, SetAlt, SetFlip, SetGlobal, ClearModifier, ClearModifiers
class SetShift : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetShift; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetShift();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetShift(value != 0, context->GetModifierMode());
    }
};

//! @action Option
//!
//! @brief Engages/disengages the Option modifier. Supports latch.
//!
//! @zone_usage  WidgetName    Option
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetOption : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetOption; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetOption();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetOption(value != 0, context->GetModifierMode());
    }
};

//! @action Control
//!
//! @brief Engages/disengages the Control modifier. Supports latch.
//!
//! @zone_usage  WidgetName    Control
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetControl : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetControl; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetControl();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetControl(value != 0, context->GetModifierMode());
    }
};

//! @action Alt
//!
//! @brief Engages/disengages the Alt modifier. Supports latch.
//!
//! @zone_usage  WidgetName    Alt
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetAlt : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetAlt; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetAlt();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetAlt(value != 0, context->GetModifierMode());
    }
};

//! @action Flip
//!
//! @brief Engages/disengages the Flip modifier (swaps fader/knob assignments). Supports latch.
//!
//! @zone_usage  WidgetName    Flip
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetFlip : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetFlip; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetFlip();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetFlip(value != 0, context->GetModifierMode());
    }
};

//! @action Global
//!
//! @brief Engages/disengages the Global modifier. Supports latch.
//!
//! @zone_usage  WidgetName    Global
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetGlobal : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetGlobal; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetGlobal();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetGlobal(value != 0, context->GetModifierMode());
    }
};

//! @action Marker
//!
//! @brief Engages/disengages the Marker modifier. Mutually exclusive with Nudge, Zoom, and Scrub.
//!
//! @zone_usage  WidgetName    Marker
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetMarker : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetMarker; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetMarker();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetMarker(value != 0, context->GetModifierMode());
    }
};

//! @action Nudge
//!
//! @brief Engages/disengages the Nudge modifier. Mutually exclusive with Marker, Zoom, and Scrub.
//!
//! @zone_usage  WidgetName    Nudge
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetNudge : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetNudge; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetNudge();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetNudge(value != 0, context->GetModifierMode());
    }
};

//! @action Zoom
//!
//! @brief Engages/disengages the Zoom modifier. Mutually exclusive with Marker, Nudge, and Scrub.
//!
//! @zone_usage  WidgetName    Zoom
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetZoom : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetZoom; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetZoom();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetZoom(value != 0, context->GetModifierMode());
    }
};

//! @action Scrub
//!
//! @brief Engages/disengages the Scrub modifier. Mutually exclusive with Marker, Nudge, and Zoom.
//!
//! @zone_usage  WidgetName    Scrub
//!
//! @feedback Toggle — 1.0 when engaged, 0.0 when disengaged.
class SetScrub : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetScrub; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetScrub();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetScrub(value != 0, context->GetModifierMode());
    }
};

//! @action ClearModifier
//!
//! @brief Clears a specific named modifier (disengages and unlocks it).
//!
//! @zone_usage  WidgetName    ClearModifier "Shift"
//!
//! @feedback None.
//!
//! @params String param: modifier name (Shift, Option, Control, Alt, Flip, Global, Marker, Nudge, Zoom, Scrub).
class ClearModifier : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::ClearModifier; }
    bool IgnoresRelease() const override { return true; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->ClearModifier(context->GetStringParam());
    }
};

//! @action ClearModifiers
//!
//! @brief Clears all modifiers at once (disengages and unlocks all).
//!
//! @zone_usage  WidgetName    ClearModifiers
//!
//! @feedback None.
class ClearModifiers : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::ClearModifiers; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->ClearModifiers();
    }
};

//! @action ToggleChannel
//!
//! @brief Toggles the channel toggle state for the widget's channel number (used for per-channel mode switching).
//!
//! @zone_usage  WidgetName    ToggleChannel
//!
//! @feedback None (clears color only).
class SetToggleChannel : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::SetToggleChannel; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->ToggleChannel(context->GetWidget()->GetChannelNumber());
    }
};

//! @action ToggleOSK
//!
//! @brief Toggles the On-Screen Keyboard (OSK) overlay for this surface.
//!
//! @zone_usage  WidgetName    ToggleOSK
//!
//! @feedback Toggle — 1.0 when OSK is enabled, 0.0 when disabled.
//!
//! @notes When enabling, publishes OSK layout, labels, and state. When disabling, closes the OSK panel.
class ToggleOSK : public ManagerPressOnlyAction
{
public:
    ActionType GetType() const override { return ActionType::ToggleOSK; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetOskEnabled() ? 1.0 : 0.0);
    }

    void Do(ActionContext* context, double value) override {
        ControlSurface* surface = context->GetSurface();
        bool newState = !surface->GetOskEnabled();
        surface->SetOskEnabled(newState);

        if (newState) {
            surface->PublishOSKLayout();
            surface->PublishOSKLabels();
            surface->PublishOSKState();
            context->GetCSI()->PublishOSKSurfacesList();
            context->GetCSI()->OpenOSKPanel();
        } else {
            context->GetCSI()->PublishOSKSurfacesList();
            if (!context->GetCSI()->HasAnyOSKEnabled()) context->GetCSI()->CloseOSKPanel();
        }
    }
};

#endif /* control_surface_manager_actions_h */
