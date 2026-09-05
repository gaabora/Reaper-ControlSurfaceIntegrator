// osk.cpp — ControlSurface OSK (On-Screen Keyboard) member implementations.

#include "integrator.h"
#include "format2_action_metadata.h"
#include "format2_gesture_validation.h"
#include "format2_surface_document.h"
#include "zone_file_creator.h"

static string BuildTimestampForBackup() {
    time_t rawTime = time(nullptr);
    struct tm timeInfo;
#ifdef _WIN32
    localtime_s(&timeInfo, &rawTime);
#else
    localtime_r(&rawTime, &timeInfo);
#endif
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &timeInfo);
    return string(buffer);
}

static string SanitizeConfigStatusField(const string& value) {
    string sanitized = value;
    ReplaceAllWith(sanitized, "|", "/");
    ReplaceAllWith(sanitized, "\r", " ");
    ReplaceAllWith(sanitized, "\n", " ");
    return sanitized;
}

static void PublishConfigStatus(const string& outcome, const string& operation, const string& surfaceName, const string& widgetName, const string& zoneName, const string& message) {
    const string status = SanitizeConfigStatusField(outcome)
        + "|" + SanitizeConfigStatusField(operation)
        + "|" + SanitizeConfigStatusField(surfaceName)
        + "|" + SanitizeConfigStatusField(widgetName)
        + "|" + SanitizeConfigStatusField(zoneName)
        + "|" + SanitizeConfigStatusField(message);
    const string scopedKey = "ConfigStatus_" + surfaceName + "_" + widgetName;
    ::SetExtState(ProductIdentity::ExtStateOsk, scopedKey.c_str(), status.c_str(), false);
}

static void PublishZoneCreateStatus(const string& surfaceName, const string& outcome, const string& path, const string& message) {
    const string status = SanitizeConfigStatusField(outcome) + "|" + SanitizeConfigStatusField(path) + "|" + SanitizeConfigStatusField(message);
    const string scopedKey = "ZoneCreateStatus_" + surfaceName;
    ::SetExtState(ProductIdentity::ExtStateOsk, scopedKey.c_str(), status.c_str(), false);
}

static string GetConfiguredOskLabel(ActionContext* context) {
    if (!context) return "";
    if (const char* keyLabel = context->GetWidgetProperties().get_prop(PropertyType_KeyLabel))
        if (keyLabel[0] != '\0') return keyLabel;
    if (const char* osdLabel = context->GetWidgetProperties().get_prop(PropertyType_OSD))
        if (osdLabel[0] != '\0' && !IsSameString(osdLabel, "No") && !IsSameString(osdLabel, "?")) return osdLabel;
    return "";
}

static string GetConfiguredOskKeyLabel(ActionContext* context) {
    if (!context) return "";
    if (const char* keyLabel = context->GetWidgetProperties().get_prop(PropertyType_KeyLabel))
        if (keyLabel[0] != '\0') return keyLabel;
    return "";
}

static double ClampOskNormalizedValue(double value) {
    return (std::max)(0.0, (std::min)(value, 1.0));
}

static bool IsOskContinuousValueAction(Action* action) {
    if (!action || action->IsDisplayRelated()) return false;
    return action->IsVolumeRelated() || action->IsPanRelated() || action->IsFxRelated() || action->IsTrackSendRelated() || action->IsTrackReceiveRelated() || action->IsMeterRelated();
}

static bool IsOskColorMeaningful(const rgba_color& color) {
    return color.r > 10 || color.g > 10 || color.b > 10;
}

static bool IsOskContinuousWidgetRole(const string& role) {
    return IsSameString(role.c_str(), "fader") || IsSameString(role.c_str(), "rotary");
}

static const char* GetOskContinuousKind(Action* action) {
    if (!action) return "";
    if (action->IsVolumeRelated()) return "V";
    if (action->IsPanRelated()) return "P";
    return "";
}

static double MapOskContinuousValueToAction(Action* action, double value) {
    if (!action) return value;
    const bool isNormalizedValue = value >= 0.0 && value <= 1.0;

    switch (action->GetType()) {
        case ActionType::TrackVolumeDB:
        case ActionType::TrackSendVolumeDB:
        case ActionType::TrackReceiveVolumeDB:
            return isNormalizedValue ? VAL2DB(normalizedToVol(value)) : value;
        case ActionType::TrackPanPercent:
        case ActionType::TrackPanWidthPercent:
        case ActionType::TrackPanLPercent:
        case ActionType::TrackPanRPercent:
        case ActionType::TrackSendPanPercent:
        case ActionType::TrackReceivePanPercent:
            return isNormalizedValue ? normalizedToPan(value) * 100.0 : value;
        default:
            return value;
    }
}

static string QuoteZoneToken(const string& token) {
    if (token.find_first_of(" \t") == string::npos) return token;

    const size_t equalsPosition = token.find('=');
    if (equalsPosition != string::npos) {
        return token.substr(0, equalsPosition + 1) + "\"" + token.substr(equalsPosition + 1) + "\"";
    }
    return "\"" + token + "\"";
}

static string SerializeContextAction(ActionContext* context) {
    if (!context) return "";

    string serialized;
    const auto& sourceParams = context->GetSourceParams();
    if (sourceParams.empty())
        return context->GetAction()->GetName();

    for (const auto& token : sourceParams) {
        if (!serialized.empty()) serialized += " ";
        serialized += QuoteZoneToken(token);
    }
    return serialized;
}

static bool HasBalancedBindingSyntax(const string& actionText, string& errorMessage) {
    bool insideQuote = false;
    int braceDepth = 0;
    int bracketDepth = 0;

    for (const char character : actionText) {
        if (character == '"') {
            insideQuote = !insideQuote;
        }
        if (insideQuote) continue;

        if (character == '{') ++braceDepth;
        else if (character == '}') --braceDepth;
        else if (character == '[') ++bracketDepth;
        else if (character == ']') --bracketDepth;

        if (braceDepth < 0 || bracketDepth < 0) {
            errorMessage = "Unbalanced color or value block";
            return false;
        }
    }

    if (insideQuote) errorMessage = "Unterminated quoted value";
    else if (braceDepth != 0) errorMessage = "Unbalanced color block";
    else if (bracketDepth != 0) errorMessage = "Unbalanced value block";
    else return true;
    return false;
}

struct OskConfigBinding {
    int modifierValue = 0;
    vector<string> actionTokens;
    bool hasHold = false;
    bool hasDoublePress = false;
    bool isValueInverted = false;
    bool isFeedbackInverted = false;
    bool isIncrease = false;
    bool isDecrease = false;
};

static bool IsOskMetadataToken(const string& token, OskConfigBinding& binding) {
    if (token == "__OSK_HOLD") binding.hasHold = true;
    else if (token == "__OSK_DOUBLE_PRESS") binding.hasDoublePress = true;
    else if (token == "__OSK_INVERT") binding.isValueInverted = true;
    else if (token == "__OSK_INVERT_FB") binding.isFeedbackInverted = true;
    else if (token == "__OSK_INCREASE") binding.isIncrease = true;
    else if (token == "__OSK_DECREASE") binding.isDecrease = true;
    else return false;
    return true;
}

static bool ParseConfigBindings(CSurfIntegrator* csi, const string& bindingData, vector<OskConfigBinding>& bindings, string& errorMessage) {
    bindings.clear();
    if (bindingData.empty()) return true;
    if (bindingData.find_first_of("\r\n") != string::npos) {
        errorMessage = "Bindings cannot contain line breaks";
        return false;
    }

    vector<string> serializedBindings;
    GetTokens(serializedBindings, bindingData, ';');
    for (int bindingIdx = 0; bindingIdx < (int) serializedBindings.size(); ++bindingIdx) {
        const string& serializedBinding = serializedBindings[bindingIdx];
        if (serializedBinding.empty()) {
            errorMessage = "Empty binding at position " + to_string(bindingIdx + 1);
            return false;
        }

        const size_t separatorPosition = serializedBinding.find(':');
        if (separatorPosition == string::npos) {
            errorMessage = "Missing modifier separator at binding " + to_string(bindingIdx + 1);
            return false;
        }

        const string modifierText = serializedBinding.substr(0, separatorPosition);
        const string actionText = serializedBinding.substr(separatorPosition + 1);
        if (actionText.empty()) {
            errorMessage = "Missing action at binding " + to_string(bindingIdx + 1);
            return false;
        }
        if (!HasBalancedBindingSyntax(actionText, errorMessage)) {
            errorMessage += " at binding " + to_string(bindingIdx + 1);
            return false;
        }

        OskConfigBinding binding;
        size_t parsedCharacters = 0;
        try {
            binding.modifierValue = std::stoi(modifierText, &parsedCharacters);
        } catch (...) {
            errorMessage = "Invalid modifier at binding " + to_string(bindingIdx + 1);
            return false;
        }
        if (parsedCharacters != modifierText.size() || binding.modifierValue < 0) {
            errorMessage = "Invalid modifier at binding " + to_string(bindingIdx + 1);
            return false;
        }

        vector<string> parsedTokens;
        GetTokens(parsedTokens, actionText);
        for (const auto& token : parsedTokens)
            if (!IsOskMetadataToken(token, binding))
                binding.actionTokens.push_back(token);

        if (binding.actionTokens.empty() || binding.actionTokens[0].empty()) {
            errorMessage = "Missing action at binding " + to_string(bindingIdx + 1);
            return false;
        }
        if (csi->GetAction(binding.actionTokens[0].c_str())->GetType() == ActionType::InvalidAction) {
            errorMessage = "Unknown action '" + binding.actionTokens[0] + "' at binding " + to_string(bindingIdx + 1);
            return false;
        }
        if (binding.isIncrease && binding.isDecrease) {
            errorMessage = "Binding cannot be both Increase and Decrease at position " + to_string(bindingIdx + 1);
            return false;
        }
        bindings.push_back(binding);
    }
    return true;
}

static string OskConfigActionIdentity(const OskConfigBinding& binding) {
    string identity;
    for (const string& token : binding.actionTokens) {
        if (!identity.empty()) identity += "\x1f";
        identity += token;
    }
    return identity;
}

static vector<Format2Diagnostic> ValidateOskConfigGestures(Widget* widget, const vector<OskConfigBinding>& bindings, const SettingsValues& settings, int doublePressWindowMs) {
    map<int, vector<Format2GestureBinding>> gestureGroups;
    vector<Format2Diagnostic> diagnostics;
    for (size_t bindingIdx = 0; bindingIdx < bindings.size(); ++bindingIdx) {
        const OskConfigBinding& binding = bindings[bindingIdx];
        Format2SourceLocation location;
        location.line = static_cast<int>(bindingIdx + 1);
        if (binding.hasHold && binding.hasDoublePress) {
            diagnostics.push_back({"format2.zone.binding.button-event", "A binding cannot contain both Hold and DoublePress", location});
            continue;
        }
        ActionInputEvent inputEvent = ActionInputEvent::Legacy;
        if (binding.hasHold) inputEvent = ActionInputEvent::Hold;
        else if (binding.hasDoublePress) inputEvent = ActionInputEvent::DoublePress;
        else if (!binding.isIncrease && !binding.isDecrease && widget->GetIsTwoState()) inputEvent = settings.GetString("DefaultButtonTrigger") == "Tap" ? ActionInputEvent::Tap : ActionInputEvent::Press;
        if (inputEvent == ActionInputEvent::Legacy) continue;
        Format2GestureBinding gestureBinding;
        gestureBinding.gesture.inputEvent = inputEvent;
        if (inputEvent == ActionInputEvent::Hold) gestureBinding.gesture.delayMs = settings.GetInteger("HoldDelayMs");
        gestureBinding.actionName = binding.actionTokens[0];
        gestureBinding.location = location;
        gestureBinding.actionIdentity = OskConfigActionIdentity(binding);
        gestureBinding.changesModifier = Format2ActionChangesModifier(gestureBinding.actionName);
        gestureGroups[binding.modifierValue].push_back(std::move(gestureBinding));
    }
    const bool exclusiveDoublePress = settings.GetString("DoublePressPolicy") == "Exclusive";
    for (const auto& gestureGroup : gestureGroups) {
        const vector<Format2Diagnostic> groupDiagnostics = ValidateFormat2GestureBindings(gestureGroup.second, doublePressWindowMs, exclusiveDoublePress);
        diagnostics.insert(diagnostics.end(), groupDiagnostics.begin(), groupDiagnostics.end());
    }
    return diagnostics;
}

static string FormatOskConfigDiagnostics(const vector<Format2Diagnostic>& diagnostics, Format2DiagnosticSeverity severity) {
    const Format2Diagnostic* first = nullptr;
    int count = 0;
    for (const Format2Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity != severity) continue;
        if (!first) first = &diagnostic;
        count++;
    }
    if (!first) return "";
    string message = first->message;
    ReplaceAllWith(message, "line ", "binding ");
    string result = first->code + " at binding " + to_string(first->location.line) + ": " + message;
    if (count > 1) result += " (and " + to_string(count - 1) + " more)";
    return result;
}

static vector<OskConfigBinding> CaptureConfigBindings(Zone* zone, Widget* widget) {
    vector<OskConfigBinding> bindings;
    if (!zone || !widget) return bindings;

    map<int, const vector<unique_ptr<ActionContext>>*> modifierContexts;
    zone->GetAllModifierContexts(widget, modifierContexts);
    for (const auto& [modifierValue, contexts] : modifierContexts) {
        for (const auto& contextPtr : *contexts) {
            ActionContext* context = contextPtr.get();
            if (!context) continue;

            OskConfigBinding binding;
            binding.modifierValue = modifierValue;
            binding.actionTokens = context->GetSourceParams();
            if (binding.actionTokens.empty()) binding.actionTokens.push_back(context->GetAction()->GetName());
            binding.hasHold = context->GetHoldDelay() > 0;
            binding.hasDoublePress = context->IsDoublePress();
            binding.isValueInverted = context->GetIsValueInverted();
            binding.isFeedbackInverted = context->GetIsFeedbackInverted();
            binding.isDecrease = context->GetRangeMinimum() == -2.0 && context->GetRangeMaximum() == 1.0;
            binding.isIncrease = context->GetRangeMinimum() == 0.0 && context->GetRangeMaximum() == 2.0;
            bindings.push_back(binding);
        }
    }
    return bindings;
}

static void ApplyConfigBindings(Zone* zone, Widget* widget, const vector<OskConfigBinding>& bindings) {
    zone->ClearActionContexts(widget);
    for (const auto& binding : bindings) {
        vector<string> actionTokens = binding.actionTokens;
        ActionContext* context = zone->AddActionContext(widget, binding.modifierValue, zone, actionTokens[0].c_str(), actionTokens);
        if (!context) throw std::runtime_error("Action context creation failed");

        if (binding.isValueInverted) context->SetIsValueInverted();
        if (binding.isFeedbackInverted) context->SetIsFeedbackInverted();
        if (binding.hasHold) {
            if (context->GetHoldDelay() == 0) context->SetHoldDelay(ActionContext::INHERIT_VALUE);
            widget->SetHasHoldActions();
        }
        if (binding.hasDoublePress) {
            context->SetDoublePress();
            widget->SetHasDoublePressActions();
        }
        if (binding.isDecrease) context->SetRange({ -2.0, 1.0 });
        else if (binding.isIncrease) context->SetRange({ 0.0, 2.0 });
    }
}

static string BuildWidgetTokenPrefix(int modifierValue, ActionContext* context) {
    string prefix;

    if (context && context->IsDoublePress()) prefix += "DoublePress+";
    if (context && context->GetHoldDelay() > 0) prefix += "Hold+";
    if (context && context->GetIsValueInverted()) prefix += "Invert+";
    if (context && context->GetIsFeedbackInverted()) prefix += "InvertFB+";
    if (context && context->GetRangeMinimum() == -2.0 && context->GetRangeMaximum() == 1.0) prefix += "Decrease+";
    else if (context && context->GetRangeMinimum() == 0.0 && context->GetRangeMaximum() == 2.0) prefix += "Increase+";

    if (modifierValue != 0) {
        char modifierBuffer[128];
        ModifierManager::GetModifierString(modifierValue, modifierBuffer, sizeof(modifierBuffer));
        prefix += modifierBuffer;
    }

    return prefix;
}

static string BuildOskTooltipModifierName(int modifierValue, ActionContext* context) {
    string modifierName;

    if (context && context->IsDoublePress()) modifierName += "DoublePress+";
    if (context && context->GetHoldDelay() > 0) modifierName += "Hold+";

    if (modifierValue != 0) {
        char modifierBuffer[128];
        ModifierManager::GetModifierString(modifierValue, modifierBuffer, sizeof(modifierBuffer));
        modifierName += modifierBuffer;
    }

    while (!modifierName.empty() && modifierName.back() == '+') modifierName.pop_back();
    return modifierName.empty() ? "NoMod" : modifierName;
}

static bool IsPseudoModifierToken(const string& token) {
    return IsSameString(token, "Hold") || IsSameString(token, "DoublePress") || IsSameString(token, "Increase") || IsSameString(token, "Decrease");
}

static string ExtractWidgetNameFromZoneToken(const string& token) {
    vector<string> parts;
    GetTokens(parts, token, '+');
    for (int idx = (int) parts.size() - 1; idx >= 0; --idx) {
        const string& candidate = parts[idx];
        if (ModifierManager::IsModifierName(candidate.c_str())) continue;
        if (IsPseudoModifierToken(candidate)) continue;
        return candidate;
    }
    return "";
}

static vector<string> BuildSerializedWidgetLines(Zone* zone, Widget* widget, const string& widgetName) {
    vector<string> lines;
    if (!zone || !widget) return lines;

    map<int, const vector<unique_ptr<ActionContext>>*> modifierContexts;
    zone->GetAllModifierContexts(widget, modifierContexts);

    for (const auto& [modifierValue, contexts] : modifierContexts) {
        for (const auto& contextPtr : *contexts) {
            ActionContext* context = contextPtr.get();
            if (!context) continue;

            string line = "  ";
            line += BuildWidgetTokenPrefix(modifierValue, context);
            line += widgetName;
            line += "  ";

            line += SerializeContextAction(context);

            lines.push_back(line);
        }
    }

    return lines;
}

static string FindInlineZoneComment(const string& line) {
    bool insideQuote = false;
    for (size_t characterIdx = 0; characterIdx + 1 < line.size(); ++characterIdx) {
        const char character = line[characterIdx];
        if (character == '"') insideQuote = !insideQuote;
        if (!insideQuote && character == '/' && line[characterIdx + 1] == '/')
            return line.substr(characterIdx);
    }
    return "";
}

static vector<string> CollectWidgetInlineComments(const vector<string>& originalLines, const string& targetZoneName, const string& widgetName) {
    vector<string> comments;
    bool inTargetZone = false;

    for (const auto& rawLine : originalLines) {
        string trimmed = rawLine;
        TrimLine(trimmed);

        if (!inTargetZone && trimmed.rfind("Zone ", 0) == 0) {
            vector<string> zoneTokens;
            GetTokens(zoneTokens, trimmed);
            inTargetZone = zoneTokens.size() >= 2 && IsSameString(zoneTokens[1], targetZoneName);
        }
        if (inTargetZone && IsSameString(trimmed, "ZoneEnd"))
            break;
        if (!inTargetZone || trimmed.empty() || IsCommentedOrEmpty(trimmed))
            continue;

        vector<string> lineTokens;
        GetTokens(lineTokens, trimmed);
        if (lineTokens.empty()) continue;
        if (!IsSameString(ExtractWidgetNameFromZoneToken(lineTokens[0]), widgetName))
            continue;

        comments.push_back(FindInlineZoneComment(rawLine));
    }
    return comments;
}

static void ApplyInlineComments(vector<string>& replacementLines, const vector<string>& comments) {
    const size_t sharedCount = (std::min)(replacementLines.size(), comments.size());
    for (size_t commentIdx = 0; commentIdx < sharedCount; ++commentIdx)
        if (!comments[commentIdx].empty())
            replacementLines[commentIdx] += " " + comments[commentIdx];

    for (size_t commentIdx = sharedCount; commentIdx < comments.size(); ++commentIdx)
        if (!comments[commentIdx].empty())
            replacementLines.push_back("  " + comments[commentIdx]);
}

static bool WriteLinesToFile(const string& filePath, const vector<string>& lines, string& errorMessage) {
    ofstream outputFile(filePath, std::ios::trunc);
    if (!outputFile.is_open()) {
        errorMessage = "Unable to open temporary zone file for write";
        return false;
    }

    for (const auto& line : lines) {
        outputFile << line << "\n";
        if (!outputFile.good()) {
            errorMessage = "Failed while writing temporary zone file";
            outputFile.close();
            return false;
        }
    }
    outputFile.close();
    if (!outputFile.good()) {
        errorMessage = "Failed while closing temporary zone file";
        return false;
    }
    return true;
}

static bool CommitZoneFile(const string& zonePath, const vector<string>& updatedLines, string& backupPath, string& errorMessage) {
    const string timestamp = BuildTimestampForBackup();
    const string temporaryPath = zonePath + ".tmp." + timestamp;
    backupPath = zonePath + "~" + timestamp;

    std::error_code fileError;
    filesystem::remove(temporaryPath, fileError);
    fileError.clear();

    if (!WriteLinesToFile(temporaryPath, updatedLines, errorMessage)) {
        filesystem::remove(temporaryPath, fileError);
        return false;
    }

    filesystem::copy_file(zonePath, backupPath, filesystem::copy_options::overwrite_existing, fileError);
    if (fileError) {
        errorMessage = "Backup failed: " + fileError.message();
        filesystem::remove(temporaryPath, fileError);
        filesystem::remove(backupPath, fileError);
        return false;
    }

    filesystem::remove(zonePath, fileError);
    if (fileError) {
        errorMessage = "Unable to replace zone file: " + fileError.message();
        filesystem::remove(temporaryPath, fileError);
        filesystem::remove(backupPath, fileError);
        return false;
    }

    filesystem::rename(temporaryPath, zonePath, fileError);
    if (!fileError) return true;

    const string replaceError = fileError.message();
    fileError.clear();
    filesystem::copy_file(backupPath, zonePath, filesystem::copy_options::overwrite_existing, fileError);
    filesystem::remove(temporaryPath, fileError);
    filesystem::remove(backupPath, fileError);
    errorMessage = "Zone replacement failed: " + replaceError;
    return false;
}

static string EscapeOskLayoutValue(const string& value) {
    const bool needsQuotes = value.find_first_of(",|\"\\\n\r") != string::npos;
    if (!needsQuotes) return value;

    string result = "\"";
    for (const char ch : value) {
        if (ch == '\\') result += "\\\\";
        else if (ch == '"') result += "\\\"";
        else if (ch == '\n') result += "\\n";
        else if (ch == '\r') result += "\\r";
        else result += ch;
    }
    result += "\"";
    return result;
}

static string JoinOskList(const vector<string>& values) {
    string result;
    for (const auto& value : values) {
        if (value.empty()) continue;
        if (!result.empty()) result += "+";
        result += value;
    }
    return result;
}

void ControlSurface::ApplyOskWidgetMetadata(OskWidgetInfo& info) {
    Widget* widget = this->GetWidgetByName(info.name);
    if (!widget) return;

    if (info.widgetClass.empty()) info.widgetClass = widget->GetOskWidgetClass();
    if (info.role.empty()) info.role = widget->GetOskRole();

    if (info.input.empty()) {
        vector<string> inputTypes;
        if (widget->HasOskPressInput()) inputTypes.push_back("press");
        if (widget->HasOskRelativeInput()) inputTypes.push_back("relative");
        if (widget->HasOskAbsoluteInput()) inputTypes.push_back("absolute");
        if (widget->HasOskTouchInput()) inputTypes.push_back("touch");
        info.input = JoinOskList(inputTypes);
    }

    if (info.feedback.empty()) {
        vector<string> feedbackTypes;
        if (widget->HasOskValueFeedback()) feedbackTypes.push_back("value");
        if (widget->HasOskToggleFeedback()) feedbackTypes.push_back("toggle");
        if (widget->HasOskColorFeedback()) feedbackTypes.push_back("color");
        if (widget->HasOskTextFeedback()) feedbackTypes.push_back("text");
        if (widget->HasOskMeterFeedback()) feedbackTypes.push_back("meter");
        info.feedback = JoinOskList(feedbackTypes);
    }

    if (info.pressTarget.empty() && widget->HasOskPressInput()) info.pressTarget = info.name;
    if (info.scrollTarget.empty() && widget->HasOskRelativeInput()) info.scrollTarget = info.name;
    if (info.valueTarget.empty() && widget->HasOskAbsoluteInput()) info.valueTarget = info.name;
    if (info.touchTarget.empty() && widget->HasOskTouchInput()) info.touchTarget = info.name;
    if (info.rotaryStyle.empty() && IsSameString(info.role.c_str(), "rotary")) info.rotaryStyle = "wiper";
}

void ControlSurface::ApplyGroupedOskTargets(const vector<OskWidgetInfo>& hiddenWidgets) {
    for (auto& row : this->oskLayout_) {
        for (auto& cell : row.cells) {
            if (cell.isSpacer || !cell.widget.pressTarget.empty() || cell.widget.group.empty() || !IsSameString(cell.widget.role.c_str(), "rotary")) continue;

            string targetName;
            int targetCount = 0;
            for (const auto& hiddenWidget : hiddenWidgets) {
                if (!IsSameString(hiddenWidget.group.c_str(), cell.widget.group.c_str())) continue;
                Widget* hiddenNativeWidget = this->GetWidgetByName(hiddenWidget.name);
                if (!hiddenNativeWidget || !hiddenNativeWidget->HasOskPressInput()) continue;
                targetName = hiddenWidget.name;
                ++targetCount;
            }

            if (targetCount == 1) cell.widget.pressTarget = targetName;
        }
    }
}

void ControlSurface::BuildCachedLayoutString() {
    this->cachedOskLayoutString_.clear();

    for (const auto& row : this->oskLayout_) {
        if (!this->cachedOskLayoutString_.empty()) this->cachedOskLayoutString_ += "\n";

        bool firstCell = true;
        for (const auto& cell : row.cells) {
            if (!firstCell) this->cachedOskLayoutString_ += "|";
            firstCell = false;

            if (cell.isSpacer) {
                char buf[32];
                snprintf(buf, sizeof(buf), "SPACER:%.2f", cell.spacerWidth);
                this->cachedOskLayoutString_ += buf;
            } else {
                this->cachedOskLayoutString_ += cell.widget.name;
                this->cachedOskLayoutString_ += ":";
                bool firstProperty = true;
                auto appendProperty = [&](const char* key, const string& value) {
                    if (value.empty()) return;
                    if (!firstProperty) this->cachedOskLayoutString_ += ",";
                    firstProperty = false;
                    this->cachedOskLayoutString_ += key;
                    this->cachedOskLayoutString_ += "=";
                    this->cachedOskLayoutString_ += EscapeOskLayoutValue(value);
                };
                appendProperty("Shape", cell.widget.shape);
                char buf[64];
                snprintf(buf, sizeof(buf), "%.2f", cell.widget.width);
                appendProperty("Width", buf);
                snprintf(buf, sizeof(buf), "%.2f", cell.widget.height);
                appendProperty("Height", buf);
                snprintf(buf, sizeof(buf), "%.2f", cell.widget.top);
                appendProperty("Top", buf);
                appendProperty("Group", cell.widget.group);
                appendProperty("Label", cell.widget.label);
                appendProperty("Color", cell.widget.color);
                appendProperty("Class", cell.widget.widgetClass);
                appendProperty("Role", cell.widget.role);
                appendProperty("Input", cell.widget.input);
                appendProperty("Feedback", cell.widget.feedback);
                appendProperty("PressTarget", cell.widget.pressTarget);
                appendProperty("ScrollTarget", cell.widget.scrollTarget);
                appendProperty("ValueTarget", cell.widget.valueTarget);
                appendProperty("TouchTarget", cell.widget.touchTarget);
                appendProperty("RotaryStyle", cell.widget.rotaryStyle);
            }
        }
    }
}

void ControlSurface::ApplyFormat2OSKLayout(const string& surfaceFilePath, const Format2OskLayout& layout) {
    this->surfaceFilePath_ = surfaceFilePath;
    this->oskLayout_.clear();
    this->cachedOskLayoutString_.clear();
    vector<OskWidgetInfo> hiddenWidgets;

    for (const Format2OskRow& sourceRow : layout.rows) {
        OskRow row;
        for (const Format2OskCell& sourceCell : sourceRow.cells) {
            OskCell cell;
            if (sourceCell.kind == Format2OskCellKind::Spacer) {
                cell.isSpacer = true;
                cell.spacerWidth = (float) sourceCell.width;
                row.cells.push_back(std::move(cell));
                continue;
            }

            OskWidgetInfo info;
            info.name = sourceCell.widget;
            if (!this->GetWidgetByName(info.name)) {
                LogToConsole("[ERROR] Format 2 OSK layout references unknown widget '%s' in %s, line %d\n", info.name.c_str(), surfaceFilePath.c_str(), sourceCell.location.line);
                continue;
            }
            info.shape = sourceCell.shape;
            info.width = (float) sourceCell.width;
            info.height = (float) sourceCell.height;
            info.top = (float) sourceCell.top;
            info.group = sourceCell.group;
            info.label = sourceCell.label;
            if (sourceCell.color) {
                char color[8];
                snprintf(color, sizeof(color), "%06X", (unsigned int) (*sourceCell.color & 0xFFFFFF));
                info.color = color;
            }
            info.role = sourceCell.role;
            info.pressTarget = sourceCell.pressTarget;
            info.scrollTarget = sourceCell.scrollTarget;
            info.valueTarget = sourceCell.valueTarget;
            info.touchTarget = sourceCell.touchTarget;
            info.rotaryStyle = sourceCell.rotaryStyle;
            this->ApplyOskWidgetMetadata(info);
            if (info.hidden) hiddenWidgets.push_back(info);
            else {
                cell.widget = std::move(info);
                row.cells.push_back(std::move(cell));
            }
        }
        if (!row.cells.empty()) this->oskLayout_.push_back(std::move(row));
    }

    this->ApplyGroupedOskTargets(hiddenWidgets);
    this->BuildCachedLayoutString();
}

void ControlSurface::ApplyFormat2ColorCalibration(const Format2ColorCalibration& calibration) {
    this->colorCalibration_ = ColorCalibrationConfig{};
    this->colorCalibration_.enabled = true;
    this->colorCalibration_.inputMax = calibration.inputMax;
    this->colorCalibration_.outputMax = calibration.outputMax.value_or(0);
    this->colorCalibration_.neutralTolerancePercent = calibration.neutralTolerancePercent;
    this->colorCalibration_.redScale = (float) calibration.redScale;
    this->colorCalibration_.greenScale = (float) calibration.greenScale;
    this->colorCalibration_.blueScale = (float) calibration.blueScale;
    this->colorCalibration_.neutralRedScale = (float) calibration.neutralRedScale;
    this->colorCalibration_.neutralGreenScale = (float) calibration.neutralGreenScale;
    this->colorCalibration_.neutralBlueScale = (float) calibration.neutralBlueScale;
    this->colorCalibration_.neutralCurve = (float) calibration.neutralCurve;
}

void ControlSurface::PublishOSKLayout() {
    if (!isOskEnabled_) return;

    string key = string("Layout_") + name_;
    ::SetExtState(ProductIdentity::ExtStateOsk, key.c_str(), cachedOskLayoutString_.c_str(), false);
}

void ControlSurface::PublishOSKState() {
    if (!isOskEnabled_) return;
    // Throttle to ~10Hz (every 3rd call of 30Hz)
    if (++oskRunCounter_ < 3) return;
    oskRunCounter_ = 0;
    this->PublishOSKLabels();
    string state;
    for (const auto& row : oskLayout_) {
        for (const auto& cell : row.cells) {
            if (cell.isSpacer) continue;
            Widget* widget = GetWidgetByName(cell.widget.name);
            if (!widget) continue;
            double value = widget->GetLastFeedbackValue();
            bool isContinuousWidget = IsOskContinuousWidgetRole(cell.widget.role);
            bool hasValue = !isContinuousWidget;
            const char* continuousKind = "";
            rgba_color color = (!isContinuousWidget || widget->HasOskColorFeedback()) ? widget->GetLastFeedbackColor() : rgba_color();
            if (this->zoneManager_ && isContinuousWidget) {
                const auto& contexts = this->zoneManager_->GetCurrentActionContextsForWidget(widget);
                for (const auto& context : contexts) {
                    Action* action = context->GetAction();
                    if (!IsOskContinuousValueAction(action)) continue;
                    value = ClampOskNormalizedValue(action->GetCurrentNormalizedValue(context.get()));
                    hasValue = true;
                    continuousKind = GetOskContinuousKind(action);
                    if (MediaTrack* track = context->GetTrack()) {
                        rgba_color trackColor = DAW::GetTrackColor(track);
                        if (IsOskColorMeaningful(trackColor)) color = trackColor;
                    }
                    break;
                }
            }
            if (!hasValue) value = 0.0;
            if (!state.empty()) state += ";";
            char buf[192];
            snprintf(buf, sizeof(buf), "%s=V:%.2f,C:#%02X%02X%02X,A:%d,K:%s", cell.widget.name.c_str(), value, (unsigned char) color.r, (unsigned char) color.g, (unsigned char) color.b, hasValue ? 1 : 0, continuousKind);
            state += buf;
        }
    }
    if (state != cachedOskStateString_) {
        cachedOskStateString_ = state;
        string key = string("State_") + name_;
        ::SetExtState(ProductIdentity::ExtStateOsk, key.c_str(), state.c_str(), false);
    }
}

void ControlSurface::PublishOSKLabels() {
    if (!isOskEnabled_) return;
    if (!zoneManager_) return;

    string labels;

    // Collect labels from active zones.
    for (const auto& row : oskLayout_) {
        for (const auto& cell : row.cells) {
            if (cell.isSpacer) continue;

            Widget* widget = GetWidgetByName(cell.widget.name);
            if (!widget) continue;

            // Get the current action contexts for this widget (respects modifiers)
            const auto& contexts = zoneManager_->GetCurrentActionContextsForWidget(widget);

            string keyLabel;
            for (const auto& ctx : contexts) {
                keyLabel = GetConfiguredOskKeyLabel(ctx.get());
                if (!keyLabel.empty()) break;
            }

            if (keyLabel.empty() && IsOskContinuousWidgetRole(cell.widget.role)) {
                for (const auto& ctx : contexts) {
                    Action* action = ctx->GetAction();
                    if (!action || (!action->IsVolumeRelated() && !action->IsPanRelated())) continue;
                    if (MediaTrack* track = ctx->GetTrack()) {
                        keyLabel = DAW::GetTrackName(track);
                        break;
                    }
                }
            }

            for (const auto& ctx : contexts) {
                if (!keyLabel.empty()) break;
                keyLabel = GetConfiguredOskLabel(ctx.get());
            }

            // Fallback: use action title
            if (keyLabel.empty()) {
                for (const auto& ctx : contexts) {
                    const char* title = ctx->GetActionTitle();
                    if (title && title[0] != '\0') {
                        keyLabel = title;
                        break;
                    }
                }
            }

            // Final fallback: widget name or label override
            if (keyLabel.empty()) {
                if (!cell.widget.label.empty()) keyLabel = cell.widget.label;
                else keyLabel = cell.widget.name;
            }

            if (!labels.empty()) labels += ";";
            labels += cell.widget.name;
            labels += "=";
            labels += keyLabel;
        }
    }

    if (labels != cachedOskLabelsString_) {
        cachedOskLabelsString_ = labels;
        string key = string("Labels_") + name_;
        ::SetExtState(ProductIdentity::ExtStateOsk, key.c_str(), labels.c_str(), false);
    }
    PublishOSKLabelMap();
}

// ---------------------------------------------------------------------------
// OSK input simulation
// ---------------------------------------------------------------------------

void ControlSurface::InjectOSKPressDown(const string& widgetName) {
    Widget* widget = GetWidgetByName(widgetName);
    if (!widget) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] InjectOSKPressDown: widget '%s' not found on '%s'\n", widgetName.c_str(), name_.c_str());
        return;
    }
    widget->LogInput(1.0);
    zoneManager_->DoAction(widget, 1.0);
}

void ControlSurface::InjectOSKPressUp(const string& widgetName) {
    Widget* widget = GetWidgetByName(widgetName);
    if (!widget) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] InjectOSKPressUp: widget '%s' not found on '%s'\n", widgetName.c_str(), name_.c_str());
        return;
    }
    widget->LogInput(0.0);
    zoneManager_->DoAction(widget, 0.0);
}

void ControlSurface::InjectOSKScroll(const string& widgetName, int accelerationIndex, double delta, int eventCount) {
    Widget* widget = this->GetWidgetByName(widgetName);
    if (!widget) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] InjectOSKScroll: widget '%s' not found on '%s'\n", widgetName.c_str(), this->name_.c_str());
        return;
    }

    double step = widget->GetStepSize();
    if (step <= 0.0) step = 0.01;
    const double signedDelta = delta >= 0.0 ? step : -step;
    const int boundedEventCount = (std::max)(1, (std::min)(eventCount, 8));
    for (int eventIdx = 0; eventIdx < boundedEventCount; ++eventIdx)
        this->zoneManager_->DoRelativeAction(widget, accelerationIndex, signedDelta);
}

void ControlSurface::InjectOSKValue(const string& widgetName, double value) {
    Widget* widget = this->GetWidgetByName(widgetName);
    if (!widget) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] InjectOSKValue: widget '%s' not found on '%s'\n", widgetName.c_str(), this->name_.c_str());
        return;
    }
    if (!this->zoneManager_) return;
    const double faderValue = value < 0.0 || value > 1.0 ? value : ClampOskNormalizedValue(value);
    bool dispatchedToFaderContext = false;
    const auto& contexts = this->zoneManager_->GetCurrentActionContextsForWidget(widget);
    for (const auto& context : contexts) {
        Action* action = context->GetAction();
        if (!IsOskContinuousValueAction(action)) continue;
        context->DoRangeBoundAction(MapOskContinuousValueToAction(action, faderValue));
        dispatchedToFaderContext = true;
    }
    if (dispatchedToFaderContext) {
        widget->LogInput(faderValue);
        return;
    }
    this->zoneManager_->DoAction(widget, ClampOskNormalizedValue(value));
}

void ControlSurface::InjectOSKTouch(const string& widgetName, double value) {
    Widget* widget = this->GetWidgetByName(widgetName);
    if (!widget) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] InjectOSKTouch: widget '%s' not found on '%s'\n", widgetName.c_str(), this->name_.c_str());
        return;
    }
    this->zoneManager_->DoTouch(widget, value != 0.0 ? 1.0 : 0.0);
}

void ControlSurface::HandleOSKConfigQuery(const string& widgetName) {
    if (!this->zoneManager_) {
        PublishConfigStatus("ERR", "Query", this->name_, widgetName, "", "ZoneManager unavailable");
        return;
    }

    Widget* widget = this->GetWidgetByName(widgetName);
    if (!widget) {
        PublishConfigStatus("ERR", "Query", this->name_, widgetName, "", "Widget not found");
        return;
    }

    map<int, const vector<unique_ptr<ActionContext>>*> modContexts;
    this->zoneManager_->CollectAllModifierContextsForWidget(widget, modContexts);

    string zoneName;
    string zonePath;
    this->zoneManager_->GetActiveZoneInfoForWidget(widget, zoneName, zonePath);
    if (!zoneName.empty()) {
        this->oskConfigZoneNamesByWidget_[widgetName] = zoneName;
        this->oskConfigZonePathsByWidget_[widgetName] = zonePath;
    } else {
        const auto zoneNameEntry = this->oskConfigZoneNamesByWidget_.find(widgetName);
        const auto zonePathEntry = this->oskConfigZonePathsByWidget_.find(widgetName);
        if (zoneNameEntry != this->oskConfigZoneNamesByWidget_.end())
            zoneName = zoneNameEntry->second;
        if (zonePathEntry != this->oskConfigZonePathsByWidget_.end())
            zonePath = zonePathEntry->second;
    }

    string result;
    for (const auto& [mod, ctxs] : modContexts) {
        for (const auto& ctx : *ctxs) {
            if (!result.empty()) result += ";";
            result += std::to_string(mod);
            result += ":";
            result += SerializeContextAction(ctx.get());
            if (ctx->GetHoldDelay() > 0) result += " __OSK_HOLD";
            if (ctx->IsDoublePress()) result += " __OSK_DOUBLE_PRESS";
            if (ctx->GetIsValueInverted()) result += " __OSK_INVERT";
            if (ctx->GetIsFeedbackInverted()) result += " __OSK_INVERT_FB";
            if (ctx->GetRangeMinimum() == -2.0 && ctx->GetRangeMaximum() == 1.0) result += " __OSK_DECREASE";
            else if (ctx->GetRangeMinimum() == 0.0 && ctx->GetRangeMaximum() == 2.0) result += " __OSK_INCREASE";
        }
    }

    const string keyResult = string("ConfigResult_") + this->name_ + "_" + widgetName;
    ::SetExtState(ProductIdentity::ExtStateOsk, keyResult.c_str(), result.c_str(), false);

    const string keyZoneName = string("ConfigZoneName_") + this->name_ + "_" + widgetName;
    ::SetExtState(ProductIdentity::ExtStateOsk, keyZoneName.c_str(), zoneName.c_str(), false);

    const string keyZonePath = string("ConfigZonePath_") + this->name_ + "_" + widgetName;
    ::SetExtState(ProductIdentity::ExtStateOsk, keyZonePath.c_str(), zonePath.c_str(), false);

    PublishConfigStatus("OK", "Query", this->name_, widgetName, zoneName, "Config query completed");
}

void ControlSurface::HandleOSKConfigApplyLive(const string& widgetName, const string& bindingData) {
    if (!this->zoneManager_) {
        PublishConfigStatus("ERR", "ApplyLive", this->name_, widgetName, "", "ZoneManager unavailable");
        return;
    }

    Widget* widget = this->GetWidgetByName(widgetName);
    if (!widget) {
        PublishConfigStatus("ERR", "ApplyLive", this->name_, widgetName, "", "Widget not found");
        return;
    }

    Zone* activeZone = this->zoneManager_->GetActiveZoneForWidget(widget);
    if (!activeZone) {
        PublishConfigStatus("ERR", "ApplyLive", this->name_, widgetName, "", "No active zone for widget");
        return;
    }

    vector<OskConfigBinding> parsedBindings;
    string errorMessage;
    if (!ParseConfigBindings(this->csi_, bindingData, parsedBindings, errorMessage)) {
        PublishConfigStatus("ERR", "ApplyLive", this->name_, widgetName, activeZone->GetName(), errorMessage);
        return;
    }
    const vector<Format2Diagnostic> gestureDiagnostics = ValidateOskConfigGestures(widget, parsedBindings, this->settings_, this->doublePressTime_);
    const string gestureError = FormatOskConfigDiagnostics(gestureDiagnostics, Format2DiagnosticSeverity::Error);
    if (!gestureError.empty()) {
        PublishConfigStatus("ERR", "ApplyLive", this->name_, widgetName, activeZone->GetName(), gestureError);
        return;
    }
    const string gestureWarning = FormatOskConfigDiagnostics(gestureDiagnostics, Format2DiagnosticSeverity::Warning);

    const vector<OskConfigBinding> previousBindings = CaptureConfigBindings(activeZone, widget);
    try {
        activeZone->AddWidget(widget);
        ApplyConfigBindings(activeZone, widget, parsedBindings);
    } catch (const std::exception& exception) {
        try {
            ApplyConfigBindings(activeZone, widget, previousBindings);
        } catch (...) {
            this->zoneManager_->Initialize();
        }
        PublishConfigStatus("ERR", "ApplyLive", this->name_, widgetName, activeZone->GetName(), string("Apply failed: ") + exception.what());
        return;
    }

    activeZone->UpdateCurrentActionContextModifiers();
    this->PublishOSKLabels();
    this->PublishOSKState();
    this->PublishOSKLabelMap();
    PublishConfigStatus(gestureWarning.empty() ? "OK" : "WARN", "ApplyLive", this->name_, widgetName, activeZone->GetName(), gestureWarning.empty() ? "Apply live completed" : gestureWarning);
}

void ControlSurface::HandleOSKConfigSave(const string& widgetName) {
    if (!this->zoneManager_) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, "", "ZoneManager unavailable");
        return;
    }

    Widget* widget = this->GetWidgetByName(widgetName);
    if (!widget) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, "", "Widget not found");
        return;
    }

    Zone* activeZone = this->zoneManager_->GetActiveZoneForWidget(widget);
    string targetZoneName;
    string zonePath;
    if (activeZone) {
        targetZoneName = activeZone->GetName();
        zonePath = activeZone->GetSourceFilePath();
        this->oskConfigZoneNamesByWidget_[widgetName] = targetZoneName;
        this->oskConfigZonePathsByWidget_[widgetName] = zonePath;
    } else {
        const auto zoneNameEntry = this->oskConfigZoneNamesByWidget_.find(widgetName);
        const auto zonePathEntry = this->oskConfigZonePathsByWidget_.find(widgetName);
        if (zoneNameEntry != this->oskConfigZoneNamesByWidget_.end())
            targetZoneName = zoneNameEntry->second;
        if (zonePathEntry != this->oskConfigZonePathsByWidget_.end())
            zonePath = zonePathEntry->second;
    }
    if (targetZoneName.empty()) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, "", "No edit target zone for widget");
        return;
    }
    if (zonePath.empty()) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, targetZoneName, "Zone file path unavailable");
        return;
    }

    bool activateUserZoneProfile = false;
    string editableZonePath;
    string preparationError;
    if (!this->zoneManager_->PrepareZonePathForWrite(zonePath, editableZonePath, activateUserZoneProfile, preparationError)) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, targetZoneName, preparationError);
        return;
    }
    zonePath = editableZonePath;

    ifstream inputFile(zonePath);
    if (!inputFile.is_open()) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, targetZoneName, "Unable to open zone file for read");
        return;
    }

    vector<string> originalLines;
    for (string line; getline(inputFile, line);)
        originalLines.push_back(line);
    inputFile.close();

    vector<string> replacementLines;
    if (activeZone)
        replacementLines = BuildSerializedWidgetLines(activeZone, widget, widgetName);
    ApplyInlineComments(
        replacementLines,
        CollectWidgetInlineComments(originalLines, targetZoneName, widgetName)
    );

    vector<string> updatedLines;
    bool inTargetZone = false;
    bool foundTargetZone = false;
    bool insertedReplacementLines = false;

    for (const auto& rawLine : originalLines) {
        string trimmed = rawLine;
        TrimLine(trimmed);

        if (!inTargetZone && trimmed.rfind("Zone ", 0) == 0) {
            vector<string> zoneTokens;
            GetTokens(zoneTokens, trimmed);
            if (zoneTokens.size() >= 2 && IsSameString(zoneTokens[1], targetZoneName)) {
                inTargetZone = true;
                foundTargetZone = true;
            }
        }

        if (inTargetZone && IsSameString(trimmed, "ZoneEnd")) {
            if (!insertedReplacementLines) {
                for (const auto& replacementLine : replacementLines)
                    updatedLines.push_back(replacementLine);
                insertedReplacementLines = true;
            }
            updatedLines.push_back(rawLine);
            inTargetZone = false;
            continue;
        }

        bool skipCurrentLine = false;
        if (inTargetZone && !trimmed.empty() && !IsCommentedOrEmpty(trimmed)) {
            vector<string> lineTokens;
            GetTokens(lineTokens, trimmed);
            if (!lineTokens.empty()) {
                const string lineWidgetName = ExtractWidgetNameFromZoneToken(lineTokens[0]);
                if (IsSameString(lineWidgetName, widgetName)) {
                    if (!insertedReplacementLines) {
                        for (const auto& replacementLine : replacementLines)
                            updatedLines.push_back(replacementLine);
                        insertedReplacementLines = true;
                    }
                    skipCurrentLine = true;
                }
            }
        }

        if (!skipCurrentLine)
            updatedLines.push_back(rawLine);
    }

    if (!foundTargetZone) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, targetZoneName, "Target zone section not found in file");
        return;
    }

    if (updatedLines == originalLines) {
        if (activateUserZoneProfile) {
            this->oskConfigZoneNamesByWidget_.clear();
            this->oskConfigZonePathsByWidget_.clear();
            this->zoneManager_->ReloadFromDisk();
            PublishConfigStatus("OK", "Save", this->name_, widgetName, targetZoneName, "Editable user zone profile is active; no zone file changes required");
        } else {
            PublishConfigStatus("OK", "Save", this->name_, widgetName, targetZoneName, "No file changes required");
        }
        return;
    }

    string backupPath;
    string errorMessage;
    if (!CommitZoneFile(zonePath, updatedLines, backupPath, errorMessage)) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, targetZoneName, errorMessage);
        return;
    }

    if (activateUserZoneProfile) {
        this->oskConfigZoneNamesByWidget_.clear();
        this->oskConfigZonePathsByWidget_.clear();
        this->zoneManager_->ReloadFromDisk();
    }

    this->PublishOSKLabels();
    this->PublishOSKState();
    this->PublishOSKLabelMap();
    PublishConfigStatus("OK", "Save", this->name_, widgetName, targetZoneName, "Saved to zone file; backup: " + backupPath);
}

void ControlSurface::HandleOSKConfigRevert(const string& widgetName) {
    if (!this->zoneManager_) {
        PublishConfigStatus("ERR", "Revert", this->name_, widgetName, "", "ZoneManager unavailable");
        return;
    }

    this->zoneManager_->ReloadFromDisk();
    this->PublishOSKLabels();
    this->PublishOSKState();
    this->PublishOSKLabelMap();
    PublishConfigStatus("OK", "Revert", this->name_, widgetName, "", "Reverted from disk");
}

void ControlSurface::HandleOSKZoneCreate(const string& scaffoldType, const string& zoneName, const string& alias, const string& navigator) {
    const ZoneFileCreateResult result = ZoneFileCreator::Create(this->zoneManager_.get(), { scaffoldType, zoneName, alias, navigator });
    if (!result.success) {
        PublishZoneCreateStatus(this->name_, "ERR", result.path, result.message);
        return;
    }
    try {
        this->oskConfigZoneNamesByWidget_.clear();
        this->oskConfigZonePathsByWidget_.clear();
        this->zoneManager_->ReloadFromDisk();
        this->PublishOSKLabels();
        this->PublishOSKState();
        this->PublishOSKLabelMap();
        PublishZoneCreateStatus(this->name_, "OK", result.path, result.message);
    } catch (const std::exception& error) {
        PublishZoneCreateStatus(this->name_, "ERR", result.path, string("Zone was created, but reload failed: ") + error.what());
    }
}

void ControlSurface::PublishOSKLabelMap() {
    if (!isOskEnabled_) return;
    if (!zoneManager_) return;

    // Extract the best display label from an ActionContext, falling back to cell defaults.
    auto getLabel = [](ActionContext* ctx, const OskWidgetInfo& wi) -> string {
        const string configuredLabel = GetConfiguredOskLabel(ctx);
        if (!configuredLabel.empty()) return configuredLabel;
        if (const char* title = ctx->GetActionTitle())
            if (title[0] != '\0') return title;
        return wi.label.empty() ? wi.name : wi.label;
    };

    string labelMap;
    auto appendLabelMapEntry = [&](const string& mapWidgetName, Widget* widget, const OskWidgetInfo& widgetInfo) {
        if (!widget) return;

        map<int, const vector<unique_ptr<ActionContext>>*> modContexts;
        zoneManager_->CollectAllModifierContextsForWidget(widget, modContexts);
        if (modContexts.empty()) return;

        string entries;
        for (const auto& [mod, ctxs] : modContexts) {
            for (const auto& ctx : *ctxs) {
                const ActionType actionType = ctx->GetAction()->GetType();
                if (actionType == ActionType::NoAction) continue;

                const string modName = BuildOskTooltipModifierName(mod, ctx.get());
                string label = getLabel(ctx.get(), widgetInfo);
                if (!entries.empty()) entries += "|";
                entries += modName + ":" + label;
            }
        }

        if (entries.empty()) return;
        if (!labelMap.empty()) labelMap += ";";
        labelMap += mapWidgetName + "=" + entries;
    };

    for (const auto& row : oskLayout_) {
        for (const auto& cell : row.cells) {
            if (cell.isSpacer) continue;

            Widget* w = GetWidgetByName(cell.widget.name);
            if (!w) continue;
            appendLabelMapEntry(cell.widget.name, w, cell.widget);
            if (!cell.widget.pressTarget.empty() && !IsSameString(cell.widget.pressTarget.c_str(), cell.widget.name.c_str())) {
                OskWidgetInfo targetInfo = cell.widget;
                targetInfo.name = cell.widget.pressTarget;
                targetInfo.label.clear();
                appendLabelMapEntry(cell.widget.pressTarget, GetWidgetByName(cell.widget.pressTarget), targetInfo);
            }
        }
    }

    if (labelMap != cachedOskLabelMapString_) {
        cachedOskLabelMapString_ = labelMap;
        string key = string("LabelMap_") + name_;
        ::SetExtState(ProductIdentity::ExtStateOsk, key.c_str(), labelMap.c_str(), false);
    }
}
