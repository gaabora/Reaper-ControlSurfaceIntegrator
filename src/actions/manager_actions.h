// control_surface_manager_actions.h

#ifndef control_surface_manager_actions_h
#define control_surface_manager_actions_h

class SendMIDIMessage : public Action
{
public:
    ActionType GetType() const override { return ActionType::SendMIDIMessage; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return; //FIXME: there are many actions with this, only in this file there are 40 checks like this almost on all Do methods, can we dry somehow?

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

class SendOSCMessage : public Action
{
public:
    ActionType GetType() const override { return ActionType::SendOSCMessage; }

    void Do(ActionContext* context, double value) override { //FIXME: improve implementation
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
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

class SpeakOSARAMessage : public Action
{
public:
    ActionType GetType() const override { return ActionType::SpeakOSARAMessage; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetCSI()->Speak(context->GetStringParam());
    }
};

class SetXTouchDisplayColors : public Action
{
public:
    ActionType GetType() const override { return ActionType::SetXTouchDisplayColors; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetZone()->SetXTouchDisplayColors(context->GetStringParam());
    }
};

class RestoreXTouchDisplayColors : public Action
{
public:
    ActionType GetType() const override { return ActionType::RestoreXTouchDisplayColors; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetZone()->RestoreXTouchDisplayColors();
    }
};

class SaveProject : public Action
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
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (IsProjectDirty(NULL))
            Main_SaveProject(NULL, false);
    }
};

class Undo : public Action
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
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (DAW::CanUndo())
            DAW::Undo();
    }
};

class Redo : public Action
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
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (DAW::CanRedo())
            DAW::Redo();
    }
};

class ToggleSynchPageBanking : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleSynchPageBanking; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetSynchPages());
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetTrackNavigationManager()->ToggleSynchPages();
    }
};

class ToggleFollowMCP : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleFollowMCP; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetFollowMCP());
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetTrackNavigationManager()->ToggleFollowMCP();
    }
};

class ToggleScrollLink : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleScrollLink; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetTrackNavigationManager()->GetScrollLink());
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetTrackNavigationManager()->ToggleScrollLink(context->GetIntParam());
    }
};

class ToggleRestrictTextLength : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleRestrictTextLength; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->ToggleRestrictTextLength(context->GetIntParam());
    }
};

class CSINameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::CSINameDisplay; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(s_CSIName);
    }
};

class CSIVersionDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::CSIVersionDisplay; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(s_CSIVersionDisplay);
    }
};

class GlobalModeDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::GlobalModeDisplay; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetGlobal());
    }
};

class CycleTimeDisplayModes : public Action
{
public:
    ActionType GetType() const override { return ActionType::CycleTimeDisplayModes; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetCSI()->NextTimeDisplayMode();
    }
};

class GoNextPage : public Action
{
public:
    ActionType GetType() const override { return ActionType::GoNextPage; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetCSI()->NextPage();
    }
};

class GoPage : public Action
{
public:
    ActionType GetType() const override { return ActionType::GoPage; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetCSI()->GoToPage(context->GetStringParam());
    }
};

class PageNameDisplay : public DisplayAction
{
public:
    ActionType GetType() const override { return ActionType::PageNameDisplay; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetPage()->GetName());
    }
};

class GoHome : public Action
{
public:
    ActionType GetType() const override { return ActionType::GoHome; }

    virtual void RequestUpdate(ActionContext* context) override {
        if (context->GetSurface()->GetZoneManager()->GetIsHomeZoneOnlyActive())
            context->UpdateWidgetValue(1.0);
        else
            context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DeclareGoHome();
    }
};

class AllSurfacesGoHome : public Action
{
public:
    ActionType GetType() const override { return ActionType::AllSurfacesGoHome; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetPage()->GoHome();
    }
};

class GoSubZone : public Action
{
public:
    ActionType GetType() const override { return ActionType::GoSubZone; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetZone()->GoSubZone(context->GetStringParam());
    }
};

class LeaveSubZone : public Action
{
public:
    ActionType GetType() const override { return ActionType::LeaveSubZone; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(1.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetZone()->Deactivate();
    }
};

class GoFXSlot : public Action
{
public:
    ActionType GetType() const override { return ActionType::GoFXSlot; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (MediaTrack* track = context->GetTrack())
            context->GetSurface()->GetZoneManager()->DeclareGoFXSlot(track, context->GetZone()->GetNavigator(), context->GetSlotIndex());
    }
};

class ShowFXSlot : public Action
{
public:
    ActionType GetType() const override { return ActionType::ShowFXSlot; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (MediaTrack* track = context->GetTrack())
            TrackFX_SetOpen(track, context->GetSlotIndex(), true);
    }
};

class HideFXSlot : public Action
{
public:
    ActionType GetType() const override { return ActionType::HideFXSlot; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        if (MediaTrack* track = context->GetTrack())
            TrackFX_SetOpen(track, context->GetSlotIndex(), false);
    }
};

class ToggleUseLocalModifiers : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleUseLocalModifiers; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->ToggleUseLocalModifiers();
    }
};

class ToggleUseLocalFXSlot : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleUseLocalFXSlot; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetToggleUseLocalFXSlot());
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->ToggleUseLocalFXSlot();
    }
};

class RangeValidatedSettingsAction : public SettingsAction
{
protected:
    int min_ = 0;
    int max_ = std::numeric_limits<int>::max();
    virtual void ApplyValue(ActionContext* context, int value) = 0;

public:
    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        int rawValue = context->GetIntParam();
        int clampedValue = context->ClampValueWithWarning(rawValue, min_, max_);
        ApplyValue(context, clampedValue);
    }
};

class SetLatchTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetLatchTime; }
    SetLatchTime() { min_ = 50; max_ = 5000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetLatchTime(value); }
};

class SetBlinkTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetBlinkTime; }
    SetBlinkTime() { min_ = 50; max_ = 2000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetBlinkTime(value); }
};

class SetHoldTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetHoldTime; }
    SetHoldTime() { min_ = 50; max_ = 10000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetHoldTime(value); }
};

class SetDoublePressTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetDoublePressTime; }
    SetDoublePressTime() { min_ = 100; max_ = 2000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetDoublePressTime(value); }
};

class SetOSDTime : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetOSDTime; }
    SetOSDTime() { min_ = 100; max_ = 30000; }
    void ApplyValue(ActionContext* context, int value) override { context->GetSurface()->SetOSDTime(value); }
};

class SetDebugLevel : public RangeValidatedSettingsAction
{
public:
    ActionType GetType() const override { return ActionType::SetDebugLevel; }
    SetDebugLevel() { min_ = 0; max_ = 4; }
    void ApplyValue(ActionContext* context, int value) override { g_debugLevel = value; }
};

class CycleDebugLevel : public SettingsAction
{
public:
    ActionType GetType() const override { return ActionType::CycleDebugLevel; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        constexpr int maxLevel = DEBUG_LEVEL_DEBUG;
        g_debugLevel = (g_debugLevel + 1) % (maxLevel + 1);
    }
};

class EnableOSD : public SettingsAction
{
public:
    ActionType GetType() const override { return ActionType::EnableOSD; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->SetOsdEnabled(!IsSameString(context->GetStringParam(), "No"));
    }
};

class ToggleEnableFocusedFXMapping : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleEnableFocusedFXMapping; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsFocusedFXMappingEnabled());
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DeclareToggleEnableFocusedFXMapping();
    }
};

class DisableFocusedFXMapping : public Action
{
public:
    ActionType GetType() const override { return ActionType::DisableFocusedFXMapping; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsFocusedFXMappingEnabled());
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DisableFocusedFXMapping();
    }
};

class ToggleEnableLastTouchedFXParamMapping : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleEnableLastTouchedFXParamMapping; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsLastTouchedFXParamMappingEnabled());
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DeclareToggleEnableLastTouchedFXParamMapping();
    }
};

class DisableLastTouchedFXParamMapping : public Action
{
public:
    ActionType GetType() const override { return ActionType::DisableLastTouchedFXParamMapping; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetZoneManager()->GetIsLastTouchedFXParamMappingEnabled());
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DisableLastTouchedFXParamMapping();
    }
};

class LearnFocusedFX : public Action
{
public:
    ActionType GetType() const override { return ActionType::LearnFocusedFX; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        RequestFocusedFXDialog(context->GetSurface()->GetZoneManager());
    }
};

class GoZone : public Action
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
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        const char* name = context->GetStringParam();
        if (IsSameString(name, "Folder") || IsSameString(name, "VCA") || IsSameString(name, "TrackSend") || IsSameString(name, "TrackReceive") || IsSameString(name, "MasterTrackFXMenu") || IsSameString(name, "TrackFXMenu"))
            context->GetPage()->GoZone(name);
        else
            context->GetSurface()->GetZoneManager()->DeclareGoZone(name);
    }
};

class ClearLastTouchedFXParam : public Action
{
public:
    ActionType GetType() const override { return ActionType::ClearLastTouchedFXParam; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DeclareClearFXZone("LastTouchedFXParam");
    }
};

class ClearFocusedFX : public Action
{
public:
    ActionType GetType() const override { return ActionType::ClearFocusedFX; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DeclareClearFXZone("FocusedFX");
    }
};

class ClearSelectedTrackFX : public Action
{
public:
    ActionType GetType() const override { return ActionType::ClearSelectedTrackFX; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DeclareClearFXZone("SelectedTrackFX");
    }
};

class ClearFXSlot : public Action
{
public:
    ActionType GetType() const override { return ActionType::ClearFXSlot; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->GetZoneManager()->DeclareClearFXZone("FXSlot");
    }
};

class Bank : public Action
{
public:
    ActionType GetType() const override { return ActionType::Bank; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value < 0 && context->GetRangeMinimum() < 0)
            context->GetCSI()->AdjustBank(context->GetPage(), context->GetStringParam(), context->GetIntParam());
        else if (value > 0 && context->GetRangeMinimum() >= 0)
            context->GetCSI()->AdjustBank(context->GetPage(), context->GetStringParam(), context->GetIntParam());
    }
};

class SetShift : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetShift; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetShift();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetShift(value != 0);
    }
};

class SetOption : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetOption; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetOption();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetOption(value != 0);
    }
};

class SetControl : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetControl; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetControl();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetControl(value != 0);
    }
};

class SetAlt : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetAlt; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetAlt();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetAlt(value != 0);
    }
};

class SetFlip : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetFlip; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetFlip();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetFlip(value != 0);
    }
};

class SetGlobal : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetGlobal; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetGlobal();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetGlobal(value != 0);
    }
};

class SetMarker : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetMarker; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetMarker();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetMarker(value != 0);
    }
};

class SetNudge : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetNudge; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetNudge();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetNudge(value != 0);
    }
};

class SetZoom : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetZoom; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetZoom();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetZoom(value != 0);
    }
};

class SetScrub : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::SetScrub; }

    virtual double GetCurrentNormalizedValue(ActionContext* context) override {
        return context->GetSurface()->GetScrub();
    }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->SetScrub(value != 0);
    }
};

class ClearModifier : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::ClearModifier; }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->ClearModifier(context->GetStringParam());
    }
};

class ClearModifiers : public ModifierAction
{
public:
    ActionType GetType() const override { return ActionType::ClearModifiers; }

    void Do(ActionContext* context, double value) override {
        context->GetSurface()->ClearModifiers();
    }
};

class SetToggleChannel : public Action
{
public:
    ActionType GetType() const override { return ActionType::SetToggleChannel; }

    virtual void RequestUpdate(ActionContext* context) override {
        context->UpdateColorValue(0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        context->GetSurface()->ToggleChannel(context->GetWidget()->GetChannelNumber());
    }
};

class ToggleOSK : public Action
{
public:
    ActionType GetType() const override { return ActionType::ToggleOSK; }

    void RequestUpdate(ActionContext* context) override {
        context->UpdateWidgetValue(context->GetSurface()->GetOskEnabled() ? 1.0 : 0.0);
    }

    void Do(ActionContext* context, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
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
            context->GetCSI()->CloseOSKPanel();
        }
    }
};

#endif /* control_surface_manager_actions_h */
