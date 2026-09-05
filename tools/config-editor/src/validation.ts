import path from "node:path";
import type { ActionTraits } from "./action-catalog.ts";
import { validateFormat2ZoneGestures, type EffectiveGestureSettings } from "./format2-zone-gesture.ts";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import type { AnyDocument } from "./formats.ts";
import type { ProductConfigRecord, ProductConfigSemantic } from "./product-config.ts";
import type { SettingsSchema } from "./settings-schema.ts";
import type { SurfaceSemantic } from "./surface.ts";
import type { ZoneBinding, ZoneDependencyReference, ZoneSemantic } from "./zone.ts";

interface ZoneLayerLocation {
    collection: "FX" | "Main";
    profileId: string;
    source: "User" | "Vendor";
}

const BUTTON_EVENTS = new Set(["Press", "Tap", "Release", "Hold", "LongHold", "DoublePress"]);

export interface ValidationOptions {
    actionTraits?: ReadonlyMap<string, ActionTraits>;
    availableZoneNamesByProfile?: Map<string, Set<string>>;
    settingsSchema?: SettingsSchema;
}

function zoneLayerLocation(documentPath?: string): ZoneLayerLocation | undefined {
    if (!documentPath) return undefined;
    const match = documentPath.replaceAll("\\", "/").match(/(?:^|\/)Zones\/(Vendor|User)\/([^/]+)\/(Main|FX)(?:\/|$)/);
    if (!match) return undefined;
    return { collection: match[3] as "FX" | "Main", profileId: match[2], source: match[1] as "User" | "Vendor" };
}

function isZoneLayerOverride(existing: AnyDocument, incoming: AnyDocument): boolean {
    const existingLocation = zoneLayerLocation(existing.path);
    const incomingLocation = zoneLayerLocation(incoming.path);
    const existingName = (existing.semantic as ZoneSemantic).name;
    const incomingName = (incoming.semantic as ZoneSemantic).name;
    return existingName?.toLowerCase() === incomingName?.toLowerCase() && existingLocation?.collection === incomingLocation?.collection && existingLocation?.profileId.toLowerCase() === incomingLocation?.profileId.toLowerCase() && existingLocation?.source !== incomingLocation?.source;
}

function zoneHeaderLine(document: AnyDocument): number | undefined {
    return document.lines.find((line) => line.kind === "header")?.lineNumber ?? document.lines.find((line) => line.kind === "format")?.lineNumber;
}

function zoneScope(document: AnyDocument): string {
    return zoneLayerLocation(document.path)?.profileId.toLowerCase() ?? "";
}

function zoneKey(document: AnyDocument, zoneName: string): string {
    return `${zoneScope(document)}\0${zoneLayerLocation(document.path)?.collection.toLowerCase() ?? "main"}\0${zoneName.toLowerCase()}`;
}

function addDuplicateZoneDiagnostic(diagnostics: Diagnostic[], document: AnyDocument, existing: AnyDocument, zoneName: string): void {
    const existingPath = existing.path ?? "another input file";
    const existingLine = zoneHeaderLine(existing);
    const profileId = zoneLayerLocation(document.path)?.profileId;
    const scope = profileId ? ` in profile "${profileId}"` : "";
    addDiagnostic(diagnostics, "error", "zones.name.duplicate", `Zone ID "${zoneName}" is also declared in ${existingPath}${existingLine ? `:${existingLine}` : ""}. Zone IDs are case-insensitive${scope}.`, zoneHeaderLine(document), document.path, existing.path ? [{ line: existingLine, path: existing.path }] : undefined);
}

function effectiveGestureSettings(device: ProductConfigRecord): EffectiveGestureSettings {
    const settings = device.effectiveSettings;
    const integer = (name: string): number | undefined => {
        const value = settings?.get(name);
        return value !== undefined && /^\d+$/.test(value) ? Number(value) : undefined;
    };
    return {
        defaultButtonTrigger: settings?.get("DefaultButtonTrigger"),
        defaultModifierMode: settings?.get("DefaultModifierMode"),
        doublePressPolicy: settings?.get("DoublePressPolicy"),
        doublePressWindowMs: integer("DoublePressWindowMs"),
        holdDelayMs: integer("HoldDelayMs"),
        longHoldDelayMs: integer("LongHoldDelayMs"),
    };
}

function resolveZoneBindings(document: AnyDocument, surfaceDocument: AnyDocument, diagnostics: Diagnostic[], effectiveSettings: EffectiveGestureSettings): ZoneBinding[] {
    const semantic = document.semantic as ZoneSemantic;
    const surface = surfaceDocument.semantic as SurfaceSemantic;
    const widgetsByName = new Map(surface.widgets.map((widget) => [widget.name, widget]));
    const resolved: ZoneBinding[] = [];
    const addMissingWidget = (widget: string, binding: ZoneBinding): void => addDiagnostic(diagnostics, "error", "format2.zone.widget.missing", `Widget does not exist on Surface ${surfaceDocument.path}: ${widget}`, binding.line, document.path, surfaceDocument.path ? [{ path: surfaceDocument.path }] : undefined);
    const resolveExact = (widgetName: string, binding: ZoneBinding): void => {
        const widget = widgetsByName.get(widgetName);
        if (!widget) {
            addMissingWidget(widgetName, binding);
            return;
        }
        const supportsPress = widget.body.some((line) => line.tokens[0] === "Input" && line.tokens[1] === "Press");
        if (binding.inputSelectors.includes("Modifier") && !supportsPress) {
            addDiagnostic(diagnostics, "error", "format2.zone.runtime.modifier-input", "A modifier declaration requires a Widget with press and release input", binding.line, document.path);
            return;
        }
        if (binding.inputSelectors.some((selector) => BUTTON_EVENTS.has(selector)) && !supportsPress) {
            addDiagnostic(diagnostics, "error", "format2.zone.runtime.button-input", `A button event requires a Widget with press and release input: ${widgetName}`, binding.line, document.path);
            return;
        }
        const inputSelectors = !binding.inputSelectors.length && supportsPress ? [effectiveSettings.defaultButtonTrigger ?? "Press"] : binding.inputSelectors;
        resolved.push({ ...binding, inputSelectors, widget: widgetName });
    };
    for (const binding of [...semantic.bindings, ...semantic.modifierDeclarations]) {
        if (!binding.widget.endsWith("#")) {
            resolveExact(binding.widget, binding);
            continue;
        }
        const baseName = binding.widget.slice(0, -1);
        for (let channel = 1; channel <= (surface.channels ?? 0); channel++) {
            const widget = `${baseName}${channel}`;
            resolveExact(widget, binding);
        }
    }
    const modifierModes = new Map<string, string>(resolved.filter((binding) => binding.modifierKind === "standard").map((binding): [string, string] => {
        const override = binding.properties.get("Mode");
        return [binding.widget, override && ["Momentary", "Latch", "Hybrid"].includes(override) ? override : effectiveSettings.defaultModifierMode ?? "Latch"];
    }));
    for (const binding of resolved) {
        if (!binding.inputSelectors.some((selector) => selector === "Hold" || selector === "LongHold")) continue;
        const mode = modifierModes.get(binding.widget);
        if (mode && mode !== "Latch") addDiagnostic(diagnostics, "error", "format2.zone.runtime.modifier-hold", "Hold and LongHold cannot use a Momentary or Hybrid modifier source Widget", binding.line, document.path);
    }
    return resolved;
}

function diagnosticIdentity(diagnostic: Diagnostic): string {
    const related = (diagnostic.related ?? []).map((location) => `${location.path}:${location.line ?? ""}`).sort().join("|");
    return `${diagnostic.code}\0${diagnostic.severity}\0${diagnostic.line ?? ""}\0${diagnostic.message}\0${related}`;
}

export function validateDocumentSet(documents: AnyDocument[], options: ValidationOptions = {}): Diagnostic[] {
    const diagnostics: Diagnostic[] = [];
    const zonesByKey = new Map<string, AnyDocument>();
    const fxZonesByLayer = new Map<string, AnyDocument>();
    const activeSurfaces = new Map<string, { document: AnyDocument; user: boolean }>();
    for (const document of documents) {
        if (document.format !== "surface" || !document.path) continue;
        const match = document.path.replaceAll("\\", "/").match(/(?:^|\/)Surfaces\/(Vendor|User)\/([^/]+)\.txt$/i);
        if (!match) continue;
        const surfaceId = match[2].toLowerCase();
        const user = match[1].toLowerCase() === "user";
        const existing = activeSurfaces.get(surfaceId);
        if (!existing || user || !existing.user) activeSurfaces.set(surfaceId, { document, user });
    }
    for (const document of documents) {
        if (document.format !== "product-config") continue;
        const assignments = (document.semantic as ProductConfigSemantic).records.filter((record): record is ProductConfigRecord => record.kind === "surface-assignment");
        const channelsByDevice = new Map<string, { channels: number; record: ProductConfigRecord }>();
        for (const assignment of assignments) {
            const deviceId = assignment.properties.get("Device")?.toLowerCase();
            const templateId = assignment.properties.get("Template")?.toLowerCase();
            const surfaceDocument = templateId ? activeSurfaces.get(templateId)?.document : undefined;
            const channels = surfaceDocument ? (surfaceDocument.semantic as SurfaceSemantic).channels : undefined;
            if (!deviceId || !channels) continue;
            const existing = channelsByDevice.get(deviceId);
            if (existing && existing.channels !== channels) {
                addDiagnostic(diagnostics, "error", "product.device.channels.conflict", `Device ${assignment.properties.get("Device")} is assigned to Surface templates with different Channels values: ${existing.channels} and ${channels}.`, assignment.line, document.path, [{ line: existing.record.line, path: document.path ?? "" }]);
            } else if (!existing) channelsByDevice.set(deviceId, { channels, record: assignment });
        }
    }
    const documentsByPath = new Map<string, AnyDocument>();
    for (const document of documents) {
        if (!document.path) continue;
        const lowercasePath = path.normalize(document.path).toLowerCase();
        const existing = documentsByPath.get(lowercasePath);
        if (existing) addDiagnostic(diagnostics, "error", "files.name.duplicate", `Filenames differ only by case or are duplicated: ${path.basename(existing.path ?? "")}, ${path.basename(document.path)}`, undefined, document.path);
        else documentsByPath.set(lowercasePath, document);
    }
    for (const document of documents) {
        if (document.format !== "zone") continue;
        const semantic = document.semantic as ZoneSemantic;
        if (!semantic.name) continue;
        const lowercaseName = semantic.name.toLowerCase();
        const location = zoneLayerLocation(document.path);
        if (location?.collection === "FX") {
            const layerKey = `${location.profileId.toLowerCase()}\0${location.source}\0${lowercaseName}`;
            const existingLayerZone = fxZonesByLayer.get(layerKey);
            if (existingLayerZone) {
                addDuplicateZoneDiagnostic(diagnostics, document, existingLayerZone, semantic.name);
                continue;
            }
            fxZonesByLayer.set(layerKey, document);
        }
        const key = zoneKey(document, semantic.name);
        const existing = zonesByKey.get(key);
        if (existing && isZoneLayerOverride(existing, document)) {
            if (location?.source === "User") zonesByKey.set(key, document);
        } else if (existing) addDuplicateZoneDiagnostic(diagnostics, document, existing, semantic.name);
        else zonesByKey.set(key, document);
    }
    if (options.actionTraits && options.settingsSchema) {
        const devices = new Map<string, ProductConfigRecord>();
        const assignments: Array<{ configPath?: string; record: ProductConfigRecord }> = [];
        for (const document of documents) {
            if (document.format !== "product-config") continue;
            for (const record of (document.semantic as ProductConfigSemantic).records) {
                if (record.kind === "device" && record.id) devices.set(record.id.toLowerCase(), record);
                else if (record.kind === "surface-assignment") assignments.push({ configPath: document.path, record });
            }
        }
        const contextsByProfile = new Map<string, Array<{ assignment: ProductConfigRecord; configPath?: string; device: ProductConfigRecord; surface: AnyDocument }>>();
        for (const assignment of assignments) {
            const templateId = assignment.record.properties.get("Template")?.toLowerCase();
            const deviceId = assignment.record.properties.get("Device")?.toLowerCase();
            const surface = templateId ? activeSurfaces.get(templateId)?.document : undefined;
            const device = deviceId ? devices.get(deviceId) : undefined;
            if (!templateId || !surface || !device) continue;
            const mainProfile = (assignment.record.properties.get("MainProfile") ?? templateId).toLowerCase();
            const fxProfile = (assignment.record.properties.get("FXProfile") ?? mainProfile).toLowerCase();
            for (const [collection, profile] of [["main", mainProfile], ["fx", fxProfile]] as const) {
                const key = `${profile}\0${collection}`;
                const contexts = contextsByProfile.get(key) ?? [];
                contexts.push({ assignment: assignment.record, configPath: assignment.configPath, device, surface });
                contextsByProfile.set(key, contexts);
            }
        }
        const emitted = new Set<string>();
        for (const document of zonesByKey.values()) {
            if (document.version !== "2") continue;
            const location = zoneLayerLocation(document.path);
            if (!location) continue;
            const contexts = contextsByProfile.get(`${location.profileId.toLowerCase()}\0${location.collection.toLowerCase()}`) ?? [];
            const existing = new Set(document.diagnostics.map(diagnosticIdentity));
            for (const context of contexts) {
                const resolvedDiagnostics: Diagnostic[] = [];
                const settings = effectiveGestureSettings(context.device);
                const resolvedBindings = resolveZoneBindings(document, context.surface, resolvedDiagnostics, settings);
                validateFormat2ZoneGestures(resolvedBindings, options.actionTraits, resolvedDiagnostics, options.settingsSchema, document.path, settings);
                for (const diagnostic of resolvedDiagnostics) {
                    const identity = diagnosticIdentity(diagnostic);
                    if (existing.has(identity) || emitted.has(identity)) continue;
                    if (context.configPath) diagnostic.related = [...diagnostic.related ?? [], { line: context.assignment.line, path: context.configPath }];
                    diagnostics.push(diagnostic);
                    emitted.add(identity);
                }
            }
        }
    }
    for (const document of zonesByKey.values()) {
        const semantic = document.semantic as ZoneSemantic;
        const availableNames = options.availableZoneNamesByProfile?.get(zoneScope(document));
        for (const dependency of semantic.dependencies) {
            if (zonesByKey.has(zoneKey(document, dependency)) || availableNames?.has(dependency.toLowerCase())) continue;
            const reference = semantic.dependencyReferences.find((candidate) => candidate.name.toLowerCase() === dependency.toLowerCase());
            addDiagnostic(diagnostics, "warning", "zones.dependency.missing", `Zone "${semantic.name}" references missing zone "${dependency}".`, reference?.line, document.path);
        }
    }

    const states = new Map<string, "done" | "visiting">();
    const stack: string[] = [];
    const reportedCycles = new Set<string>();
    const visitZone = (key: string, incoming?: { document: AnyDocument; reference: ZoneDependencyReference }): void => {
        if (states.get(key) === "done") return;
        if (states.get(key) === "visiting") {
            const cycleStart = stack.indexOf(key);
            const cycle = [...stack.slice(cycleStart), key];
            const cycleKey = [...new Set(cycle)].sort().join("\0");
            if (!reportedCycles.has(cycleKey)) {
                reportedCycles.add(cycleKey);
                addDiagnostic(diagnostics, "error", "zones.dependency.cycle", `Structural zone dependency cycle: ${cycle.map((zoneEntryKey) => (zonesByKey.get(zoneEntryKey)?.semantic as ZoneSemantic | undefined)?.name ?? zoneEntryKey).join(" -> ")}`, incoming?.reference.line, incoming?.document.path ?? zonesByKey.get(key)?.path);
            }
            return;
        }
        const document = zonesByKey.get(key);
        if (!document) return;
        states.set(key, "visiting");
        stack.push(key);
        const semantic = document.semantic as ZoneSemantic;
        for (const reference of semantic.dependencyReferences) if (reference.type === "IncludedZones" || reference.type === "SubZones" || reference.type === "ZoneLayers") visitZone(zoneKey(document, reference.name), { document, reference });
        stack.pop();
        states.set(key, "done");
    };
    for (const key of zonesByKey.keys()) visitZone(key);
    return diagnostics;
}
