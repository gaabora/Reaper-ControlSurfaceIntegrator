import path from "node:path";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import type { AnyDocument } from "./formats.ts";
import type { ZoneDependencyReference, ZoneSemantic } from "./zone.ts";

interface ZoneLayerLocation {
    collection: "FX" | "Main";
    profileId: string;
    source: "User" | "Vendor";
}

function zoneLayerLocation(documentPath?: string): ZoneLayerLocation | undefined {
    if (!documentPath) return undefined;
    const match = documentPath.replaceAll("\\", "/").match(/(?:^|\/)Zones\/(Vendor|User)\/([^/]+)\/(Main|FX)(?:\/|$)/);
    if (!match) return undefined;
    return { collection: match[3] as "FX" | "Main", profileId: match[2], source: match[1] as "User" | "Vendor" };
}

function isFxLayerOverride(existing: AnyDocument, incoming: AnyDocument): boolean {
    const existingLocation = zoneLayerLocation(existing.path);
    const incomingLocation = zoneLayerLocation(incoming.path);
    const existingName = (existing.semantic as ZoneSemantic).name;
    const incomingName = (incoming.semantic as ZoneSemantic).name;
    return existingName === incomingName && existingLocation?.collection === "FX" && incomingLocation?.collection === "FX" && existingLocation.profileId === incomingLocation.profileId && existingLocation.source !== incomingLocation.source;
}

export function validateDocumentSet(documents: AnyDocument[]): Diagnostic[] {
    const diagnostics: Diagnostic[] = [];
    const zonesByName = new Map<string, AnyDocument>();
    const fxZonesByLayer = new Map<string, AnyDocument>();
    const userMainProfiles = new Set<string>();
    for (const document of documents) {
        const location = zoneLayerLocation(document.path);
        if (location?.collection === "Main" && location.source === "User") userMainProfiles.add(location.profileId);
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
        if (location?.collection === "Main" && location.source === "Vendor" && userMainProfiles.has(location.profileId)) continue;
        if (location?.collection === "FX") {
            const layerKey = `${location.profileId}\0${location.source}\0${lowercaseName}`;
            if (fxZonesByLayer.has(layerKey)) {
                addDiagnostic(diagnostics, "error", "zones.name.duplicate", `Zone name is duplicated case-insensitively in the same FX layer: ${semantic.name}`, undefined, document.path);
                continue;
            }
            fxZonesByLayer.set(layerKey, document);
        }
        const existing = zonesByName.get(lowercaseName);
        if (existing && isFxLayerOverride(existing, document)) {
            if (location?.source === "User") zonesByName.set(lowercaseName, document);
        } else if (existing) addDiagnostic(diagnostics, "error", "zones.name.duplicate", `Zone name is duplicated case-insensitively: ${semantic.name}`, undefined, document.path);
        else zonesByName.set(lowercaseName, document);
    }
    for (const document of zonesByName.values()) {
        const semantic = document.semantic as ZoneSemantic;
        for (const dependency of semantic.dependencies) if (!zonesByName.has(dependency.toLowerCase())) addDiagnostic(diagnostics, "warning", "zones.dependency.missing", `Referenced zone was not included in this validation set: ${dependency}`, undefined, document.path);
    }

    const states = new Map<string, "done" | "visiting">();
    const stack: string[] = [];
    const reportedCycles = new Set<string>();
    const visitZone = (zoneName: string, incoming?: { document: AnyDocument; reference: ZoneDependencyReference }): void => {
        if (states.get(zoneName) === "done") return;
        if (states.get(zoneName) === "visiting") {
            const cycleStart = stack.indexOf(zoneName);
            const cycle = [...stack.slice(cycleStart), zoneName];
            const cycleKey = [...new Set(cycle)].sort().join("\0");
            if (!reportedCycles.has(cycleKey)) {
                reportedCycles.add(cycleKey);
                addDiagnostic(diagnostics, "error", "zones.dependency.cycle", `Zone dependency cycle: ${cycle.map((name) => (zonesByName.get(name)?.semantic as ZoneSemantic | undefined)?.name ?? name).join(" -> ")}`, incoming?.reference.line, incoming?.document.path ?? zonesByName.get(zoneName)?.path);
            }
            return;
        }
        const document = zonesByName.get(zoneName);
        if (!document) return;
        states.set(zoneName, "visiting");
        stack.push(zoneName);
        const semantic = document.semantic as ZoneSemantic;
        for (const reference of semantic.dependencyReferences) visitZone(reference.name.toLowerCase(), { document, reference });
        stack.pop();
        states.set(zoneName, "done");
    };
    for (const zoneName of zonesByName.keys()) visitZone(zoneName);
    return diagnostics;
}
