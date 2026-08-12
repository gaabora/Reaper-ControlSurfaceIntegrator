import type { AnyDocument } from "./formats.ts";
import { propertyValue } from "./text.ts";
import type { SurfaceSemantic, SurfaceWidget } from "./surface.ts";

export type WidgetCapability = "absolute-input" | "color-feedback" | "meter-feedback" | "press-input" | "relative-input" | "text-feedback" | "toggle-feedback" | "touch-input" | "value-feedback";
export type WidgetRole = "button" | "display" | "fader" | "meter" | "rotary" | "unknown";

export interface WidgetCandidate {
    capabilities: WidgetCapability[];
    name: string;
    role: WidgetRole;
}

const METADATA_CAPABILITIES = new Map<string, WidgetCapability>([
    ["absolute-input", "absolute-input"],
    ["color-feedback", "color-feedback"],
    ["meter-feedback", "meter-feedback"],
    ["press-input", "press-input"],
    ["relative-input", "relative-input"],
    ["text-feedback", "text-feedback"],
    ["toggle-feedback", "toggle-feedback"],
    ["touch-input", "touch-input"],
    ["value-feedback", "value-feedback"],
]);

export function normalizedWidgetName(widgetName: string): string {
    return widgetName.toLowerCase();
}

function metadataCapabilities(value: string | undefined, suffix: "feedback" | "input"): WidgetCapability[] {
    if (!value) return [];
    const capabilities: WidgetCapability[] = [];
    for (const token of value.split("+")) {
        const capability = METADATA_CAPABILITIES.get(`${token.toLowerCase()}-${suffix}`);
        if (capability) capabilities.push(capability);
    }
    return capabilities;
}

export function widgetCapabilities(widget: SurfaceWidget, metadata: Map<string, string> = widget.oskProperties): WidgetCapability[] {
    const capabilities = new Set<WidgetCapability>();
    for (const line of widget.body) {
        const widgetType = (line.tokens[0] ?? "").toLowerCase();
        if (widgetType === "press" || widgetType === "anypress") capabilities.add("press-input");
        else if (widgetType === "touch") capabilities.add("touch-input");
        else if (["encoder", "mftencoder", "encoderplain", "encoder7bit", "x32rotarytoencoder"].includes(widgetType)) {
            capabilities.add("relative-input");
            capabilities.add("value-feedback");
        } else if (["fader14bit", "faderportclassicfader14bit", "fader7bit", "x32fader"].includes(widgetType)) {
            capabilities.add("absolute-input");
            capabilities.add("value-feedback");
        } else if (widgetType.startsWith("fb_fader") || ["fb_encoder", "fb_asparionencoder", "fb_sce24encoder", "fb_faderportvaluebar"].includes(widgetType) || widgetType.includes("processor")) capabilities.add("value-feedback");
        else if (widgetType.startsWith("fb_mcu") || widgetType.includes("display") || widgetType.includes("scribble")) capabilities.add("text-feedback");
        else if (widgetType.includes("vumeter") || widgetType.includes("meter")) {
            capabilities.add("meter-feedback");
            capabilities.add("value-feedback");
        } else if (widgetType.includes("rgb") || widgetType.includes("twostate")) {
            capabilities.add("toggle-feedback");
            capabilities.add("color-feedback");
        }
    }
    const declaredInput = propertyValue(metadata, "Input");
    if (declaredInput) {
        for (const capability of capabilities) if (capability.endsWith("-input")) capabilities.delete(capability);
        for (const capability of metadataCapabilities(declaredInput, "input")) capabilities.add(capability);
    }
    const declaredFeedback = propertyValue(metadata, "Feedback");
    if (declaredFeedback) {
        for (const capability of capabilities) if (capability.endsWith("-feedback")) capabilities.delete(capability);
        for (const capability of metadataCapabilities(declaredFeedback, "feedback")) capabilities.add(capability);
    }
    return [...capabilities].sort();
}

export function widgetRole(widget: SurfaceWidget, capabilities: WidgetCapability[] = widgetCapabilities(widget), metadata: Map<string, string> = widget.oskProperties): WidgetRole {
    const declaredRole = propertyValue(metadata, "Role")?.toLowerCase();
    if (["button", "display", "fader", "meter", "rotary"].includes(declaredRole ?? "")) return declaredRole as WidgetRole;
    if (capabilities.includes("absolute-input")) return "fader";
    if (capabilities.includes("relative-input") || widget.widgetClass?.toLowerCase().includes("rotary") || widget.widgetClass?.toLowerCase().includes("encoder")) return "rotary";
    if (capabilities.includes("meter-feedback")) return "meter";
    if (capabilities.includes("text-feedback")) return "display";
    if (capabilities.includes("press-input")) return "button";
    return "unknown";
}

function commonWidgetCapabilities(widgets: SurfaceWidget[], layoutMetadata: Map<string, Map<string, string>>): WidgetCapability[] {
    if (!widgets.length) return [];
    const capabilities = new Set(widgetCapabilities(widgets[0], layoutMetadata.get(normalizedWidgetName(widgets[0].name)) ?? widgets[0].oskProperties));
    for (const widget of widgets.slice(1)) {
        const metadata = layoutMetadata.get(normalizedWidgetName(widget.name)) ?? widget.oskProperties;
        const current = new Set(widgetCapabilities(widget, metadata));
        for (const capability of capabilities) if (!current.has(capability)) capabilities.delete(capability);
    }
    return [...capabilities].sort();
}

export function surfaceWidgetSlots(surface: AnyDocument, patternSlots: boolean): WidgetCandidate[] {
    const semantic = surface.semantic as SurfaceSemantic;
    const widgets = semantic.widgets;
    const layoutMetadata = new Map<string, Map<string, string>>();
    for (const row of semantic.oskLayout?.rows ?? []) for (const cell of row.cells) if (cell.widgetName) layoutMetadata.set(normalizedWidgetName(cell.widgetName), cell.properties);
    if (!patternSlots) return widgets.map((widget) => {
        const metadata = layoutMetadata.get(normalizedWidgetName(widget.name)) ?? widget.oskProperties;
        const capabilities = widgetCapabilities(widget, metadata);
        return { capabilities, name: widget.name, role: widgetRole(widget, capabilities, metadata) };
    });
    const families = new Map<string, SurfaceWidget[]>();
    for (const widget of widgets) {
        const match = widget.name.match(/^(.*\D)(\d+)$/);
        if (!match) continue;
        const familyName = `${match[1]}|`;
        const family = families.get(normalizedWidgetName(familyName)) ?? [];
        family.push(widget);
        families.set(normalizedWidgetName(familyName), family);
    }
    return [...families.values()].map((family) => {
        const capabilities = commonWidgetCapabilities(family, layoutMetadata);
        const roles = new Set(family.map((widget) => {
            const metadata = layoutMetadata.get(normalizedWidgetName(widget.name)) ?? widget.oskProperties;
            return widgetRole(widget, widgetCapabilities(widget, metadata), metadata);
        }));
        const familyRole = roles.size === 1 ? [...roles][0] ?? "unknown" : "unknown";
        return { capabilities, name: `${family[0].name.replace(/\d+$/, "")}|`, role: familyRole };
    });
}

export function isCompatible(requiredCapabilities: WidgetCapability[], candidateCapabilities: WidgetCapability[]): boolean {
    const available = new Set(candidateCapabilities);
    return requiredCapabilities.every((capability) => available.has(capability));
}
