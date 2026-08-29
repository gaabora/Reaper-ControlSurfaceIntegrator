#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "format2_zone_document.h"

struct Format2ActionContextSpec {
    std::size_t bindingIndex = 0;
    std::string widgetId;
    std::optional<int> surfaceChannelOffset;
};

struct Format2ZoneCompileResult {
    std::vector<Format2ActionContextSpec> actionContexts;
    std::vector<Format2Diagnostic> diagnostics;

    bool IsValid() const { return this->diagnostics.empty(); }
};

Format2ZoneCompileResult CompileFormat2ZoneBindings(const std::vector<Format2ZoneBinding>& bindings, int surfaceChannelCount);
