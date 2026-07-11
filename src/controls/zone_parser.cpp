// zone_parser.cpp — ZoneFileParser implementation.

#include "integrator.h"
#include "zone_parser.h"
#include "../actions/reaper_actions.h"
#include "../actions/manager_actions.h"

void ZoneFileParser::ParseFile(ZoneManager* zm, Zone* zone, const char* filePath, const char* widgetSuffix) {
    int lineNumber = 0;
    bool isInIncludedZonesSection = false;
    vector<string> includedZonesList;
    bool isInSubZonesSection = false;
    vector<string> subZonesList;
    map<string, Widget*> modifierActionWidgets;

    try {
        ifstream file(filePath);

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] @%s/{%s} # LoadZoneFile: %s\n", zm->surface_->GetName(), zone->GetName(), GetRelativePath(filePath).c_str());

        for (string line; getline(file, line);) {
            TrimLine(line);

            lineNumber++;

            if (IsCommentedOrEmpty(line)) continue;

            if (line == s_BeginAutoSection || line == s_EndAutoSection) continue;

            // Replace pipe-character placeholders (e.g. Track|) with the widget suffix
            ReplaceAllWith(line, ZoneManager::PIPE_CHARACTER, widgetSuffix);

            vector<string> tokens;
            GetTokens(tokens, line);

            if (tokens[0] == "Zone" || tokens[0] == "ZoneEnd") continue;

            else if (tokens[0] == "SubZones")
                isInSubZonesSection = true;
            else if (tokens[0] == "SubZonesEnd") {
                isInSubZonesSection = false;
                zone->InitSubZones(subZonesList, widgetSuffix);
            } else if (isInSubZonesSection)
                subZonesList.push_back(tokens[0]);

            else if (tokens[0] == "IncludedZones")
                isInIncludedZonesSection = true;
            else if (tokens[0] == "IncludedZonesEnd") {
                isInIncludedZonesSection = false;
                try {
                    zm->LoadZones(zone->GetIncludedZones(), includedZonesList);
                } catch (const std::exception& e) {
                    LogToConsole("[ERROR] %s in IncludedZones section in file %s\n", e.what(), GetRelativePath(zone->GetSourceFilePath()).c_str());
                }
            } else if (isInIncludedZonesSection)
                includedZonesList.push_back(tokens[0]);
            else if (tokens.size() < 2) {
                    LogToConsole("[ERROR] Not enough params at line %d in %s\n", lineNumber, GetRelativePath(filePath).c_str());
                    continue;
            } else if (tokens.size() > 1) {
                string widgetName;
                int modifier = 0;
                bool isValueInverted = false;
                bool isFeedbackInverted = false;
                bool hasHoldModifier = false;
                bool HasDoublePressPseudoModifier = false;
                bool isDecrease = false;
                bool isIncrease = false;

                zm->GetWidgetNameAndModifiers(tokens[0].c_str(), widgetName, modifier, isValueInverted, isFeedbackInverted, hasHoldModifier, HasDoublePressPseudoModifier, isDecrease, isIncrease);

                Widget* widget = zm->surface_->GetWidgetByName(widgetName);

                if (widget == NULL) {
                    const auto aliasIt = modifierActionWidgets.find(widgetName);
                    if (aliasIt != modifierActionWidgets.end())
                        widget = aliasIt->second;
                }

                if (widget == NULL) {
                    if (widgetSuffix && widgetSuffix[0] != '\0')
                        if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole("[WARNING] Widget '%s' not found in surface '%s' (file: %s, line %d)\n", widgetName.c_str(), zm->surface_->GetName(), GetRelativePath(filePath).c_str(), lineNumber);
                    continue;
                }

                if (hasHoldModifier && (!widget->GetIsTwoState() || widget->IsModifier())) {
                    if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole("[WARNING] Hold modifier will not work without normal button widget (mapping: '%s %s', file: %s, line %d)\n", tokens[0].c_str(), tokens[1].c_str(), GetRelativePath(filePath).c_str(), lineNumber);
                    continue;
                }
                zone->AddWidget(widget);

                vector<string> memberParams;
                for (int i = 1; i < (int) tokens.size(); ++i)
                    memberParams.push_back(tokens[i]);

                // Legacy .zon compatibility
                if (tokens[1] == "NullDisplay") continue;

                ActionContext* context;
                try {
                    context = zone->AddActionContext(widget, modifier, zone, tokens[1].c_str(), memberParams);
                } catch (const std::exception& e) {
                    LogToConsole("[ERROR] FAILED to AddActionContext for line '%s': %s\n", line.c_str(), e.what());
                    continue;
                }

                if (context == NULL) {
                    if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole("[WARNING] Action '%s' for widget '%s' returned NULL context (file: %s, line %d)\n", tokens[1].c_str(), widgetName.c_str(), GetRelativePath(filePath).c_str(), lineNumber);
                    continue;
                }

                if (context->GetAction()->IsModifier() && ModifierManager::IsModifierName(tokens[1].c_str())) {
                    modifierActionWidgets[tokens[1]] = widget;
                }

                if (IsSameString(tokens[1], "GoZone") || IsSameString(tokens[1], "GoSubZone")) {
                    string zoneName = context->GetStringParam();
                    if (zm->zoneInfo_.find(zoneName) != zm->zoneInfo_.end())
                        zm->zoneInfo_[zoneName].isReferenced = true;
                }
                if (IsSameString(tokens[1], "LastTouchedFXParam")) {
                    if (zm->zoneInfo_.find("LastTouchedFXParam") != zm->zoneInfo_.end())
                        zm->zoneInfo_["LastTouchedFXParam"].isReferenced = true;
                }

                if (isValueInverted) context->SetIsValueInverted();

                if (isFeedbackInverted) context->SetIsFeedbackInverted();

                if (hasHoldModifier) {
                    if (context->GetHoldDelay() == 0) context->SetHoldDelay(ActionContext::INHERIT_VALUE);
                    widget->SetHasHoldActions();
                }

                if (HasDoublePressPseudoModifier) {
                    context->SetDoublePress();
                    widget->SetHasDoublePressActions();
                }

                vector<double> range;

                if (isDecrease) {
                    range.push_back(-2.0);
                    range.push_back(1.0);
                    context->SetRange(range);
                } else if (isIncrease) {
                    range.push_back(0.0);
                    range.push_back(2.0);
                    context->SetRange(range);
                }
            }
        }

        if (zm->zoneInfo_.find(zone->GetName()) != zm->zoneInfo_.end())
            zm->zoneInfo_[zone->GetName()].isLoaded = true;
    } catch (const std::exception& e) {
        LogToConsole("[ERROR] FAILED to LoadZoneFile in %s, around line %d\n", zone->GetSourceFilePath(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }
}
