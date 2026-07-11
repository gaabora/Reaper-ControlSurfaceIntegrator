// osk.cpp — ControlSurface OSK (On-Screen Keyboard) member implementations.

#include "integrator.h"

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

        // Remove quotes if present
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);

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
    cachedOskLayoutString_.clear();

    for (const auto& row : oskLayout_) {
        if (!cachedOskLayoutString_.empty()) cachedOskLayoutString_ += "\n";

        bool firstCell = true;
        for (const auto& cell : row.cells) {
            if (!firstCell) cachedOskLayoutString_ += "|";
            firstCell = false;

            if (cell.isSpacer) {
                char buf[32];
                snprintf(buf, sizeof(buf), "SPACER:%.2f", cell.spacerWidth);
                cachedOskLayoutString_ += buf;
            } else {
                cachedOskLayoutString_ += cell.widget.name;
                cachedOskLayoutString_ += ":Shape=";
                cachedOskLayoutString_ += cell.widget.shape;

                char buf[64];
                snprintf(buf, sizeof(buf), ",Width=%.2f,Height=%.2f,Top=%.2f", cell.widget.width, cell.widget.height, cell.widget.top);
                cachedOskLayoutString_ += buf;

                if (!cell.widget.group.empty()) {
                    cachedOskLayoutString_ += ",Group=";
                    cachedOskLayoutString_ += cell.widget.group;
                }
                if (!cell.widget.label.empty()) {
                    cachedOskLayoutString_ += ",Label=";
                    cachedOskLayoutString_ += cell.widget.label;
                }
                if (!cell.widget.color.empty()) {
                    cachedOskLayoutString_ += ",Color=";
                    cachedOskLayoutString_ += cell.widget.color;
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
    ::SetExtState("CSI_OSK", key.c_str(), cachedOskLayoutString_.c_str(), false);
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
        ::SetExtState("CSI_OSK", key.c_str(), state.c_str(), false);
    }
}

void ControlSurface::PublishOSKLabels() {
    if (!isOskEnabled_) return;
    if (!zoneManager_) return;

    string labels;

    // Collect labels from active zones
    // Walk all widgets and find their current ActionContexts, look for KeyLabel property
    for (const auto& row : oskLayout_) {
        for (const auto& cell : row.cells) {
            if (cell.isSpacer) continue;

            Widget* widget = GetWidgetByName(cell.widget.name);
            if (!widget) continue;

            // Get the current action contexts for this widget (respects modifiers)
            const auto& contexts = zoneManager_->GetCurrentActionContextsForWidget(widget);

            string keyLabel;
            for (const auto& ctx : contexts) {
                const char* kl = ctx->GetWidgetProperties().get_prop(PropertyType_KeyLabel);
                if (kl && kl[0] != '\0') {
                    keyLabel = kl;
                    break;
                }
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
        ::SetExtState("CSI_OSK", key.c_str(), labels.c_str(), false);
    }
    PublishOSKLabelMap();
}

// ---------------------------------------------------------------------------
// OSK input simulation
// ---------------------------------------------------------------------------

void ControlSurface::InjectOSKPress(const string& widgetName) {
    Widget* widget = GetWidgetByName(widgetName);
    if (!widget) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] InjectOSKPress: widget '%s' not found on '%s'\n", widgetName.c_str(), name_.c_str());
        return;
    }
    widget->LogInput(1.0);
    zoneManager_->DoAction(widget, 1.0);
    // Simulate release for two-state buttons so they don't stay "held".
    if (widget->GetIsTwoState())
        zoneManager_->DoAction(widget, 0.0);
}

void ControlSurface::InjectOSKScroll(const string& widgetName, bool isIncrease) {
    Widget* widget = GetWidgetByName(widgetName);
    if (!widget) {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] InjectOSKScroll: widget '%s' not found on '%s'\n", widgetName.c_str(), name_.c_str());
        return;
    }
    double step = widget->GetStepSize();
    if (step <= 0.0) step = 0.01;   // sensible default: 1 % of normalized range per tick
    widget->LogInput(isIncrease ? step : -step);
    zoneManager_->DoRelativeAction(widget, isIncrease ? step : -step);
}

void ControlSurface::PublishOSKLabelMap() {
    if (!isOskEnabled_) return;
    if (!zoneManager_) return;

    // Helper: extract the best display label from an ActionContext, falling back to cell defaults.
    auto getLabel = [](ActionContext* ctx, const OskWidgetInfo& wi) -> string {
        if (const char* kl = ctx->GetWidgetProperties().get_prop(PropertyType_KeyLabel))
            if (kl[0] != '\0') return kl;
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
        ::SetExtState("CSI_OSK", key.c_str(), labelMap.c_str(), false);
    }
}
