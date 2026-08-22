import { addDiagnostic, type Diagnostic, type LosslessDocument } from "./model.ts";
import type { SettingDefinition, SettingsSchema } from "./settings-schema.ts";
import { initializeLine, isStableId, parseProperties, propertyValue, splitSourceLines } from "./text.ts";

const PRODUCT_CONFIG_COMMENT_PREFIXES = ["#"];

export interface ProductConfigRecord {
    kind: "broadcaster" | "listener" | "page" | "settings" | "surface-assignment" | "surface-type" | "unknown";
    line: number;
    properties: Map<string, string>;
}

export interface ProductConfigSemantic {
    records: ProductConfigRecord[];
}

interface SettingOverrideSource {
    line: number;
    value: string;
}

interface SettingOverrideSet {
    overrides: Map<string, SettingOverrideSource>;
    valid: boolean;
}

function validateSettingValue(definition: SettingDefinition, value: string, line: number, diagnostics: Diagnostic[], documentPath?: string): boolean {
    if (definition.type === "boolean") {
        if (value === "0" || value === "1") return true;
        addDiagnostic(diagnostics, "error", "product.setting.boolean", `${definition.name} must be 0 or 1`, line, documentPath);
        return false;
    }
    if (definition.type === "enum") {
        if (definition.enumValues?.includes(value)) return true;
        addDiagnostic(diagnostics, "error", "product.setting.enum", `${definition.name} must be one of ${(definition.enumValues ?? []).join(", ")}`, line, documentPath);
        return false;
    }
    if (!/^\d+$/.test(value)) {
        addDiagnostic(diagnostics, "error", "product.setting.integer", `${definition.name} must be a complete integer`, line, documentPath);
        return false;
    }
    const integerValue = Number(value);
    if (!Number.isSafeInteger(integerValue) || integerValue < definition.min! || integerValue > definition.max!) {
        addDiagnostic(diagnostics, "error", "product.setting.range", `${definition.name} must be from ${definition.min} to ${definition.max}`, line, documentPath);
        return false;
    }
    return true;
}

function resolveSettings(base: Map<string, string>, overrideSet: SettingOverrideSet, scope: string, schema: SettingsSchema, diagnostics: Diagnostic[], documentPath?: string): Map<string, string> {
    const result = new Map(base);
    let valuesValid = true;
    const overrides = overrideSet.overrides;
    for (const [settingName, override] of overrides) {
        const definition = schema.settings.find((setting) => setting.name === settingName);
        if (!definition) {
            addDiagnostic(diagnostics, "error", "product.setting.unknown", `Unknown setting: ${settingName}`, override.line, documentPath);
            valuesValid = false;
            continue;
        }
        if (!definition.scopes.includes(scope)) {
            addDiagnostic(diagnostics, "error", "product.setting.scope", `${settingName} is not allowed in ${scope} scope`, override.line, documentPath);
            valuesValid = false;
            continue;
        }
        if (!validateSettingValue(definition, override.value, override.line, diagnostics, documentPath)) {
            valuesValid = false;
            continue;
        }
        result.set(settingName, override.value);
    }
    if (valuesValid) {
        for (const definition of schema.settings) {
            if (!definition.greaterThan) continue;
            const value = Number(result.get(definition.name));
            const referencedValue = Number(result.get(definition.greaterThan));
            if (value > referencedValue) continue;
            addDiagnostic(diagnostics, "error", "product.setting.relationship", `${definition.name} must be greater than ${definition.greaterThan}`, overrides.get(definition.name)?.line ?? overrides.get(definition.greaterThan)?.line, documentPath);
            valuesValid = false;
        }
    }
    return overrideSet.valid && valuesValid ? result : new Map(base);
}

function addSettingTokens(tokens: string[], startIdx: number, line: number, target: Map<string, SettingOverrideSource>, schema: SettingsSchema, diagnostics: Diagnostic[], documentPath?: string): boolean {
    const definitions = new Set(schema.settings.map((definition) => definition.name));
    let valid = true;
    for (let tokenIdx = startIdx; tokenIdx < tokens.length; tokenIdx++) {
        const token = tokens[tokenIdx];
        const separatorIdx = token.indexOf("=");
        if (separatorIdx <= 0 || separatorIdx === token.length - 1 || token.indexOf("=", separatorIdx + 1) >= 0) {
            addDiagnostic(diagnostics, "error", "product.setting.token", `Invalid setting token: ${token}`, line, documentPath);
            valid = false;
            continue;
        }
        const settingName = token.slice(0, separatorIdx);
        if (!definitions.has(settingName)) {
            addDiagnostic(diagnostics, "error", "product.setting.unknown", `Unknown setting: ${settingName}`, line, documentPath);
            valid = false;
            continue;
        }
        const existing = target.get(settingName);
        if (existing) {
            addDiagnostic(diagnostics, "error", "product.setting.duplicate", `${settingName} is duplicated; first defined at line ${existing.line}`, line, documentPath);
            valid = false;
            continue;
        }
        target.set(settingName, { line, value: token.slice(separatorIdx + 1) });
    }
    return valid;
}

export function parseProductConfig(source: string, documentPath?: string, settingsSchema?: SettingsSchema): LosslessDocument<ProductConfigSemantic> {
    const lines = splitSourceLines(source);
    const diagnostics: Diagnostic[] = [];
    const records: ProductConfigRecord[] = [];
    let version = "unversioned";
    let firstContentLine: number | undefined;
    const productSettingOverrides = new Map<string, SettingOverrideSource>();
    let productSettingOverridesValid = true;
    const surfaceSettingOverrides = new Map<ProductConfigRecord, SettingOverrideSet>();

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
        if (line.tokens[0] === "Settings") kind = "settings";
        else if (propertyValue(properties, "SurfaceType")) kind = "surface-type";
        else if (propertyValue(properties, "PageName")) kind = "page";
        else if (propertyValue(properties, "Broadcaster")) kind = "broadcaster";
        else if (propertyValue(properties, "Listener")) kind = "listener";
        else if (propertyValue(properties, "Surface")) kind = "surface-assignment";
        records.push({ kind, line: line.lineNumber, properties });
        line.kind = kind === "unknown" ? "unknown" : "entry";
        if (kind === "unknown") addDiagnostic(diagnostics, "warning", "product.line.unknown", `Unknown product config line: ${text}`, line.lineNumber, documentPath);
        if (kind === "settings") {
            if (line.tokens.length === 1) {
                addDiagnostic(diagnostics, "error", "product.setting.empty", "Settings line requires at least one setting", line.lineNumber, documentPath);
                productSettingOverridesValid = false;
            } else if (settingsSchema) productSettingOverridesValid = addSettingTokens(line.tokens, 1, line.lineNumber, productSettingOverrides, settingsSchema, diagnostics, documentPath) && productSettingOverridesValid;
        }
        if (settingsSchema && kind !== "settings" && kind !== "surface-assignment") {
            const definitions = new Set(settingsSchema.settings.map((definition) => definition.name));
            if (line.tokens.some((token) => definitions.has(token.slice(0, token.indexOf("="))))) addDiagnostic(diagnostics, "error", "product.setting.location", "Setting overrides are allowed only on Settings and Surface lines", line.lineNumber, documentPath);
        }

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
            if (settingsSchema) {
                const baseProperties = new Set(["Surface", "SurfaceFolder", "ZoneFolder", "FXZoneFolder", "StartChannel"]);
                const settingTokens = line.tokens.filter((token) => !baseProperties.has(token.slice(0, token.indexOf("="))));
                const overrides = new Map<string, SettingOverrideSource>();
                const overridesValid = addSettingTokens(settingTokens, 0, line.lineNumber, overrides, settingsSchema, diagnostics, documentPath);
                const record = records[records.length - 1];
                surfaceSettingOverrides.set(record, { overrides, valid: overridesValid });
                const baseTokenCount = line.tokens.length - settingTokens.length;
                if (baseTokenCount !== 5) addDiagnostic(diagnostics, "error", "product.surface.properties", "Surface assignment must contain exactly five base properties", line.lineNumber, documentPath);
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
    if (settingsSchema) {
        const compiledDefaults = new Map(settingsSchema.settings.map((definition) => [definition.name, String(definition.defaultValue)]));
        const productSettings = resolveSettings(compiledDefaults, { overrides: productSettingOverrides, valid: productSettingOverridesValid }, "Product", settingsSchema, diagnostics, documentPath);
        for (const overrideSet of surfaceSettingOverrides.values()) resolveSettings(productSettings, overrideSet, "Surface", settingsSchema, diagnostics, documentPath);
    }
    return { diagnostics, format: "product-config", lines, path: documentPath, semantic: { records }, source, version };
}
