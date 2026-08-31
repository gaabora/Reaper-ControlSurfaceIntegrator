import { addDiagnostic, type Diagnostic, type LosslessDocument, type SourceLine } from "./model.ts";
import type { SettingDefinition, SettingsSchema } from "./settings-schema.ts";
import { analysisText, isStableId, splitSourceLines } from "./text.ts";

export interface ProductConfigRecord {
    id?: string;
    kind: "device" | "link" | "page" | "settings" | "surface-assignment" | "unknown";
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

interface ProductToken {
    kind: "bare" | "quoted" | "symbol";
    line: number;
    text: string;
}

interface ProductBlock {
    children: ProductBlock[];
    id?: string;
    line: number;
    name: string;
    properties: Map<string, { line: number; list?: string[]; quoted: boolean; value: string }>;
}

const CONFIG_ID = /^[A-Za-z][A-Za-z0-9_]*$/;

function lexProductConfig(source: string, diagnostics: Diagnostic[], documentPath?: string): ProductToken[] {
    const tokens: ProductToken[] = [];
    let cursor = source.startsWith("\uFEFF") ? 1 : 0;
    let line = 1;
    const symbols = new Set(["{", "}", "[", "]", "=", ","]);
    while (cursor < source.length) {
        const character = source[cursor];
        if (character === "\r" || character === "\n") {
            if (character === "\r" && source[cursor + 1] === "\n") cursor++;
            cursor++;
            line++;
            continue;
        }
        if (/\s/.test(character)) {
            cursor++;
            continue;
        }
        if (character === "/" && source[cursor + 1] === "/") {
            while (cursor < source.length && source[cursor] !== "\r" && source[cursor] !== "\n") cursor++;
            continue;
        }
        if (symbols.has(character)) {
            tokens.push({ kind: "symbol", line, text: character });
            cursor++;
            continue;
        }
        if (character === '"') {
            const tokenLine = line;
            let value = "";
            cursor++;
            while (cursor < source.length && source[cursor] !== '"' && source[cursor] !== "\r" && source[cursor] !== "\n") value += source[cursor++];
            if (source[cursor] === '"') cursor++;
            else addDiagnostic(diagnostics, "error", "product.string.unclosed", "Quoted value is not closed", tokenLine, documentPath);
            tokens.push({ kind: "quoted", line: tokenLine, text: value });
            continue;
        }
        const start = cursor;
        while (cursor < source.length && !/\s/.test(source[cursor]) && !symbols.has(source[cursor]) && !(source[cursor] === "/" && source[cursor + 1] === "/")) cursor++;
        tokens.push({ kind: "bare", line, text: source.slice(start, cursor) });
    }
    return tokens;
}

function markLine(lines: SourceLine[], lineNumber: number, kind: SourceLine["kind"]): void {
    const line = lines[lineNumber - 1];
    if (line) line.kind = kind;
}

function parseProductBlocks(tokens: ProductToken[], lines: SourceLine[], diagnostics: Diagnostic[], documentPath?: string): ProductBlock[] {
    let cursor = 0;
    const parseBlock = (): ProductBlock | undefined => {
        const name = tokens[cursor++];
        if (!name || name.kind !== "bare") return undefined;
        let id: string | undefined;
        if (tokens[cursor]?.kind === "bare" && tokens[cursor + 1]?.text === "{") id = tokens[cursor++].text;
        if (tokens[cursor]?.text !== "{") {
            addDiagnostic(diagnostics, "error", "product.block.open", `${name.text} must start a block with {`, name.line, documentPath);
            return undefined;
        }
        cursor++;
        markLine(lines, name.line, "block-start");
        const block: ProductBlock = { children: [], id, line: name.line, name: name.text, properties: new Map() };
        while (cursor < tokens.length && tokens[cursor].text !== "}") {
            const current = tokens[cursor];
            if (current.kind !== "bare") {
                addDiagnostic(diagnostics, "error", "product.statement.invalid", `Unexpected token: ${current.text}`, current.line, documentPath);
                cursor++;
                continue;
            }
            if (tokens[cursor + 1]?.text !== "=") {
                const child = parseBlock();
                if (child) block.children.push(child);
                continue;
            }
            cursor += 2;
            const valueToken = tokens[cursor];
            let list: string[] | undefined;
            let value = "";
            let quoted = false;
            if (valueToken?.text === "[") {
                cursor++;
                list = [];
                while (cursor < tokens.length && tokens[cursor].text !== "]") {
                    if (tokens[cursor].text !== ",") {
                        if (tokens[cursor].kind === "quoted") addDiagnostic(diagnostics, "error", "product.property.unquoted", `${current.text} list items must be unquoted`, tokens[cursor].line, documentPath);
                        list.push(tokens[cursor].text);
                    }
                    cursor++;
                }
                if (tokens[cursor]?.text === "]") cursor++;
                else addDiagnostic(diagnostics, "error", "product.list.unclosed", `${current.text} list is not closed`, current.line, documentPath);
                value = list.join(",");
            } else if (valueToken && valueToken.text !== "}") {
                value = valueToken.text;
                quoted = valueToken.kind === "quoted";
                if (quoted) addDiagnostic(diagnostics, "error", "product.property.unquoted", `${current.text} requires an unquoted value`, current.line, documentPath);
                cursor++;
            } else addDiagnostic(diagnostics, "error", "product.property.value", `${current.text} requires a value`, current.line, documentPath);
            if (block.properties.has(current.text)) addDiagnostic(diagnostics, "error", "product.property.duplicate", `${current.text} is duplicated`, current.line, documentPath);
            else block.properties.set(current.text, { line: current.line, list, quoted, value });
            markLine(lines, current.line, "entry");
        }
        if (tokens[cursor]?.text === "}") {
            markLine(lines, tokens[cursor].line, "block-end");
            cursor++;
        } else addDiagnostic(diagnostics, "error", "product.block.unclosed", `${name.text} block is not closed`, name.line, documentPath);
        return block;
    };
    const blocks: ProductBlock[] = [];
    while (cursor < tokens.length) {
        const block = parseBlock();
        if (block) blocks.push(block);
        else cursor++;
    }
    return blocks;
}

function propertyMap(block: ProductBlock): Map<string, string> {
    return new Map([...block.properties].map(([name, property]) => [name, property.value]));
}

function validateId(kind: string, id: string | undefined, line: number, diagnostics: Diagnostic[], documentPath?: string): boolean {
    if (id && CONFIG_ID.test(id)) return true;
    addDiagnostic(diagnostics, "error", "product.id.invalid", `${kind} requires one unquoted ID that starts with a letter and contains only letters, digits, or _`, line, documentPath);
    return false;
}

function requireProperties(block: ProductBlock, names: string[], diagnostics: Diagnostic[], documentPath?: string): void {
    for (const name of names) if (!block.properties.has(name)) addDiagnostic(diagnostics, "error", "product.property.required", `${block.name} requires ${name}`, block.line, documentPath);
}

function rejectUnknownProperties(block: ProductBlock, allowed: Set<string>, diagnostics: Diagnostic[], documentPath?: string): void {
    for (const [name, property] of block.properties) if (!allowed.has(name)) addDiagnostic(diagnostics, "error", "product.property.unknown", `Unknown ${block.name} property: ${name}`, property.line, documentPath);
}

function validateBooleanProperty(block: ProductBlock, name: string, diagnostics: Diagnostic[], documentPath?: string): void {
    const property = block.properties.get(name);
    if (property && property.value !== "true" && property.value !== "false") addDiagnostic(diagnostics, "error", "product.property.boolean", `${name} must be true or false`, property.line, documentPath);
}

function validateIntegerProperty(block: ProductBlock, name: string, minimum: number, maximum: number, diagnostics: Diagnostic[], documentPath?: string): void {
    const property = block.properties.get(name);
    if (!property) return;
    const value = Number(property.value);
    if (!/^\d+$/.test(property.value) || !Number.isSafeInteger(value) || value < minimum || value > maximum) addDiagnostic(diagnostics, "error", "product.property.integer", `${name} must be an integer from ${minimum} through ${maximum}`, property.line, documentPath);
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

function readSettingBlock(block: ProductBlock, schema: SettingsSchema, diagnostics: Diagnostic[], documentPath?: string): SettingOverrideSet {
    const definitions = new Set(schema.settings.map((definition) => definition.name));
    const overrides = new Map<string, SettingOverrideSource>();
    let valid = true;
    if (block.id || block.children.length) valid = false;
    for (const [settingName, property] of block.properties) {
        if (!definitions.has(settingName)) {
            addDiagnostic(diagnostics, "error", "product.setting.unknown", `Unknown setting: ${settingName}`, property.line, documentPath);
            valid = false;
            continue;
        }
        if (property.list || property.quoted) {
            addDiagnostic(diagnostics, "error", "product.setting.value", `${settingName} requires one unquoted value`, property.line, documentPath);
            valid = false;
            continue;
        }
        const definition = schema.settings.find((candidate) => candidate.name === settingName)!;
        let value = property.value;
        if (definition.type === "boolean") value = value === "true" ? "1" : value === "false" ? "0" : value;
        overrides.set(settingName, { line: property.line, value });
    }
    return { overrides, valid };
}

export function parseProductConfig(source: string, documentPath?: string, settingsSchema?: SettingsSchema): LosslessDocument<ProductConfigSemantic> {
    const lines = splitSourceLines(source);
    const diagnostics: Diagnostic[] = [];
    const records: ProductConfigRecord[] = [];
    for (const line of lines) {
        const text = analysisText(line);
        if (!text) line.kind = "blank";
        else if (text.startsWith("//")) line.kind = "comment";
    }
    const blocks = parseProductBlocks(lexProductConfig(source, diagnostics, documentPath), lines, diagnostics, documentPath);
    const devices = new Map<string, ProductBlock>();
    const pages = new Map<string, ProductBlock>();
    let productOverrides: SettingOverrideSet = { overrides: new Map(), valid: true };
    let rootSettingsSeen = false;
    for (const block of blocks) {
        if (block.name === "Settings") {
            records.push({ kind: "settings", line: block.line, properties: propertyMap(block) });
            if (rootSettingsSeen) addDiagnostic(diagnostics, "error", "product.settings.duplicate", "Root Settings block is duplicated", block.line, documentPath);
            rootSettingsSeen = true;
            if (settingsSchema) productOverrides = readSettingBlock(block, settingsSchema, diagnostics, documentPath);
            continue;
        }
        if (block.name === "Device") {
            records.push({ id: block.id, kind: "device", line: block.line, properties: propertyMap(block) });
            if (validateId("Device", block.id, block.line, diagnostics, documentPath)) {
                const canonicalId = block.id!.toLowerCase();
                if (devices.has(canonicalId)) addDiagnostic(diagnostics, "error", "product.device.duplicate", `Device ID is duplicated case-insensitively: ${block.id}`, block.line, documentPath);
                else devices.set(canonicalId, block);
            }
            requireProperties(block, ["Type"], diagnostics, documentPath);
            const type = block.properties.get("Type")?.value;
            const allowed = new Set(type === "MIDI" ? ["Type", "Input", "Output", "RefreshRate", "MaxMessagesPerRun"] : ["Type", "Protocol", "ReceivePort", "TransmitPort", "Address", "MaxPacketsPerRun"]);
            rejectUnknownProperties(block, allowed, diagnostics, documentPath);
            if (type === "MIDI") requireProperties(block, ["Input", "Output"], diagnostics, documentPath);
            else if (type === "OSC") requireProperties(block, ["ReceivePort", "TransmitPort", "Address"], diagnostics, documentPath);
            else addDiagnostic(diagnostics, "error", "product.device.type", "Device Type must be MIDI or OSC", block.line, documentPath);
            if (type === "MIDI") {
                validateIntegerProperty(block, "Input", 0, 65535, diagnostics, documentPath);
                validateIntegerProperty(block, "Output", 0, 65535, diagnostics, documentPath);
                validateIntegerProperty(block, "RefreshRate", 1, 65535, diagnostics, documentPath);
                validateIntegerProperty(block, "MaxMessagesPerRun", 1, 1000000, diagnostics, documentPath);
            } else if (type === "OSC") {
                validateIntegerProperty(block, "ReceivePort", 1, 65535, diagnostics, documentPath);
                validateIntegerProperty(block, "TransmitPort", 1, 65535, diagnostics, documentPath);
                validateIntegerProperty(block, "MaxPacketsPerRun", 1, 1000000, diagnostics, documentPath);
                const protocol = block.properties.get("Protocol");
                if (protocol && protocol.value !== "Generic" && protocol.value !== "X32") addDiagnostic(diagnostics, "error", "product.device.protocol", "OSC Protocol must be Generic or X32", protocol.line, documentPath);
            }
            const settingsBlocks = block.children.filter((child) => child.name === "Settings");
            if (settingsBlocks.length > 1) addDiagnostic(diagnostics, "error", "product.device.settings.duplicate", "Device Settings block is duplicated", settingsBlocks[1].line, documentPath);
            for (const child of block.children) if (child.name !== "Settings") addDiagnostic(diagnostics, "error", "product.device.child", `Unknown Device child block: ${child.name}`, child.line, documentPath);
            continue;
        }
        if (block.name !== "Page") {
            records.push({ id: block.id, kind: "unknown", line: block.line, properties: propertyMap(block) });
            addDiagnostic(diagnostics, "error", "product.block.unknown", `Unknown product configuration block: ${block.name}`, block.line, documentPath);
            continue;
        }
        records.push({ id: block.id, kind: "page", line: block.line, properties: propertyMap(block) });
        if (validateId("Page", block.id, block.line, diagnostics, documentPath)) {
            const canonicalId = block.id!.toLowerCase();
            if (pages.has(canonicalId)) addDiagnostic(diagnostics, "error", "product.page.duplicate", `Page ID is duplicated case-insensitively: ${block.id}`, block.line, documentPath);
            else pages.set(canonicalId, block);
        }
        rejectUnknownProperties(block, new Set(["FollowMCP", "SyncPages", "ScrollLink", "ScrollSync"]), diagnostics, documentPath);
        for (const name of ["FollowMCP", "SyncPages", "ScrollLink", "ScrollSync"]) validateBooleanProperty(block, name, diagnostics, documentPath);
        const surfaceIds = new Set<string>();
        const linkBlocks: ProductBlock[] = [];
        const linkPairs = new Set<string>();
        for (const child of block.children) {
            if (child.name === "Surface") {
                records.push({ id: child.id, kind: "surface-assignment", line: child.line, properties: propertyMap(child) });
                if (validateId("Surface", child.id, child.line, diagnostics, documentPath)) {
                    const canonicalId = child.id!.toLowerCase();
                    if (surfaceIds.has(canonicalId)) addDiagnostic(diagnostics, "error", "product.surface.duplicate", `Surface ID is duplicated on Page ${block.id}: ${child.id}`, child.line, documentPath);
                    surfaceIds.add(canonicalId);
                }
                requireProperties(child, ["Device", "Template"], diagnostics, documentPath);
                rejectUnknownProperties(child, new Set(["Device", "Template", "MainProfile", "FXProfile", "StartChannel"]), diagnostics, documentPath);
                validateIntegerProperty(child, "StartChannel", 0, 1000000, diagnostics, documentPath);
                const template = child.properties.get("Template")?.value ?? "";
                if (!isStableId(template)) addDiagnostic(diagnostics, "error", "product.surface.template", "Template must contain a stable surface ID", child.line, documentPath);
                for (const name of ["MainProfile", "FXProfile"]) {
                    const profile = child.properties.get(name)?.value;
                    if (profile && !isStableId(profile)) addDiagnostic(diagnostics, "error", "product.surface.profile", `${name} must contain a stable profile ID`, child.line, documentPath);
                }
            } else if (child.name === "Link") {
                records.push({ kind: "link", line: child.line, properties: propertyMap(child) });
                requireProperties(child, ["From", "To", "Share"], diagnostics, documentPath);
                rejectUnknownProperties(child, new Set(["From", "To", "Share"]), diagnostics, documentPath);
                const share = child.properties.get("Share");
                if (!share?.list?.length) addDiagnostic(diagnostics, "error", "product.link.share", "Link Share requires a non-empty list", share?.line ?? child.line, documentPath);
                else {
                    const categories = new Set<string>();
                    for (const category of share.list) {
                        if (categories.has(category)) addDiagnostic(diagnostics, "error", "product.link.category.duplicate", `Link Share category is duplicated: ${category}`, share.line, documentPath);
                        else if (!["Home", "Modifiers", "FXMenu", "SelectedTrackFX", "SelectedTrackSends", "SelectedTrackReceives"].includes(category)) addDiagnostic(diagnostics, "error", "product.link.category", `Unknown Link Share category: ${category}`, share.line, documentPath);
                        categories.add(category);
                    }
                }
                linkBlocks.push(child);
            } else addDiagnostic(diagnostics, "error", "product.page.child", `Unknown Page child block: ${child.name}`, child.line, documentPath);
        }
        for (const link of linkBlocks) {
            const from = link.properties.get("From")?.value.toLowerCase();
            const to = link.properties.get("To")?.value.toLowerCase();
            if (!from || !to) continue;
            if (from === to || !surfaceIds.has(from) || !surfaceIds.has(to)) addDiagnostic(diagnostics, "error", "product.link.surface", `Link requires two distinct existing Surface IDs on Page ${block.id}`, link.line, documentPath);
            const pair = `${from}\n${to}`;
            if (linkPairs.has(pair)) addDiagnostic(diagnostics, "error", "product.link.duplicate", `Link is duplicated on Page ${block.id}`, link.line, documentPath);
            linkPairs.add(pair);
        }
        if (!block.children.some((child) => child.name === "Surface")) addDiagnostic(diagnostics, "error", "product.page.surface.missing", "Page requires at least one Surface block", block.line, documentPath);
    }
    if (!devices.size) addDiagnostic(diagnostics, "error", "product.device.missing", "Product configuration requires at least one Device block", 1, documentPath);
    if (!pages.size) addDiagnostic(diagnostics, "error", "product.page.missing", "Product configuration requires at least one Page block", 1, documentPath);
    for (const record of records.filter((candidate) => candidate.kind === "surface-assignment")) {
        const deviceId = record.properties.get("Device");
        if (deviceId && !devices.has(deviceId.toLowerCase())) addDiagnostic(diagnostics, "error", "product.surface.device.missing", `Surface references unknown Device: ${deviceId}`, record.line, documentPath);
    }
    if (settingsSchema) {
        const compiledDefaults = new Map(settingsSchema.settings.map((definition) => [definition.name, String(definition.defaultValue)]));
        const productSettings = resolveSettings(compiledDefaults, productOverrides, "Product", settingsSchema, diagnostics, documentPath);
        for (const block of devices.values()) {
            const settings = block.children.find((child) => child.name === "Settings");
            if (settings) resolveSettings(productSettings, readSettingBlock(settings, settingsSchema, diagnostics, documentPath), "Device", settingsSchema, diagnostics, documentPath);
        }
    }
    return { diagnostics, format: "product-config", lines, path: documentPath, semantic: { records }, source, version: "current" };
}
