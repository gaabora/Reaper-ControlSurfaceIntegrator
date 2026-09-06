import type { AnyDocument } from "./formats.ts";
import type { LearnFxSemantic } from "./learn-fx.ts";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import type { SurfaceSemantic, SurfaceWidget } from "./surface.ts";
import { widgetCapabilities } from "./widget-capabilities.ts";

function supportsTwoStateInput(widget: SurfaceWidget): boolean {
    return widget.body.some((line) => {
        const primitive = (line.tokens[0] ?? "").toLowerCase();
        if (primitive === "press") return line.tokens.length >= 7;
        return primitive === "input" && (line.tokens[1] ?? "").toLowerCase() === "press" && /(?:^|\s)Off=/.test(line.text);
    });
}

function supportsLearnFxRole(widget: SurfaceWidget, role: string): boolean {
    const capabilities = widgetCapabilities(widget);
    if (role === "NameDisplay" || role === "ValueDisplay") return capabilities.includes("text-feedback");
    return capabilities.includes("absolute-input") || capabilities.includes("relative-input") || supportsTwoStateInput(widget);
}

function compatibleWidgetNames(surface: SurfaceSemantic, role: string, family: boolean): string[] {
    if (!family) return surface.widgets.filter((widget) => supportsLearnFxRole(widget, role)).map((widget) => widget.name).sort((left, right) => left.localeCompare(right));
    if (!surface.channels) return [];
    const prefixes = new Set<string>();
    for (const widget of surface.widgets) {
        const match = widget.name.match(/^(.*\D)(\d+)$/);
        if (match) prefixes.add(match[1]);
    }
    return [...prefixes].filter((prefix) => {
        for (let channel = 1; channel <= surface.channels!; channel++) {
            const widget = surface.widgets.find((candidate) => candidate.name === `${prefix}${channel}`);
            if (!widget || !supportsLearnFxRole(widget, role)) return false;
        }
        return true;
    }).map((prefix) => `${prefix}#`).sort((left, right) => left.localeCompare(right));
}

function requirementForRole(role: string): string {
    return role === "Parameter" ? "absolute, relative, or two-state input" : "text feedback";
}

function suggestionSuffix(surface: SurfaceSemantic, role: string, family: boolean): string {
    const names = compatibleWidgetNames(surface, role, family);
    return names.length ? ` Compatible Widgets: ${names.join(", ")}.` : "";
}

export function validateLearnFxSurface(learnFx: AnyDocument, surface: AnyDocument): Diagnostic[] {
    const diagnostics: Diagnostic[] = [];
    const learnFxSemantic = learnFx.semantic as LearnFxSemantic;
    const surfaceSemantic = surface.semantic as SurfaceSemantic;
    const resolvedRoles = new Map<string, string>();
    let resolvedParameterCount = 0;
    for (const entry of learnFxSemantic.widgets) {
        const family = entry.selector.endsWith("#");
        const baseName = family ? entry.selector.slice(0, -1) : entry.selector;
        if (family && !surfaceSemantic.channels) {
            addDiagnostic(diagnostics, "error", "format2.learn-fx.surface.channels", "A terminal # selector requires a positive Surface channel count.", entry.line, learnFx.path);
            continue;
        }
        const widgetNames = family ? Array.from({ length: surfaceSemantic.channels! }, (_, channelIdx) => `${baseName}${channelIdx + 1}`) : [baseName];
        for (const widgetName of widgetNames) {
            const surfaceWidget = surfaceSemantic.widgets.find((widget) => widget.name === widgetName);
            if (!surfaceWidget) {
                addDiagnostic(diagnostics, "error", "format2.learn-fx.surface.widget.missing", `FXWidgets selector ${entry.selector} requires missing Surface Widget ${widgetName}.${suggestionSuffix(surfaceSemantic, entry.role, family)}`, entry.line, learnFx.path);
                continue;
            }
            const supportsRole = supportsLearnFxRole(surfaceWidget, entry.role);
            if (!supportsRole) addDiagnostic(diagnostics, "error", "format2.learn-fx.surface.widget.capability", `Surface Widget ${widgetName} cannot be used as ${entry.role}; it requires ${requirementForRole(entry.role)}.${suggestionSuffix(surfaceSemantic, entry.role, family)}`, entry.line, learnFx.path, surface.path ? [{ line: surfaceWidget.line, path: surface.path }] : undefined);
            else if (entry.role === "Parameter") resolvedParameterCount++;
            const previousRole = resolvedRoles.get(widgetName);
            if (previousRole) addDiagnostic(diagnostics, "error", "format2.learn-fx.surface.widget.overlap", `Surface Widget ${widgetName} is selected more than once for ${previousRole} and ${entry.role}.`, entry.line, learnFx.path, surface.path ? [{ line: surfaceWidget.line, path: surface.path }] : undefined);
            else resolvedRoles.set(widgetName, entry.role);
        }
    }
    if (resolvedParameterCount === 0) addDiagnostic(diagnostics, "error", "format2.learn-fx.surface.parameter.required", "LearnFX.fxzon must resolve at least one input-capable Parameter Widget on the selected Surface.", learnFxSemantic.widgets[0]?.line, learnFx.path);
    return diagnostics;
}
