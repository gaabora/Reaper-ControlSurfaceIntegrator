import { addDiagnostic, type Diagnostic, type LosslessDocument } from "./model.ts";
import { initializeLine, isStableId, parseProperties, propertyValue, splitSourceLines } from "./text.ts";

const PRODUCT_CONFIG_COMMENT_PREFIXES = ["#"];

export interface ProductConfigRecord {
    kind: "broadcaster" | "listener" | "page" | "surface-assignment" | "surface-type" | "unknown";
    line: number;
    properties: Map<string, string>;
}

export interface ProductConfigSemantic {
    records: ProductConfigRecord[];
}

export function parseProductConfig(source: string, documentPath?: string): LosslessDocument<ProductConfigSemantic> {
    const lines = splitSourceLines(source);
    const diagnostics: Diagnostic[] = [];
    const records: ProductConfigRecord[] = [];
    let version = "unversioned";
    let firstContentLine: number | undefined;

    for (const line of lines) {
        const text = initializeLine(line, PRODUCT_CONFIG_COMMENT_PREFIXES);
        if (!text || line.kind === "comment") continue;
        if (!firstContentLine) firstContentLine = line.lineNumber;
        const properties = parseProperties(line.tokens);
        const versionValue = propertyValue(properties, "Version");
        if (versionValue) {
            line.kind = "format";
            if (version !== "unversioned") addDiagnostic(diagnostics, "error", "product.version.duplicate", "Product config Version is duplicated", line.lineNumber, documentPath);
            version = versionValue;
            if (line.lineNumber !== 1) addDiagnostic(diagnostics, "error", "product.version.position", "Version must be the first physical line for the runtime parser", line.lineNumber, documentPath);
            if (version !== "7.0") addDiagnostic(diagnostics, "error", "product.version.unsupported", `Unsupported product config version: ${version}`, line.lineNumber, documentPath);
            continue;
        }

        let kind: ProductConfigRecord["kind"] = "unknown";
        if (propertyValue(properties, "SurfaceType")) kind = "surface-type";
        else if (propertyValue(properties, "PageName")) kind = "page";
        else if (propertyValue(properties, "Broadcaster")) kind = "broadcaster";
        else if (propertyValue(properties, "Listener")) kind = "listener";
        else if (propertyValue(properties, "Surface")) kind = "surface-assignment";
        records.push({ kind, line: line.lineNumber, properties });
        line.kind = kind === "unknown" ? "unknown" : "entry";
        if (kind === "unknown") addDiagnostic(diagnostics, "warning", "product.line.unknown", `Unknown product config line: ${text}`, line.lineNumber, documentPath);

        if (kind === "surface-type") {
            for (const key of ["SurfaceType", "SurfaceName", "SurfaceChannelCount"]) if (!propertyValue(properties, key)) addDiagnostic(diagnostics, "error", "product.surface-type.property", `SurfaceType record requires ${key}`, line.lineNumber, documentPath);
        }
        if (kind === "surface-assignment") {
            const surfaceId = propertyValue(properties, "SurfaceFolder") ?? "";
            if (!isStableId(surfaceId)) addDiagnostic(diagnostics, "error", "product.surface.id", "SurfaceFolder must contain a stable surface ID", line.lineNumber, documentPath);
            for (const key of ["ZoneFolder", "FXZoneFolder"]) {
                const profileId = propertyValue(properties, key);
                if (profileId && !isStableId(profileId)) addDiagnostic(diagnostics, "error", "product.zone.id", `${key} must contain a stable profile ID`, line.lineNumber, documentPath);
            }
        }
    }

    if (version === "unversioned") addDiagnostic(diagnostics, "error", "product.version.missing", "Product config has no Version", firstContentLine, documentPath);
    const surfaceNames = new Map<string, ProductConfigRecord>();
    for (const record of records.filter((candidate) => candidate.kind === "surface-type")) {
        const name = propertyValue(record.properties, "SurfaceName");
        if (!name) continue;
        const existing = surfaceNames.get(name.toLowerCase());
        if (existing) addDiagnostic(diagnostics, "error", "product.surface.duplicate", `Surface names differ only by case or are duplicated: ${name}`, record.line, documentPath);
        else surfaceNames.set(name.toLowerCase(), record);
    }
    return { diagnostics, format: "product-config", lines, path: documentPath, semantic: { records }, source, version };
}
