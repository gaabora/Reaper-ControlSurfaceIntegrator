import { readFile } from "node:fs/promises";

export type SettingType = "enum" | "integer";

export interface SettingDefinition {
    category: string;
    defaultValue: string | number;
    enumValues?: string[];
    greaterThan?: string;
    max?: number;
    min?: number;
    name: string;
    scopes: string[];
    type: SettingType;
    unit?: "Milliseconds";
}

export interface SettingsSchema {
    settings: SettingDefinition[];
    version: number;
}

const SETTING_KEYS = new Set(["Category", "Default", "GreaterThan", "Max", "Min", "Scopes", "Setting", "Type", "Unit", "Values"]);

function parseProperties(line: string, lineNumber: number): Map<string, string> {
    const properties = new Map<string, string>();
    for (const token of line.split(/\s+/)) {
        const separatorIdx = token.indexOf("=");
        if (separatorIdx <= 0 || separatorIdx === token.length - 1) throw new Error(`Invalid settings schema token at line ${lineNumber}: ${token}`);
        const key = token.slice(0, separatorIdx);
        if (properties.has(key)) throw new Error(`Duplicate settings schema property at line ${lineNumber}: ${key}`);
        properties.set(key, token.slice(separatorIdx + 1));
    }
    return properties;
}

function requiredProperty(properties: Map<string, string>, key: string, lineNumber: number): string {
    const value = properties.get(key);
    if (!value) throw new Error(`Settings schema line ${lineNumber} requires ${key}`);
    return value;
}

function parseInteger(value: string, label: string, lineNumber: number): number {
    if (!/^\d+$/.test(value)) throw new Error(`Settings schema ${label} must be a non-negative integer at line ${lineNumber}`);
    const parsedValue = Number(value);
    if (!Number.isSafeInteger(parsedValue)) throw new Error(`Settings schema ${label} is outside the supported integer range at line ${lineNumber}`);
    return parsedValue;
}

function splitUniqueValues(value: string, label: string, lineNumber: number): string[] {
    const values = value.split(",");
    if (values.some((entry) => !entry)) throw new Error(`Settings schema ${label} contains an empty value at line ${lineNumber}`);
    if (new Set(values).size !== values.length) throw new Error(`Settings schema ${label} contains a duplicate value at line ${lineNumber}`);
    return values;
}

export function parseSettingsSchema(source: string): SettingsSchema {
    const settings: SettingDefinition[] = [];
    const settingsByName = new Map<string, SettingDefinition>();
    let version: number | undefined;
    let sawSetting = false;

    for (const [lineIdx, rawLine] of source.split(/\r?\n/).entries()) {
        const lineNumber = lineIdx + 1;
        const line = rawLine.trim();
        if (!line || line.startsWith("#")) continue;
        const properties = parseProperties(line, lineNumber);
        if (properties.has("Version")) {
            if (properties.size !== 1) throw new Error(`Settings schema Version must be the only property at line ${lineNumber}`);
            if (version !== undefined) throw new Error("Settings schema Version is duplicated");
            if (sawSetting) throw new Error("Settings schema Version must appear before settings");
            version = parseInteger(requiredProperty(properties, "Version", lineNumber), "Version", lineNumber);
            if (version !== 1) throw new Error(`Unsupported settings schema version: ${version}`);
            continue;
        }
        sawSetting = true;
        for (const key of properties.keys()) if (!SETTING_KEYS.has(key)) throw new Error(`Unknown settings schema property at line ${lineNumber}: ${key}`);
        const name = requiredProperty(properties, "Setting", lineNumber);
        if (!/^[A-Z][A-Za-z0-9]*$/.test(name)) throw new Error(`Invalid setting name at line ${lineNumber}: ${name}`);
        if (settingsByName.has(name)) throw new Error(`Duplicate setting: ${name}`);
        const sourceType = requiredProperty(properties, "Type", lineNumber);
        if (sourceType !== "Enum" && sourceType !== "Integer") throw new Error(`Unsupported setting type at line ${lineNumber}: ${sourceType}`);
        const scopes = splitUniqueValues(requiredProperty(properties, "Scopes", lineNumber), "Scopes", lineNumber);
        if (scopes.some((scope) => !/^[A-Z][A-Za-z0-9]*$/.test(scope))) throw new Error(`Invalid setting scope at line ${lineNumber}: ${scopes.join(",")}`);
        const category = requiredProperty(properties, "Category", lineNumber);
        if (!/^[A-Z][A-Za-z0-9]*$/.test(category)) throw new Error(`Invalid setting category at line ${lineNumber}: ${category}`);
        const defaultSource = requiredProperty(properties, "Default", lineNumber);
        let definition: SettingDefinition;
        if (sourceType === "Enum") {
            if (properties.has("Min") || properties.has("Max") || properties.has("Unit") || properties.has("GreaterThan")) throw new Error(`Enum setting has integer-only properties at line ${lineNumber}: ${name}`);
            const enumValues = splitUniqueValues(requiredProperty(properties, "Values", lineNumber), "Values", lineNumber);
            if (enumValues.some((value) => !/^[A-Z][A-Za-z0-9]*$/.test(value))) throw new Error(`Setting Values contains an invalid enum value at line ${lineNumber}: ${name}`);
            if (!enumValues.includes(defaultSource)) throw new Error(`Setting default is not in Values at line ${lineNumber}: ${name}`);
            definition = { category, defaultValue: defaultSource, enumValues, name, scopes, type: "enum" };
        } else {
            if (properties.has("Values")) throw new Error(`Integer setting has Values at line ${lineNumber}: ${name}`);
            const min = parseInteger(requiredProperty(properties, "Min", lineNumber), "Min", lineNumber);
            const max = parseInteger(requiredProperty(properties, "Max", lineNumber), "Max", lineNumber);
            const defaultValue = parseInteger(defaultSource, "Default", lineNumber);
            if (min > max || defaultValue < min || defaultValue > max) throw new Error(`Setting integer range is invalid at line ${lineNumber}: ${name}`);
            const unit = requiredProperty(properties, "Unit", lineNumber);
            if (unit !== "Milliseconds") throw new Error(`Unsupported setting unit at line ${lineNumber}: ${unit}`);
            definition = { category, defaultValue, greaterThan: properties.get("GreaterThan"), max, min, name, scopes, type: "integer", unit };
        }
        settings.push(definition);
        settingsByName.set(name, definition);
    }

    if (version === undefined) throw new Error("Settings schema has no Version");
    if (settings.length === 0) throw new Error("Settings schema has no settings");
    for (const setting of settings) {
        if (!setting.greaterThan) continue;
        const referencedSetting = settingsByName.get(setting.greaterThan);
        if (!referencedSetting) throw new Error(`Setting ${setting.name} references unknown GreaterThan setting: ${setting.greaterThan}`);
        if (setting.type !== "integer" || referencedSetting.type !== "integer") throw new Error(`Setting ${setting.name} has a non-integer GreaterThan relationship`);
        if ((setting.defaultValue as number) <= (referencedSetting.defaultValue as number)) throw new Error(`Setting ${setting.name} default must be greater than ${setting.greaterThan}`);
    }
    return { settings, version };
}

export async function loadSettingsSchema(schemaPath: string): Promise<SettingsSchema> {
    return parseSettingsSchema(await readFile(schemaPath, "utf8"));
}

export function settingDefinition(schema: SettingsSchema, name: string): SettingDefinition | undefined {
    return schema.settings.find((setting) => setting.name === name);
}
