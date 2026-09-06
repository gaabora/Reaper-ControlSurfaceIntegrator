#include "format2_learn_fx_surface.h"

#include <map>

static bool Format2LearnFxWidgetHasCapability(const Format2SurfaceWidget& widget, Format2Capability capability) {
    for (Format2Capability existing : widget.capabilities) if (existing == capability) return true;
    return false;
}

static bool Format2LearnFxWidgetSupportsRole(const Format2SurfaceWidget& widget, Format2LearnFxWidgetRole role) {
    if (role == Format2LearnFxWidgetRole::NameDisplay || role == Format2LearnFxWidgetRole::ValueDisplay) return Format2LearnFxWidgetHasCapability(widget, Format2Capability::Text);
    const bool hasTwoStateInput = Format2LearnFxWidgetHasCapability(widget, Format2Capability::Press) && Format2LearnFxWidgetHasCapability(widget, Format2Capability::Release);
    return hasTwoStateInput || Format2LearnFxWidgetHasCapability(widget, Format2Capability::Absolute) || Format2LearnFxWidgetHasCapability(widget, Format2Capability::Relative);
}

static const char* Format2LearnFxRoleName(Format2LearnFxWidgetRole role) {
    if (role == Format2LearnFxWidgetRole::NameDisplay) return "NameDisplay";
    if (role == Format2LearnFxWidgetRole::ValueDisplay) return "ValueDisplay";
    return "Parameter";
}

static std::size_t FindFormat2SurfaceWidget(const Format2SurfaceDocument& surface, const std::string& widgetId) {
    for (std::size_t widgetIdx = 0; widgetIdx < surface.widgets.size(); widgetIdx++) if (surface.widgets[widgetIdx].id == widgetId) return widgetIdx;
    return surface.widgets.size();
}

Format2LearnFxSurfaceResolveResult ResolveFormat2LearnFxSurface(const Format2LearnFxDocument& learnFx, const Format2SurfaceParseResult& surface) {
    Format2LearnFxSurfaceResolveResult result;
    std::map<std::string, Format2LearnFxWidgetRole> resolvedRoles;
    int resolvedParameterCount = 0;

    for (std::size_t learnFxWidgetIdx = 0; learnFxWidgetIdx < learnFx.widgets.size(); learnFxWidgetIdx++) {
        const Format2LearnFxWidget& entry = learnFx.widgets[learnFxWidgetIdx];
        std::vector<std::string> widgetIds;
        if (entry.selector.kind == Format2WidgetSelectorKind::Exact) {
            widgetIds.push_back(entry.selector.baseName);
        } else if (!surface.document.metadata.channels || *surface.document.metadata.channels < 1) {
            result.diagnostics.push_back({"format2.learn-fx.surface.channels", "A terminal # selector requires a positive Surface channel count", entry.selector.location});
            continue;
        } else {
            for (int channel = 1; channel <= *surface.document.metadata.channels; channel++) widgetIds.push_back(entry.selector.baseName + std::to_string(channel));
        }

        for (const std::string& widgetId : widgetIds) {
            const std::size_t surfaceWidgetIdx = FindFormat2SurfaceWidget(surface.surface, widgetId);
            if (surfaceWidgetIdx == surface.surface.widgets.size()) {
                result.diagnostics.push_back({"format2.learn-fx.surface.widget.missing", "FXWidgets selector " + entry.selector.source + " requires missing Surface Widget " + widgetId, entry.selector.location});
                continue;
            }
            const Format2SurfaceWidget& widget = surface.surface.widgets[surfaceWidgetIdx];
            const bool supportsRole = Format2LearnFxWidgetSupportsRole(widget, entry.role);
            if (!supportsRole) {
                const std::string requirement = entry.role == Format2LearnFxWidgetRole::Parameter ? "absolute, relative, or two-state input" : "text feedback";
                result.diagnostics.push_back({"format2.learn-fx.surface.widget.capability", "Surface Widget " + widgetId + " cannot be used as " + Format2LearnFxRoleName(entry.role) + "; it requires " + requirement, entry.selector.location});
            } else if (entry.role == Format2LearnFxWidgetRole::Parameter) {
                resolvedParameterCount++;
            }

            const auto previousRole = resolvedRoles.find(widgetId);
            if (previousRole != resolvedRoles.end()) {
                result.diagnostics.push_back({"format2.learn-fx.surface.widget.overlap", "Surface Widget " + widgetId + " is selected more than once for " + Format2LearnFxRoleName(previousRole->second) + " and " + Format2LearnFxRoleName(entry.role), entry.selector.location});
            } else {
                resolvedRoles[widgetId] = entry.role;
            }
            result.widgets.push_back({entry.role, widgetId, learnFxWidgetIdx, surfaceWidgetIdx});
        }
    }

    if (resolvedParameterCount == 0) {
        const Format2SourceLocation location = learnFx.widgets.empty() ? Format2SourceLocation{} : learnFx.widgets.front().location;
        result.diagnostics.push_back({"format2.learn-fx.surface.parameter.required", "LearnFX.fxzon must resolve at least one input-capable Parameter Widget on the selected Surface", location});
    }
    return result;
}
