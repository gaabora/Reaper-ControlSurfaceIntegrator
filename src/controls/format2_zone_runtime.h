#pragma once

#include <vector>

#include "format2_zone_document.h"

class Zone;
class ZoneManager;

struct Format2ZoneRuntimeResult {
    std::vector<Format2Diagnostic> diagnostics;

    bool IsValid() const { return !HasFormat2DiagnosticErrors(this->diagnostics); }
};

Format2ZoneRuntimeResult LoadFormat2ZoneRuntimeBindings(ZoneManager* zoneManager, Zone* zone, const Format2ZoneParseResult& parsed, const Format2DocumentMetadata* inheritedMetadata = nullptr);
