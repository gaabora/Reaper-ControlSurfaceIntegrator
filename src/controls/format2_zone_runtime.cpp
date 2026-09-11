#include "integrator.h"
#include "format2_zone_compiler.h"
#include "format2_zone_runtime.h"
#include "format2_gesture_validation.h"
#include "format2_value_validation.h"
#include "../shared/settings_values.h"

#include <array>
#include <limits>
#include <set>
#include <tuple>

struct Format2PreparedActionContext {
    Widget* widget = nullptr;
    Navigator* navigator = nullptr;
    int modifier = 0;
    int surfaceChannelOffset = -1;
    std::string actionName;
    std::vector<std::string> parameters;
    ActionInputEvent inputEvent = ActionInputEvent::Legacy;
    ActionModifierMode modifierMode = ActionModifierMode::Legacy;
    bool modifierModeUsesDefault = false;
    int eventDelayMs = 0;
    int repeatIntervalMs = 0;
    int modifierTapWindowMs = 0;
    bool invert = false;
    bool invertFeedback = false;
    bool increase = false;
    bool decrease = false;
    bool blink = false;
    int blinkIntervalMs = -1;
    std::optional<std::array<double, 2>> range;
    std::optional<double> delta;
    std::vector<double> stepValues;
    std::vector<double> accelerationDeltas;
    std::vector<int> ticksPerStep;
    std::string actionIdentity;
};

static ActionModifierMode ResolveFormat2ModifierMode(ControlSurface* surface, Format2ModifierMode mode) {
    if (mode == Format2ModifierMode::Momentary) return ActionModifierMode::Momentary;
    if (mode == Format2ModifierMode::Latch) return ActionModifierMode::Latch;
    if (mode == Format2ModifierMode::Hybrid) return ActionModifierMode::Hybrid;
    const std::string& defaultMode = surface->GetSettings().GetString("DefaultModifierMode");
    if (defaultMode == "Momentary") return ActionModifierMode::Momentary;
    if (defaultMode == "Hybrid") return ActionModifierMode::Hybrid;
    return ActionModifierMode::Latch;
}

static bool IsFormat2HoldEvent(const Format2ZoneBinding& binding) {
    for (const Format2ZoneSelector& selector : binding.selectors) {
        if (selector.kind == Format2ZoneSelectorKind::Input && (selector.name == "Hold" || selector.name == "LongHold")) return true;
    }
    return false;
}

static const Format2PropertySyntax* FindFormat2Property(const Format2ZoneAction& action, const char* propertyName) {
    for (const Format2PropertySyntax& property : action.properties) {
        if (property.name == propertyName) return &property;
    }
    return nullptr;
}

static void AddFormat2RuntimeDiagnostic(Format2ZoneRuntimeResult& result, const std::string& code, const std::string& message, const Format2SourceLocation& location, Format2DiagnosticSeverity severity = Format2DiagnosticSeverity::Error) {
    result.diagnostics.push_back({code, message, location, severity});
}

static bool ReadFormat2IntegerProperty(const Format2ZoneAction& action, const char* propertyName, int fallback, int minimum, int maximum, int& value, Format2ZoneRuntimeResult& result) {
    const Format2PropertySyntax* property = FindFormat2Property(action, propertyName);
    value = fallback;
    if (!property) return true;
    if (property->value.list || !ParseFormat2IntegerScalar(property->value.scalar, value)) {
        AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.integer-property", std::string(propertyName) + " must be one complete unquoted integer", property->value.location);
        return false;
    }
    if (value < minimum || value > maximum) {
        AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.integer-property-range", std::string(propertyName) + " must be from " + std::to_string(minimum) + " through " + std::to_string(maximum), property->value.location);
        return false;
    }
    return true;
}

static bool ReadFormat2TimingProperty(const Format2ZoneAction& action, const char* propertyName, int fallback, const char* settingName, int& value, Format2ZoneRuntimeResult& result) {
    const Settings::Definition* definition = FindSettingDefinition(settingName);
    if (!definition) {
        AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.timing-setting", std::string("Missing timing setting definition: ") + settingName, action.actionLocation);
        return false;
    }
    return ReadFormat2IntegerProperty(action, propertyName, fallback, definition->minValue, definition->maxValue, value, result);
}

static bool ReadFormat2DoubleList(const Format2ZoneAction& action, const char* propertyName, bool positiveOnly, std::vector<double>& values, Format2ZoneRuntimeResult& result) {
    const Format2PropertySyntax* property = FindFormat2Property(action, propertyName);
    if (!property) return true;
    if (!property->value.list || property->value.items.empty()) {
        AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.value-list", std::string(propertyName) + " must be one non-empty numeric list", property->value.location);
        return false;
    }
    bool valid = true;
    for (const Format2ScalarSyntax& item : property->value.items) {
        double value = 0.0;
        if (!ParseFormat2FiniteScalar(item, value) || (positiveOnly && value <= 0.0)) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.value-list-item", std::string(propertyName) + (positiveOnly ? " values must be positive finite numbers" : " values must be finite numbers"), item.location);
            valid = false;
            continue;
        }
        values.push_back(value);
    }
    return valid;
}

static bool ReadFormat2IntegerList(const Format2ZoneAction& action, const char* propertyName, std::vector<int>& values, Format2ZoneRuntimeResult& result) {
    const Format2PropertySyntax* property = FindFormat2Property(action, propertyName);
    if (!property) return true;
    if (!property->value.list || property->value.items.empty()) {
        AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.integer-list", std::string(propertyName) + " must be one non-empty integer list", property->value.location);
        return false;
    }
    bool valid = true;
    for (const Format2ScalarSyntax& item : property->value.items) {
        int value = 0;
        if (!ParseFormat2IntegerScalar(item, value) || value <= 0) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.integer-list-item", std::string(propertyName) + " values must be positive integers", item.location);
            valid = false;
            continue;
        }
        values.push_back(value);
    }
    return valid;
}

static bool PrepareFormat2ActionValues(const Format2ZoneAction& action, Format2PreparedActionContext& prepared, Format2ZoneRuntimeResult& result) {
    bool valid = true;
    const Format2PropertySyntax* range = FindFormat2Property(action, "Range");
    if (range) {
        std::vector<double> values;
        const bool listValid = ReadFormat2DoubleList(action, "Range", false, values, result);
        if (listValid && (values.size() != 2 || values[0] >= values[1])) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.range", "Range must contain two increasing finite numbers", range->value.location);
            valid = false;
        } else if (listValid) prepared.range = std::array<double, 2>{values[0], values[1]};
        else valid = false;
    }
    const Format2PropertySyntax* delta = FindFormat2Property(action, "Delta");
    if (delta) {
        double value = 0.0;
        if (delta->value.list || !ParseFormat2FiniteScalar(delta->value.scalar, value) || value <= 0.0) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.delta", "Delta must be one positive finite number", delta->value.location);
            valid = false;
        } else prepared.delta = value;
    }
    valid = ReadFormat2DoubleList(action, "StepValues", false, prepared.stepValues, result) && valid;
    valid = ReadFormat2DoubleList(action, "AccelerationDeltas", true, prepared.accelerationDeltas, result) && valid;
    valid = ReadFormat2IntegerList(action, "TicksPerStep", prepared.ticksPerStep, result) && valid;
    if (!prepared.stepValues.empty() && (prepared.delta || !prepared.accelerationDeltas.empty())) {
        AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.value-mode", "StepValues cannot be combined with Delta or AccelerationDeltas", action.actionLocation);
        valid = false;
    }
    if (!prepared.ticksPerStep.empty() && prepared.stepValues.empty()) {
        AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.ticks-without-steps", "TicksPerStep requires StepValues", action.actionLocation);
        valid = false;
    }
    if (prepared.range) {
        for (double value : prepared.stepValues) {
            if (value < (*prepared.range)[0] || value > (*prepared.range)[1]) {
                AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.step-range", "Every StepValues item must be inside Range", range->value.location);
                valid = false;
                break;
            }
        }
    }
    std::set<double> uniqueSteps;
    for (double value : prepared.stepValues) {
        if (!uniqueSteps.insert(value).second) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.step-duplicate", "StepValues cannot contain duplicates", action.actionLocation);
            valid = false;
            break;
        }
    }
    return valid;
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

static std::string ExpandFormat2ChannelArgument(const std::string& argument, const std::optional<int>& surfaceChannelOffset) {
    if (!surfaceChannelOffset || argument.empty() || (argument.back() != '#' && argument.back() != '|')) return argument;
    return argument.substr(0, argument.size() - 1) + std::to_string(*surfaceChannelOffset + 1);
}

static std::vector<std::string> MakeFormat2ActionParameters(const Format2ZoneAction& action, const std::optional<int>& surfaceChannelOffset = std::nullopt) {
    std::vector<std::string> parameters;
    parameters.push_back(action.action);
    for (const Format2ScalarSyntax& argument : action.arguments) parameters.push_back(ExpandFormat2ChannelArgument(argument.text, surfaceChannelOffset));
    const std::set<std::string> runtimeOwnedProperties = {"AccelerationDeltas", "DelayMs", "Delta", "Range", "RepeatIntervalMs", "StepValues", "TicksPerStep"};
    for (const Format2PropertySyntax& property : action.properties) if (runtimeOwnedProperties.find(property.name) == runtimeOwnedProperties.end()) parameters.push_back(property.name + "=" + SerializeFormat2PropertyValue(property.value));
    return parameters;
}

static bool HasFormat2Property(const Format2ZoneAction& action, const char* propertyName) {
    for (const Format2PropertySyntax& property : action.properties) if (property.name == propertyName) return true;
    return false;
}

static std::string MakeFormat2ActionIdentity(const Format2ZoneAction& action) {
    std::string identity = action.action;
    for (const Format2ScalarSyntax& argument : action.arguments) identity += std::string(1, '\x1f') + (argument.quoted ? "Q" : "B") + argument.text;
    std::vector<std::string> properties;
    for (const Format2PropertySyntax& property : action.properties) properties.push_back(property.name + "=" + SerializeFormat2PropertyValue(property.value));
    std::sort(properties.begin(), properties.end());
    for (const std::string& property : properties) identity += std::string(1, '\x1f') + "P" + property;
    return identity;
}

static const char* GetFormat2LifecycleWidgetName(Format2LifecycleEvent event) {
    switch (event) {
        case Format2LifecycleEvent::SurfaceInitialization: return "OnInitialization";
        case Format2LifecycleEvent::TrackSelection: return "OnTrackSelection";
        case Format2LifecycleEvent::PageEnter: return "OnPageEnter";
        case Format2LifecycleEvent::PageExit: return "OnPageLeave";
        case Format2LifecycleEvent::PlaybackStart: return "OnPlayStart";
        case Format2LifecycleEvent::PlaybackStop: return "OnPlayStop";
        case Format2LifecycleEvent::RecordStart: return "OnRecordStart";
        case Format2LifecycleEvent::RecordStop: return "OnRecordStop";
        case Format2LifecycleEvent::ZoneActivation: return "OnZoneActivation";
        case Format2LifecycleEvent::ZoneDeactivation: return "OnZoneDeactivation";
    }
    return "";
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
        if (selector.name == "Press") prepared.inputEvent = ActionInputEvent::Press;
        else if (selector.name == "Tap") prepared.inputEvent = ActionInputEvent::Tap;
        else if (selector.name == "Release") prepared.inputEvent = ActionInputEvent::Release;
        else if (selector.name == "Hold") prepared.inputEvent = ActionInputEvent::Hold;
        else if (selector.name == "LongHold") prepared.inputEvent = ActionInputEvent::LongHold;
        else if (selector.name == "DoublePress") prepared.inputEvent = ActionInputEvent::DoublePress;
        else if (selector.name == "Increase") prepared.increase = true;
        else if (selector.name == "Decrease") prepared.decrease = true;
        else if (selector.name == "Invert") prepared.invert = true;
        else if (selector.name == "InvertFB") prepared.invertFeedback = true;
        else AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.input-event", "Input event is parsed but is not supported by the runtime yet: " + selector.name, selector.location);
    }
    prepared.modifier += zoneManager->GetSurface()->GetModifierManager()->GetModifierValue(standardModifiers);
    return result.IsValid();
}

Format2ZoneRuntimeResult LoadFormat2ZoneRuntimeBindings(ZoneManager* zoneManager, Zone* zone, const Format2ZoneParseResult& parsed,
    const Format2DocumentMetadata* inheritedMetadata, const Format2ZoneRuntimeBindingSelection* selection) {
    Format2ZoneRuntimeResult result;
    const Format2DocumentMetadata& runtimeMetadata = inheritedMetadata ? *inheritedMetadata : parsed.document.metadata;
    const Format2ZoneCompileResult compiled = CompileFormat2ZoneBindings(parsed.zone.bindings, zoneManager->GetNumChannels());
    result.diagnostics.insert(result.diagnostics.end(), compiled.diagnostics.begin(), compiled.diagnostics.end());
    std::vector<Format2PreparedActionContext> preparedContexts;
    std::map<std::string, ActionModifierMode> modifierModesByWidget;
    std::map<std::pair<Widget*, int>, std::vector<Format2GestureBinding>> gestureGroups;

    for (const Format2ModifierDeclaration& declaration : parsed.zone.modifiers) {
        const bool selectedDeclaration = !selection || (selection->channelFamilyBaseName.empty() && declaration.widget.source == selection->widgetId)
            || (!selection->channelFamilyBaseName.empty() && declaration.widget.kind == Format2WidgetSelectorKind::ChannelFamily && declaration.widget.baseName == selection->channelFamilyBaseName);
        if (!selectedDeclaration) continue;
        if (declaration.kind == Format2ModifierDeclarationKind::Pseudo) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.pseudo-modifier", "PseudoModifier declarations are not part of the format 2 runtime yet", declaration.location);
            continue;
        }
        Widget* widget = zoneManager->GetSurface()->GetWidgetByName(declaration.widget.baseName);
        if (!widget) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.widget.missing", "Modifier Widget does not exist on the Surface: " + declaration.widget.baseName, declaration.widget.location, Format2DiagnosticSeverity::Warning);
            continue;
        }
        if (!widget->GetIsTwoState()) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.modifier-input", "A modifier declaration requires a Widget with press and release input", declaration.widget.location);
            continue;
        }
        Format2PreparedActionContext prepared;
        prepared.widget = widget;
        prepared.navigator = zone->GetNavigator();
        prepared.actionName = declaration.name;
        prepared.actionIdentity = "Modifier:" + declaration.name;
        prepared.parameters = {declaration.name};
        for (const Format2PropertySyntax& property : declaration.feedbackProperties) prepared.parameters.push_back(property.name + "=" + SerializeFormat2PropertyValue(property.value));
        prepared.inputEvent = ActionInputEvent::Modifier;
        prepared.modifierMode = ResolveFormat2ModifierMode(zoneManager->GetSurface(), declaration.mode);
        prepared.modifierModeUsesDefault = declaration.mode == Format2ModifierMode::Default;
        prepared.modifierTapWindowMs = zoneManager->GetSurface()->GetSettings().GetInteger("ModifierTapWindowMs");
        prepared.blink = declaration.blink;
        prepared.blinkIntervalMs = declaration.blinkIntervalMs;
        modifierModesByWidget[declaration.widget.baseName] = prepared.modifierMode;
        gestureGroups[{widget, 0}].push_back({{prepared.inputEvent, prepared.modifierMode, 0, 0, prepared.modifierTapWindowMs}, prepared.actionName, declaration.location, "Modifier:" + prepared.actionName, false, false, 1, true, false});
        preparedContexts.push_back(std::move(prepared));
    }

    for (const Format2LifecycleBlock& block : parsed.zone.lifecycleBlocks) {
        if (selection) continue;
        Widget* widget = zoneManager->GetSurface()->GetWidgetByName(GetFormat2LifecycleWidgetName(block.event));
        if (!widget) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.lifecycle-widget", "The internal lifecycle Widget is not available on the Surface", block.location);
            continue;
        }
        for (const Format2ZoneAction& action : block.actions) {
            if (Action::NameToType(action.action) == ActionType::Invalid) {
                AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.action.unknown", "Unknown runtime action: " + action.action, action.actionLocation);
                continue;
            }
            Format2PreparedActionContext prepared;
            prepared.widget = widget;
            prepared.navigator = zone->GetNavigator();
            prepared.actionName = action.action;
            prepared.actionIdentity = MakeFormat2ActionIdentity(action);
            prepared.parameters = MakeFormat2ActionParameters(action);
            preparedContexts.push_back(std::move(prepared));
        }
    }

    for (const Format2ActionContextSpec& spec : compiled.actionContexts) {
        const Format2ZoneBinding& binding = parsed.zone.bindings[spec.bindingIndex];
        if (selection) {
            const bool selectedExactWidget = selection->channelFamilyBaseName.empty() && spec.widgetId == selection->widgetId;
            const bool selectedChannelFamily = !selection->channelFamilyBaseName.empty() && binding.widget.kind == Format2WidgetSelectorKind::ChannelFamily && binding.widget.baseName == selection->channelFamilyBaseName;
            if (!selectedExactWidget && !selectedChannelFamily) continue;
        }
        const auto modifierMode = modifierModesByWidget.find(binding.widget.baseName);
        if (modifierMode != modifierModesByWidget.end() && modifierMode->second != ActionModifierMode::Latch && IsFormat2HoldEvent(binding)) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.modifier-hold", "Hold and LongHold cannot use a Momentary or Hybrid modifier source Widget", binding.location);
            continue;
        }
        Widget* widget = zoneManager->GetSurface()->GetWidgetByName(spec.widgetId);
        if (!widget) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.widget.missing", "Widget does not exist on the Surface: " + spec.widgetId, binding.widget.location, Format2DiagnosticSeverity::Warning);
            continue;
        }
        if (Action::NameToType(binding.action.action) == ActionType::Invalid) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.action.unknown", "Unknown runtime action: " + binding.action.action, binding.action.actionLocation);
            continue;
        }
        Format2PreparedActionContext prepared;
        const std::size_t bindingDiagnosticStart = result.diagnostics.size();
        prepared.widget = widget;
        prepared.navigator = ResolveFormat2BindingNavigator(zoneManager, zone, runtimeMetadata, spec.surfaceChannelOffset);
        prepared.surfaceChannelOffset = spec.surfaceChannelOffset ? *spec.surfaceChannelOffset : -1;
        prepared.actionName = binding.action.action;
        prepared.actionIdentity = MakeFormat2ActionIdentity(binding.action);
        prepared.parameters = MakeFormat2ActionParameters(binding.action, spec.surfaceChannelOffset);
        PrepareFormat2Selectors(zoneManager, binding, prepared, result);
        if (prepared.inputEvent == ActionInputEvent::Legacy && !prepared.increase && !prepared.decrease && widget->GetIsTwoState()) prepared.inputEvent = zoneManager->GetSurface()->GetSettings().GetString("DefaultButtonTrigger") == "Tap" ? ActionInputEvent::Tap : ActionInputEvent::Press;
        if (prepared.inputEvent != ActionInputEvent::Legacy && !widget->GetIsTwoState()) {
            AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.button-input", "A button event requires a Widget with press and release input: " + spec.widgetId, binding.widget.location);
            continue;
        }
        bool integerPropertiesValid = true;
        if (prepared.inputEvent == ActionInputEvent::Hold) integerPropertiesValid = ReadFormat2TimingProperty(binding.action, "DelayMs", zoneManager->GetSurface()->GetSettings().GetInteger("HoldDelayMs"), "HoldDelayMs", prepared.eventDelayMs, result) && integerPropertiesValid;
        else if (prepared.inputEvent == ActionInputEvent::LongHold) integerPropertiesValid = ReadFormat2TimingProperty(binding.action, "DelayMs", zoneManager->GetSurface()->GetSettings().GetInteger("LongHoldDelayMs"), "LongHoldDelayMs", prepared.eventDelayMs, result) && integerPropertiesValid;
        else if (HasFormat2Property(binding.action, "DelayMs")) integerPropertiesValid = ReadFormat2IntegerProperty(binding.action, "DelayMs", 0, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)(), prepared.eventDelayMs, result) && integerPropertiesValid;
        if (prepared.inputEvent == ActionInputEvent::Hold || prepared.inputEvent == ActionInputEvent::LongHold) integerPropertiesValid = ReadFormat2TimingProperty(binding.action, "RepeatIntervalMs", 0, "HoldRepeatIntervalMs", prepared.repeatIntervalMs, result) && integerPropertiesValid;
        else if (HasFormat2Property(binding.action, "RepeatIntervalMs")) integerPropertiesValid = ReadFormat2IntegerProperty(binding.action, "RepeatIntervalMs", 0, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)(), prepared.repeatIntervalMs, result) && integerPropertiesValid;
        int runCount = 1;
        integerPropertiesValid = ReadFormat2IntegerProperty(binding.action, "RunCount", 1, 1, (std::numeric_limits<int>::max)(), runCount, result) && integerPropertiesValid;
        integerPropertiesValid = PrepareFormat2ActionValues(binding.action, prepared, result) && integerPropertiesValid;
        if (prepared.inputEvent != ActionInputEvent::Legacy && integerPropertiesValid) {
            gestureGroups[{widget, prepared.modifier}].push_back({{prepared.inputEvent, prepared.modifierMode, prepared.eventDelayMs, prepared.repeatIntervalMs, prepared.modifierTapWindowMs}, prepared.actionName, binding.location, MakeFormat2ActionIdentity(binding.action), HasFormat2Property(binding.action, "DelayMs"), HasFormat2Property(binding.action, "RepeatIntervalMs"), runCount, Format2ActionChangesModifier(prepared.actionName), binding.modifierSource.has_value()});
        }
        if (!prepared.navigator) AddFormat2RuntimeDiagnostic(result, "format2.zone.runtime.navigator.missing", "This binding is ignored because no Navigator is available for Widget: " + spec.widgetId, binding.widget.location);
        const bool bindingHasErrors = std::any_of(result.diagnostics.begin() + bindingDiagnosticStart, result.diagnostics.end(), [](const Format2Diagnostic& diagnostic) { return diagnostic.severity == Format2DiagnosticSeverity::Error; });
        if (bindingHasErrors) continue;
        preparedContexts.push_back(std::move(prepared));
    }

    for (const auto& group : gestureGroups) {
        const auto diagnostics = ValidateFormat2GestureBindings(group.second, zoneManager->GetSurface()->GetDoublePressTime(), zoneManager->GetSurface()->GetSettings().GetString("DoublePressPolicy") == "Exclusive");
        result.diagnostics.insert(result.diagnostics.end(), diagnostics.begin(), diagnostics.end());
    }
    if (selection) {
        if (selection->channelFamilyBaseName.empty()) {
            Widget* selectedWidget = zoneManager->GetSurface()->GetWidgetByName(selection->widgetId);
            if (selectedWidget) zone->ClearActionContexts(selectedWidget);
        } else {
            for (int channel = 1; channel <= zoneManager->GetNumChannels(); ++channel) {
                Widget* selectedWidget = zoneManager->GetSurface()->GetWidgetByName(selection->channelFamilyBaseName + std::to_string(channel));
                if (selectedWidget) zone->ClearActionContexts(selectedWidget);
            }
        }
    }
    std::set<std::tuple<Widget*, int, ActionInputEvent, std::string>> addedContexts;
    for (Format2PreparedActionContext& prepared : preparedContexts) {
        const auto contextKey = std::make_tuple(prepared.widget, prepared.modifier, prepared.inputEvent, prepared.actionIdentity);
        if (!prepared.actionIdentity.empty() && !addedContexts.insert(contextKey).second) continue;
        zone->AddWidget(prepared.widget);
        ActionContext* context = zone->AddActionContext(prepared.widget, prepared.modifier, zone, prepared.actionName.c_str(), prepared.parameters, prepared.navigator, prepared.surfaceChannelOffset);
        context->SetInputEvent(prepared.inputEvent);
        context->SetModifierMode(prepared.modifierMode);
        context->SetModifierModeUsesDefault(prepared.modifierModeUsesDefault);
        context->SetModifierTapWindow(prepared.modifierTapWindowMs);
        if (prepared.blink) context->SetBlinkInterval(prepared.blinkIntervalMs);
        if (prepared.invert) context->SetIsValueInverted();
        if (prepared.invertFeedback) context->SetIsFeedbackInverted();
        if (prepared.inputEvent == ActionInputEvent::Hold || prepared.inputEvent == ActionInputEvent::LongHold) {
            context->SetHoldDelay(prepared.eventDelayMs);
            context->SetHoldRepeatInterval(prepared.repeatIntervalMs);
        }
        if (prepared.inputEvent == ActionInputEvent::DoublePress) {
            context->SetDoublePress();
        }
        if (prepared.increase) context->SetRange({0.0, 2.0});
        else if (prepared.decrease) context->SetRange({-2.0, 1.0});
        else if (prepared.range) context->SetRange({(*prepared.range)[0], (*prepared.range)[1]});
        if (prepared.delta) context->SetDeltaValue(*prepared.delta);
        if (!prepared.stepValues.empty()) context->SetStepValues(prepared.stepValues);
        if (!prepared.accelerationDeltas.empty()) context->SetAccelerationValues(prepared.accelerationDeltas);
        if (!prepared.ticksPerStep.empty()) context->SetTickCounts(prepared.ticksPerStep);
    }
    return result;
}
