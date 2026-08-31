import { readFile } from "node:fs/promises";

export type SurfaceIoDirection = "Feedback" | "Input";
export type SurfaceIoProtocol = "MIDI" | "OSC";

export interface SurfaceIoPrimitiveDefinition {
    capabilities: string[];
    direction: SurfaceIoDirection;
    name: string;
}

export interface SurfaceIoRepresentationDefinition {
    direction: SurfaceIoDirection;
    encoding: string;
    nested: string[];
    optional: string[];
    primitive: string;
    protocol: SurfaceIoProtocol;
    required: string[];
}

export interface SurfaceIoSchemaRecord {
    kind: string;
    line: number;
    properties: ReadonlyMap<string, string>;
}

export interface SurfaceIoSchema {
    primitives: SurfaceIoPrimitiveDefinition[];
    records: SurfaceIoSchemaRecord[];
    representations: SurfaceIoRepresentationDefinition[];
    version: number;
}

const RECORD_KEYS = ["Primitive", "CapabilityWhen", "NestedBlock", "Profile", "ProfileLine", "SurfaceBlock", "SurfaceLine", "Representation"];

function parseProperties(line: string, lineNumber: number): Map<string, string> {
    const properties = new Map<string, string>();
    for (const token of line.split(/\s+/)) {
        const separatorIdx = token.indexOf("=");
        if (separatorIdx <= 0 || separatorIdx === token.length - 1) throw new Error(`Invalid Surface I/O schema token at line ${lineNumber}: ${token}`);
        const key = token.slice(0, separatorIdx);
        if (properties.has(key)) throw new Error(`Duplicate Surface I/O schema property at line ${lineNumber}: ${key}`);
        properties.set(key, token.slice(separatorIdx + 1));
    }
    return properties;
}

function requiredProperty(properties: ReadonlyMap<string, string>, key: string, lineNumber: number): string {
    const value = properties.get(key);
    if (!value) throw new Error(`Surface I/O schema line ${lineNumber} requires ${key}`);
    return value;
}

function parseList(value: string): string[] {
    if (value === "None") return [];
    const entries = value.split(",");
    if (entries.some((entry) => !entry)) throw new Error(`Surface I/O schema list contains an empty value: ${value}`);
    if (new Set(entries).size !== entries.length) throw new Error(`Surface I/O schema list contains a duplicate value: ${value}`);
    return entries;
}

function parseDirection(value: string, lineNumber: number): SurfaceIoDirection {
    if (value !== "Input" && value !== "Feedback") throw new Error(`Unsupported Surface I/O direction at line ${lineNumber}: ${value}`);
    return value;
}

function parseProtocol(value: string, lineNumber: number): SurfaceIoProtocol {
    if (value !== "MIDI" && value !== "OSC") throw new Error(`Unsupported Surface I/O protocol at line ${lineNumber}: ${value}`);
    return value;
}

function representationKey(definition: SurfaceIoRepresentationDefinition): string {
    return `${definition.direction}:${definition.primitive}:${definition.protocol}:${definition.encoding}`;
}

export function parseSurfaceIoSchema(source: string): SurfaceIoSchema {
    const primitives: SurfaceIoPrimitiveDefinition[] = [];
    const records: SurfaceIoSchemaRecord[] = [];
    const representations: SurfaceIoRepresentationDefinition[] = [];
    const primitiveKeys = new Set<string>();
    const representationKeys = new Set<string>();
    let version: number | undefined;

    for (const [lineIdx, rawLine] of source.split(/\r?\n/).entries()) {
        const lineNumber = lineIdx + 1;
        const line = rawLine.trim();
        if (!line || line.startsWith("#")) continue;
        const properties = parseProperties(line, lineNumber);
        if (properties.has("Version")) {
            if (properties.size !== 1 || version !== undefined || records.length) throw new Error(`Surface I/O schema Version must occur once before records at line ${lineNumber}`);
            const sourceVersion = requiredProperty(properties, "Version", lineNumber);
            if (!/^\d+$/.test(sourceVersion)) throw new Error(`Surface I/O schema Version must be an integer at line ${lineNumber}`);
            version = Number(sourceVersion);
            if (version !== 1) throw new Error(`Unsupported Surface I/O schema version: ${version}`);
            continue;
        }

        const kind = properties.keys().next().value;
        if (!kind || !RECORD_KEYS.includes(kind)) throw new Error(`Surface I/O schema line ${lineNumber} requires a recognized record type as its first property`);
        records.push({ kind, line: lineNumber, properties });
        if (kind === "Primitive") {
            const definition: SurfaceIoPrimitiveDefinition = {
                capabilities: parseList(requiredProperty(properties, "Capabilities", lineNumber)),
                direction: parseDirection(requiredProperty(properties, "Direction", lineNumber), lineNumber),
                name: requiredProperty(properties, "Primitive", lineNumber),
            };
            const key = `${definition.direction}:${definition.name}`;
            if (primitiveKeys.has(key)) throw new Error(`Duplicate Surface I/O primitive at line ${lineNumber}: ${key}`);
            primitiveKeys.add(key);
            primitives.push(definition);
        } else if (kind === "Representation") {
            const definition: SurfaceIoRepresentationDefinition = {
                direction: parseDirection(requiredProperty(properties, "Direction", lineNumber), lineNumber),
                encoding: requiredProperty(properties, "Encoding", lineNumber),
                nested: parseList(requiredProperty(properties, "Nested", lineNumber)),
                optional: parseList(requiredProperty(properties, "Optional", lineNumber)),
                primitive: requiredProperty(properties, "Representation", lineNumber),
                protocol: parseProtocol(requiredProperty(properties, "Protocol", lineNumber), lineNumber),
                required: parseList(requiredProperty(properties, "Required", lineNumber)),
            };
            const key = representationKey(definition);
            if (representationKeys.has(key)) throw new Error(`Duplicate Surface I/O representation at line ${lineNumber}: ${key}`);
            representationKeys.add(key);
            representations.push(definition);
        }
    }

    if (version === undefined) throw new Error("Surface I/O schema has no Version");
    if (!primitives.length) throw new Error("Surface I/O schema has no primitives");
    if (!representations.length) throw new Error("Surface I/O schema has no representations");
    for (const representation of representations) {
        const primitiveKey = `${representation.direction}:${representation.primitive}`;
        if (!primitiveKeys.has(primitiveKey)) throw new Error(`Surface I/O representation references unknown primitive: ${primitiveKey}`);
    }
    return { primitives, records, representations, version };
}

export async function loadSurfaceIoSchema(schemaPath: string): Promise<SurfaceIoSchema> {
    return parseSurfaceIoSchema(await readFile(schemaPath, "utf8"));
}

export function findSurfaceIoRepresentation(schema: SurfaceIoSchema, direction: SurfaceIoDirection, primitive: string, protocol: SurfaceIoProtocol, encoding: string): SurfaceIoRepresentationDefinition | undefined {
    return schema.representations.find((definition) => definition.direction === direction && definition.primitive === primitive && definition.protocol === protocol && definition.encoding === encoding);
}
