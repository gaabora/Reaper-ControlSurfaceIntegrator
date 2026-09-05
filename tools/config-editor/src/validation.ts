import path from "node:path";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import type { AnyDocument } from "./formats.ts";
import type { ProductConfigRecord, ProductConfigSemantic } from "./product-config.ts";
import type { SurfaceSemantic } from "./surface.ts";
import type { ZoneDependencyReference, ZoneSemantic } from "./zone.ts";

interface ZoneLayerLocation {
    collection: "FX" | "Main";
    profileId: string;
    source: "User" | "Vendor";
}

export interface ValidationOptions {
    availableZoneNamesByProfile?: Map<string, Set<string>>;
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
