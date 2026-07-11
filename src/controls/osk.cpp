// osk.cpp — ControlSurface OSK (On-Screen Keyboard) member implementations.

#include "integrator.h"

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
    ::SetExtState("ReaCtrlSurf_OSK", scopedKey.c_str(), status.c_str(), false);
}

static string GetConfiguredOskLabel(ActionContext* context) {
    if (!context) return "";
    if (const char* keyLabel = context->GetWidgetProperties().get_prop(PropertyType_KeyLabel))
        if (keyLabel[0] != '\0') return keyLabel;
    if (const char* osdLabel = context->GetWidgetProperties().get_prop(PropertyType_OSD))
        if (osdLabel[0] != '\0' && !IsSameString(osdLabel, "No") && !IsSameString(osdLabel, "?")) return osdLabel;
    return "";
}

static double ClampOskNormalizedValue(double value) {
    return (std::max)(0.0, (std::min)(value, 1.0));
}

static bool IsOskFaderValueAction(Action* action) {
    if (!action || action->IsDisplayRelated()) return false;
    return action->IsVolumeRelated() || action->IsPanRelated() || action->IsFxRelated() || action->IsTrackSendRelated() || action->IsTrackReceiveRelated() || action->IsMeterRelated();
}

static double MapOskFaderValueToAction(Action* action, double value) {
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
            continue;
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
            continue;
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

static string UnescapeOskLayoutValue(const string& value) {
    if (value.size() < 2 || value.front() != '"' || value.back() != '"') return value;

    string result;
    result.reserve(value.size() - 2);
    for (size_t idx = 1; idx + 1 < value.size(); ++idx) {
        const char ch = value[idx];
        if (ch == '\\' && idx + 2 < value.size()) {
            const char nextCh = value[++idx];
            if (nextCh == 'n') result += '\n';
            else if (nextCh == 'r') result += '\r';
            else result += nextCh;
        } else {
            result += ch;
        }
    }
    return result;
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

void ControlSurface::ParseOskProperties(const string& propsPart, OskWidgetInfo& info) {
    if (propsPart.empty()) return;
    vector<string> tokens;
    GetTokens(tokens, propsPart);

    for (const auto& token : tokens) {
        if (token == "OSKHidden") {
            info.hidden = true;
            continue;
        }
        auto eqPos = token.find('=');
        if (eqPos == string::npos) continue;
        string key = token.substr(0, eqPos);
        string val = token.substr(eqPos + 1);
        // TrimLine(key);
        // TrimLine(val);
        // transform(key.begin(), key.end(), key.begin(), ::tolower);
        // if (key != "label") transform(val.begin(), val.end(), val.begin(), ::tolower);
        //TODO: review the formats, maybe make all parsers everywhere case insensitive and trim tokens

        val = UnescapeOskLayoutValue(val);

        if (key == "Shape") info.shape = val;
        else if (key == "Width") info.width = (float) atof(val.c_str());
        else if (key == "Height") info.height = (float) atof(val.c_str());
        else if (key == "Top") info.top = (float) atof(val.c_str());
        else if (key == "Group") info.group = val;
        else if (key == "Label") info.label = val;
        else if (key == "Color") info.color = val;
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
                this->cachedOskLayoutString_ += ":Shape=";
                this->cachedOskLayoutString_ += EscapeOskLayoutValue(cell.widget.shape);

                char buf[64];
                snprintf(buf, sizeof(buf), ",Width=%.2f,Height=%.2f,Top=%.2f", cell.widget.width, cell.widget.height, cell.widget.top);
                this->cachedOskLayoutString_ += buf;

                if (!cell.widget.group.empty()) {
                    this->cachedOskLayoutString_ += ",Group=";
                    this->cachedOskLayoutString_ += EscapeOskLayoutValue(cell.widget.group);
                }
                if (!cell.widget.label.empty()) {
                    this->cachedOskLayoutString_ += ",Label=";
                    this->cachedOskLayoutString_ += EscapeOskLayoutValue(cell.widget.label);
                }
                if (!cell.widget.color.empty()) {
                    this->cachedOskLayoutString_ += ",Color=";
                    this->cachedOskLayoutString_ += EscapeOskLayoutValue(cell.widget.color);
                }
            }
        }
    }
}

void ControlSurface::ParseOSKLayout(const string& surfaceFilePath) {
    surfaceFilePath_ = surfaceFilePath;
    oskLayout_.clear();
    try {
        ifstream file(surfaceFilePath);
        if (!file.is_open()) return;
        OskRow currentRow;
        bool hasRow = false;
        for (string line; getline(file, line);) {
            TrimLine(line);
            // transform(line.begin(), line.end(), line.begin(), ::tolower);

            if (line == "# OSKRow") {
                if (hasRow && !currentRow.cells.empty())
                    oskLayout_.push_back(std::move(currentRow));
                currentRow = OskRow();
                hasRow = true;
                continue;
            }
            if (line.find("# OSKSpacer") == 0) {
                float width = 0.5f;
                auto pos = line.find("Width=");
                if (pos != string::npos) width = (float) atof(line.c_str() + pos + 6);
                if (hasRow) {
                    OskCell cell;
                    cell.isSpacer = true;
                    cell.spacerWidth = width;
                    currentRow.cells.push_back(cell);
                }
                continue;
            }
            if (line.find("Widget ") == 0) {
                auto hashPos = line.find('#');
                string widgetPart = (hashPos != string::npos) ? line.substr(0, hashPos) : line;
                string propsPart = (hashPos != string::npos) ? line.substr(hashPos + 1) : "";
                vector<string> tokens;
                GetTokens(tokens, widgetPart);
                if (tokens.size() < 2) continue;
                OskWidgetInfo info;
                info.name = tokens[1];
                ParseOskProperties(propsPart, info);

                if (info.hidden) continue; // Skip hidden widgets
                if (!hasRow) {
                    currentRow = OskRow();
                    hasRow = true;
                }
                OskCell cell;
                cell.isSpacer = false;
                cell.widget = info;
                currentRow.cells.push_back(cell);
            }
        }

        if (hasRow && !currentRow.cells.empty())
            oskLayout_.push_back(std::move(currentRow));
        BuildCachedLayoutString();
    } catch (const std::exception& e) {
        LogToConsole("[ERROR] ParseOSKLayout failed for %s: %s\n", surfaceFilePath.c_str(), e.what());
    }
}

void ControlSurface::PublishOSKLayout() {
    if (!isOskEnabled_) return;

    string key = string("Layout_") + name_;
    ::SetExtState("ReaCtrlSurf_OSK", key.c_str(), cachedOskLayoutString_.c_str(), false);
}

void ControlSurface::PublishOSKState() {
    if (!isOskEnabled_) return;
    // Throttle to ~10Hz (every 3rd call of 30Hz)
    if (++oskRunCounter_ < 3) return;
    oskRunCounter_ = 0;
    string state;
    for (const auto& row : oskLayout_) {
        for (const auto& cell : row.cells) {
            if (cell.isSpacer) continue;
            Widget* widget = GetWidgetByName(cell.widget.name);
            if (!widget) continue;
            double value = widget->GetLastFeedbackValue();
            if (this->zoneManager_ && IsSameString(cell.widget.shape.c_str(), "fader")) {
                const auto& contexts = this->zoneManager_->GetCurrentActionContextsForWidget(widget);
                for (const auto& context : contexts) {
                    Action* action = context->GetAction();
                    if (!IsOskFaderValueAction(action)) continue;
                    value = ClampOskNormalizedValue(action->GetCurrentNormalizedValue(context.get()));
                    break;
                }
            }
            rgba_color color = widget->GetLastFeedbackColor();
            if (!state.empty()) state += ";";
            char buf[128];
            snprintf(buf, sizeof(buf), "%s=V:%.2f,C:#%02X%02X%02X", cell.widget.name.c_str(), value, (unsigned char) color.r, (unsigned char) color.g, (unsigned char) color.b);
            state += buf;
        }
    }
    if (state != cachedOskStateString_) {
        cachedOskStateString_ = state;
        string key = string("State_") + name_;
        ::SetExtState("ReaCtrlSurf_OSK", key.c_str(), state.c_str(), false);
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
                keyLabel = GetConfiguredOskLabel(ctx.get());
                if (!keyLabel.empty()) break;
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
        ::SetExtState("ReaCtrlSurf_OSK", key.c_str(), labels.c_str(), false);
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
        if (!IsOskFaderValueAction(action)) continue;
        context->DoRangeBoundAction(MapOskFaderValueToAction(action, faderValue));
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
    ::SetExtState("ReaCtrlSurf_OSK", keyResult.c_str(), result.c_str(), false);

    const string keyZoneName = string("ConfigZoneName_") + this->name_ + "_" + widgetName;
    ::SetExtState("ReaCtrlSurf_OSK", keyZoneName.c_str(), zoneName.c_str(), false);

    const string keyZonePath = string("ConfigZonePath_") + this->name_ + "_" + widgetName;
    ::SetExtState("ReaCtrlSurf_OSK", keyZonePath.c_str(), zonePath.c_str(), false);

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
    PublishConfigStatus("OK", "ApplyLive", this->name_, widgetName, activeZone->GetName(), "Apply live completed");
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
        PublishConfigStatus("OK", "Save", this->name_, widgetName, targetZoneName, "No file changes required");
        return;
    }

    string backupPath;
    string errorMessage;
    if (!CommitZoneFile(zonePath, updatedLines, backupPath, errorMessage)) {
        PublishConfigStatus("ERR", "Save", this->name_, widgetName, targetZoneName, errorMessage);
        return;
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

    this->zoneManager_->Initialize();
    this->PublishOSKLabels();
    this->PublishOSKState();
    this->PublishOSKLabelMap();
    PublishConfigStatus("OK", "Revert", this->name_, widgetName, "", "Reverted from disk");
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

    for (const auto& row : oskLayout_) {
        for (const auto& cell : row.cells) {
            if (cell.isSpacer) continue;

            Widget* w = GetWidgetByName(cell.widget.name);
            if (!w) continue;

            // Collect all modifier -> context-vector pairs from the first active zone
            // that defines this widget (mirrors the priority order of GetCurrentActionContextsForWidget).
            map<int, const vector<unique_ptr<ActionContext>>*> modContexts;
            zoneManager_->CollectAllModifierContextsForWidget(w, modContexts);
            if (modContexts.empty()) continue;

            // Build per-widget portion: "widgetName=modName:label|modName:label|..."
            string entries;
            for (const auto& [mod, ctxs] : modContexts) {
                for (const auto& ctx : *ctxs) {
                    // Name this modifier slot.
                    // At modifier=0, contexts with HoldDelay>0 are "Hold" pseudo-bindings.
                    string modName;
                    if (mod == 0) {
                        modName = (ctx->GetHoldDelay() > 0) ? "Hold" : "NoMod";
                    } else {
                        char buf[64];
                        ModifierManager::GetModifierString(mod, buf, sizeof(buf));
                        // GetModifierString appends '+' after each name; strip trailing '+'.
                        string s(buf);
                        while (!s.empty() && s.back() == '+') s.pop_back();
                        modName = s;
                    }

                    string label = getLabel(ctx.get(), cell.widget);

                    // Skip pure feedback (Feedback=No) NoAction contexts — they add noise.
                    const ActionType at = ctx->GetAction()->GetType();
                    if (at == ActionType::NoAction) continue;

                    if (!entries.empty()) entries += "|";
                    entries += modName + ":" + label;
                }
            }

            if (!entries.empty()) {
                if (!labelMap.empty()) labelMap += ";";
                labelMap += cell.widget.name + "=" + entries;
            }
        }
    }

    if (labelMap != cachedOskLabelMapString_) {
        cachedOskLabelMapString_ = labelMap;
        string key = string("LabelMap_") + name_;
        ::SetExtState("ReaCtrlSurf_OSK", key.c_str(), labelMap.c_str(), false);
    }
}
