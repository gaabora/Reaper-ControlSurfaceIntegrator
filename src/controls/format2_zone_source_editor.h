#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "format2_zone_document.h"

struct Format2ZoneWidgetEditResult {
    bool success = false;
    bool editedChannelFamily = false;
    std::string channelFamilyBaseName;
    std::vector<std::string> lines;
    std::string message;
    Format2ZoneParseResult parsed;
};

Format2ZoneWidgetEditResult EditFormat2ZoneWidgetSource(const std::string& sourcePath, const std::vector<std::string>& originalLines,
    const std::string& widgetName, int surfaceChannelCount, const std::vector<std::string>& replacementLines);
