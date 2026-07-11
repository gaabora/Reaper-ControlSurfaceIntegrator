#pragma once
// zone_parser.h — ZoneFileParser: extracts zone file parsing out of ZoneManager.
// ZoneManager::LoadZoneFile(zone, filePath, widgetSuffix) delegates to ZoneFileParser::ParseFile
// so that the file-reading / token-processing logic lives in its own translation unit, separate from ZoneManager's zone lifecycle and routing concerns.
// ZoneFileParser is declared friend of ZoneManager so it can access the private members (surface_, zoneInfo_, LoadZones, GetWidgetNameAndModifiers) 
// that the parsing logic needs without exposing them as public API.

#include "preamble.h"
#include "zone_manager.h"

class ZoneFileParser
{
public:
    static void ParseFile(ZoneManager* zm, Zone* zone, const char* filePath, const char* widgetSuffix);
};
