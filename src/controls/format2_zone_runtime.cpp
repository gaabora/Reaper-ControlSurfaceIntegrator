#include "integrator.h"
#include "format2_zone_compiler.h"
#include "format2_zone_runtime.h"

struct Format2PreparedActionContext {
    Widget* widget = nullptr;
    Navigator* navigator = nullptr;
    int modifier = 0;
    int slotIndexOverride = -1;
    std::string actionName;
    std::vector<std::string> parameters;
    bool hold = false;
    bool doublePress = false;
    bool invert = false;
    bool invertFeedback = false;
    bool increase = false;
    bool decrease = false;
};

static void AddFormat2RuntimeDiagnostic(Format2ZoneRuntimeResult& result, const std::string& code, const std::string& message, const Format2SourceLocation& location) {
    result.diagnostics.push_back({code, message, location});
}

static std::string SerializeFormat2PropertyValue(const Format2ValueSyntax& value) {
    if (!value.list) return value.scalar.text;
    std::string serialized = "[ ";
    for (std::size_t itemIdx = 0; itemIdx < value.items.size(); itemIdx++) {
        if (itemIdx > 0) serialized += ", ";
        serialized += value.items[itemIdx].text;
    }
    return serialized + " ]";
}

static std::vector<std::string> MakeFormat2ActionParameters(const Format2ZoneAction& action) {
    std::vector<std::string> parameters;
    parameters.push_back(action.action);
    for (const Format2ScalarSyntax& argument : action.arguments) parameters.push_back(argument.text);
    for (const Format2PropertySyntax& property : action.properties) parameters.push_back(property.name + "=" + SerializeFormat2PropertyValue(property.value));
    return parameters;
}

static Navigator* ResolveFormat2BindingNavigator(ZoneManager* zoneManager, Zone* zone, const Format2DocumentMetadata& metadata, const std::optional<int>& surfaceChannelOffset) {
    if (metadata.role == Format2ZoneRole::Layer) return zone->GetNavigator();
    if (metadata.role == Format2ZoneRole::LastTouchedFxParam || metadata.target == Format2ZoneTarget::FocusedFx) return zoneManager->GetFocusedFXNavigator();
    if (metadata.role == Format2ZoneRole::Home || metadata.target == Format2ZoneTarget::SelectedTrack) return zoneManager->GetSelectedTrackNavigator();
    if (metadata.target == Format2ZoneTarget::MasterTrack) return zoneManager->GetMasterTrackNavigator();
    if (surfaceChannelOffset && (metadata.target == Format2ZoneTarget::Tracks || metadata.target == Format2ZoneTarget::Vca || metadata.target == Format2ZoneTarget::Folder || metadata.target == Format2ZoneTarget::SelectedTracks)) {
        const int channel = *surfaceChannelOffset + zoneManager->GetSurface()->GetChannelOffset();
        return zoneManager->GetSurface()->GetPage()->GetTrackNavigationManager()->GetNavigatorForChannel(channel);
    }
    return zone->GetNavigator();
}

static int ResolveFormat2BankOffset(ZoneManager* zoneManager, const Format2DocumentMetadata& metadata) {
    if (!metadata.target || !metadata.bankTarget) return -1;
    if (*metadata.target == Format2ZoneTarget::Tracks) {
        if (*metadata.bankTarget == Format2BankTarget::Sends) return zoneManager->GetTrackSendOffset();
        if (*metadata.bankTarget == Format2BankTarget::Receives) return zoneManager->GetTrackReceiveOffset();
        return zoneManager->GetTrackFXMenuOffset();
    }
    if (*metadata.target == Format2ZoneTarget::SelectedTrack) {
        if (*metadata.bankTarget == Format2BankTarget::Sends) return zoneManager->GetSelectedTrackSendOffset();
        if (*metadata.bankTarget == Format2BankTarget::Receives) return zoneManager->GetSelectedTrackReceiveOffset();
        return zoneManager->GetSelectedTrackFXMenuOffset();
    }
    if (*metadata.target == Format2ZoneTarget::MasterTrack && *metadata.bankTarget == Format2BankTarget::Fx) return zoneManager->GetMasterTrackFXMenuOffset();
    return -1;
}

static int ResolveFormat2SlotIndex(ZoneManager* zoneManager, const Format2DocumentMetadata& metadata, const std::optional<int>& surfaceChannelOffset) {
    const int bankOffset = ResolveFormat2BankOffset(zoneManager, metadata);
    if (bankOffset < 0) return -1;
    if (surfaceChannelOffset && (metadata.target == Format2ZoneTarget::SelectedTrack || metadata.target == Format2ZoneTarget::MasterTrack)) return bankOffset + *surfaceChannelOffset;
    return bankOffset;
}

static bool PrepareFormat2Selectors(ZoneManager* zoneManager, const Format2ZoneBinding& binding, Format2PreparedActionContext& prepared, Format2ZoneRuntimeResult& result) {
    std::vector<std::string> standardModifiers;
    for (const Format2ZoneSelector& selector : binding.selectors) {
        if (selector.kind == Format2ZoneSelectorKind::Context) {
            if (selector.name == "Touch") prepared.modifier += 1;
            else if (selector.name == "Toggle") prepared.modifier += 2;
            else if (ModifierManager::IsModifierName(selector.name.c_str())) standardModifiers.push_back(selector.name);
            else AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.pseudo-modifier", "PseudoModifier selectors are not part of the format 2 runtime yet: " + selector.name, selector.location);
            continue;
        }
        if (selector.name == "Press") continue;
        if (selector.name == "Hold") prepared.hold = true;
        else if (selector.name == "DoublePress") prepared.doublePress = true;
        else if (selector.name == "Increase") prepared.increase = true;
        else if (selector.name == "Decrease") prepared.decrease = true;
        else if (selector.name == "Invert") prepared.invert = true;
        else if (selector.name == "InvertFB") prepared.invertFeedback = true;
        else AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.input-event", "Input event is parsed but is not supported by the runtime yet: " + selector.name, selector.location);
    }
    prepared.modifier += zoneManager->GetSurface()->GetModifierManager()->GetModifierValue(standardModifiers);
    return result.IsValid();
}

Format2ZoneRuntimeResult LoadFormat2ZoneRuntimeBindings(ZoneManager* zoneManager, Zone* zone, const Format2ZoneParseResult& parsed) {
    Format2ZoneRuntimeResult result;
    const Format2ZoneCompileResult compiled = CompileFormat2ZoneBindings(parsed.zone.bindings, zoneManager->GetNumChannels());
    result.diagnostics.insert(result.diagnostics.end(), compiled.diagnostics.begin(), compiled.diagnostics.end());
    std::vector<Format2PreparedActionContext> preparedContexts;

    for (const Format2ModifierDeclaration& declaration : parsed.zone.modifiers) {
        if (declaration.kind == Format2ModifierDeclarationKind::Pseudo) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.pseudo-modifier", "PseudoModifier declarations are not part of the format 2 runtime yet", declaration.location);
            continue;
        }
        if (declaration.mode != Format2ModifierMode::Default) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.modifier-mode", "Per-modifier Mode is parsed but is not supported by the runtime yet", declaration.location);
            continue;
        }
        Widget* widget = zoneManager->GetSurface()->GetWidgetByName(declaration.widget.baseName);
        if (!widget) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.widget.missing", "Modifier Widget does not exist on the Surface: " + declaration.widget.baseName, declaration.widget.location);
            continue;
        }
        Format2PreparedActionContext prepared;
        prepared.widget = widget;
        prepared.navigator = zone->GetNavigator();
        prepared.actionName = declaration.name;
        prepared.parameters = {declaration.name};
        preparedContexts.push_back(std::move(prepared));
    }

    for (const Format2ActionContextSpec& spec : compiled.actionContexts) {
        const Format2ZoneBinding& binding = parsed.zone.bindings[spec.bindingIndex];
        Widget* widget = zoneManager->GetSurface()->GetWidgetByName(spec.widgetId);
        if (!widget) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.widget.missing", "Widget does not exist on the Surface: " + spec.widgetId, binding.widget.location);
            continue;
        }
        if (Action::NameToType(binding.action.action) == ActionType::Invalid) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.action.unknown", "Unknown runtime action: " + binding.action.action, binding.action.actionLocation);
            continue;
        }
        Format2PreparedActionContext prepared;
        prepared.widget = widget;
        prepared.navigator = ResolveFormat2BindingNavigator(zoneManager, zone, parsed.document.metadata, spec.surfaceChannelOffset);
        prepared.slotIndexOverride = ResolveFormat2SlotIndex(zoneManager, parsed.document.metadata, spec.surfaceChannelOffset);
        prepared.actionName = binding.action.action;
        prepared.parameters = MakeFormat2ActionParameters(binding.action);
        PrepareFormat2Selectors(zoneManager, binding, prepared, result);
        if (!prepared.navigator) AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.navigator.missing", "No Navigator is available for Widget: " + spec.widgetId, binding.widget.location);
        preparedContexts.push_back(std::move(prepared));
    }

    if (!result.IsValid()) return result;
    for (Format2PreparedActionContext& prepared : preparedContexts) {
        zone->AddWidget(prepared.widget);
        ActionContext* context = zone->AddActionContext(prepared.widget, prepared.modifier, zone, prepared.actionName.c_str(), prepared.parameters, prepared.navigator, prepared.slotIndexOverride);
        if (prepared.invert) context->SetIsValueInverted();
        if (prepared.invertFeedback) context->SetIsFeedbackInverted();
        if (prepared.hold) {
            context->SetHoldDelay(ActionContext::INHERIT_VALUE);
            prepared.widget->SetHasHoldActions();
        }
        if (prepared.doublePress) {
            context->SetDoublePress();
            prepared.widget->SetHasDoublePressActions();
        }
        if (prepared.increase) context->SetRange({0.0, 2.0});
        else if (prepared.decrease) context->SetRange({-2.0, 1.0});
    }
    return result;
}
