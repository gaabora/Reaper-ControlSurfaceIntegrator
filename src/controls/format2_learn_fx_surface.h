#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "format2_learn_fx_document.h"
#include "format2_surface_document.h"

struct Format2ResolvedLearnFxWidget {
    Format2LearnFxWidgetRole role = Format2LearnFxWidgetRole::Parameter;
    std::string widgetId;
    std::size_t learnFxWidgetIndex = 0;
    std::size_t surfaceWidgetIndex = 0;
};

struct Format2LearnFxSurfaceResolveResult {
    std::vector<Format2ResolvedLearnFxWidget> widgets;
    std::vector<Format2Diagnostic> diagnostics;

    bool IsValid() const { return !HasFormat2DiagnosticErrors(this->diagnostics); }
};

Format2LearnFxSurfaceResolveResult ResolveFormat2LearnFxSurface(const Format2LearnFxDocument& learnFx, const Format2SurfaceParseResult& surface);
