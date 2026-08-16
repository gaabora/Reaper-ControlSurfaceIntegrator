import path from "node:path";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import type { AnyDocument } from "./formats.ts";
import type { ZoneDependencyReference, ZoneSemantic } from "./zone.ts";

export function validateDocumentSet(documents: AnyDocument[]): Diagnostic[] {
    const diagnostics: Diagnostic[] = [];
    const zonesByName = new Map<string, AnyDocument>();
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
        const existing = zonesByName.get(lowercaseName);
        if (existing) addDiagnostic(diagnostics, "error", "zones.name.duplicate", `Zone name is duplicated case-insensitively: ${semantic.name}`, undefined, document.path);
        else zonesByName.set(lowercaseName, document);
    }
    for (const document of documents) {
        if (document.format !== "zone") continue;
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
