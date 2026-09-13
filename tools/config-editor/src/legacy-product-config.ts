import { addDiagnostic, type Diagnostic } from "./model.ts";
import { initializeLine, parseProperties, splitSourceLines } from "./text.ts";

export interface LegacyProductConfigConversion {
    diagnostics: Diagnostic[];
    source: string;
}

function booleanValue(value: string | undefined): string {
    return value?.toLowerCase() === "yes" ? "true" : "false";
}

function configId(value: string): string {
    const normalized = value.replace(/[^A-Za-z0-9_]+/g, "_").replace(/^_+|_+$/g, "");
    return /^[A-Za-z]/.test(normalized) ? normalized : `item_${normalized || "missing"}`;
}

function resourceId(value: string): string {
    return value.toLowerCase().replace(/[^a-z0-9_-]+/g, "-").replace(/^[-_]+|[-_]+$/g, "") || "missing";
}

function propertiesOf(tokens: string[]): Map<string, string> {
    return new Map([...parseProperties(tokens)].map(([name, value]) => [name, value]));
}

function property(properties: Map<string, string>, name: string, fallback = ""): string {
    return properties.get(name) ?? fallback;
}

function deviceLines(properties: Map<string, string>, diagnostics: Diagnostic[], line: number, documentPath?: string): string[] {
    const originalId = property(properties, "SurfaceName");
    const id = configId(originalId);
    const legacyType = property(properties, "SurfaceType");
    if (!originalId) addDiagnostic(diagnostics, "error", "legacy.config.device.id", "SurfaceType requires SurfaceName", line, documentPath);
    if (!legacyType) addDiagnostic(diagnostics, "error", "legacy.config.device.type", "SurfaceType is missing", line, documentPath);
    const output = [`Device ${id} {`];
    if (legacyType === "MIDI") {
        output.push("  Type=MIDI", `  Input=${property(properties, "MidiInput", "0")}`, `  Output=${property(properties, "MidiOutput", "0")}`, `  RefreshRate=${property(properties, "MIDISurfaceRefreshRate", "15")}`, `  MaxMessagesPerRun=${property(properties, "MaxMIDIMesssagesPerRun", "200")}`);
    } else if (legacyType === "OSC" || legacyType === "OSCX32") {
        output.push("  Type=OSC");
        if (legacyType === "OSCX32") output.push("  Protocol=X32");
        output.push(`  ReceivePort=${property(properties, "ReceiveOnPort", "8000")}`, `  TransmitPort=${property(properties, "TransmitToPort", "9000")}`, `  Address=${property(properties, "TransmitToIPAddress", "127.0.0.1")}`, `  MaxPacketsPerRun=${property(properties, "MaxPacketsPerRun", "200")}`);
    } else addDiagnostic(diagnostics, "error", "legacy.config.device.type", `Unsupported legacy SurfaceType: ${legacyType || "(empty)"}`, line, documentPath);
    output.push("}");
    return output;
}

function surfaceLines(properties: Map<string, string>): string[] {
    const originalSurfaceId = property(properties, "Surface");
    const surfaceId = configId(originalSurfaceId);
    const template = resourceId(property(properties, "SurfaceFolder", surfaceId));
    const mainProfile = resourceId(property(properties, "ZoneFolder", template));
    const fxProfile = resourceId(property(properties, "FXZoneFolder", mainProfile));
    return [`  Surface ${surfaceId} {`, `    Device=${surfaceId}`, `    Template=${template}`, `    MainProfile=${mainProfile}`, `    FXProfile=${fxProfile}`, `    StartChannel=${property(properties, "StartChannel", "0")}`, "  }"];
}

function linkLines(broadcaster: string, properties: Map<string, string>): string[] {
    const shares: string[] = [];
    const shareProperties = [["GoHome", "Home"], ["Modifiers", "Modifiers"], ["FXMenu", "FXMenu"], ["SelectedTrackSends", "SelectedTrackSends"], ["SelectedTrackReceives", "SelectedTrackReceives"], ["SelectedTrackFX", "SelectedTrackFX"]];
    for (const [legacyName, currentName] of shareProperties) if (property(properties, legacyName).toLowerCase() === "yes") shares.push(currentName);
    return ["", "  Link {", `    From=${configId(broadcaster)}`, `    To=${configId(property(properties, "Listener"))}`, `    Share=[${shares.join(", ")}]`, "  }"];
}

export function convertLegacyProductConfig(source: string, documentPath?: string): LegacyProductConfigConversion {
    const diagnostics: Diagnostic[] = [];
    const output: string[] = [];
    let pageOpen = false;
    let broadcaster = "";
    for (const line of splitSourceLines(source)) {
        const text = initializeLine(line);
        if (!text || line.kind === "comment") continue;
        const properties = propertiesOf(line.tokens);
        const firstName = line.tokens[0]?.split("=")[0] ?? "";
        if (firstName === "Version") continue;
        if (firstName === "Settings") {
            output.push("Settings {");
            for (const [name, value] of properties) if (name !== "Settings") output.push(`  ${name}=${value}`);
            output.push("}", "");
            continue;
        }
        if (firstName === "SurfaceType") {
            if (pageOpen) {
                output.push("}", "");
                pageOpen = false;
            }
            output.push(...deviceLines(properties, diagnostics, line.lineNumber, documentPath), "");
            continue;
        }
        if (firstName === "PageName") {
            if (pageOpen) output.push("}", "");
            output.push(`Page ${configId(property(properties, "PageName", "Home"))} {`, `  FollowMCP=${booleanValue(property(properties, "PageFollowsMCP"))}`, `  SyncPages=${booleanValue(property(properties, "SynchPages"))}`, `  ScrollLink=${booleanValue(property(properties, "ScrollLink"))}`, `  ScrollSync=${booleanValue(property(properties, "ScrollSynch"))}`, "");
            pageOpen = true;
            broadcaster = "";
            continue;
        }
        if (firstName === "Surface") {
            if (!pageOpen) addDiagnostic(diagnostics, "error", "legacy.config.surface.page", "Surface must follow a PageName line", line.lineNumber, documentPath);
            output.push(...surfaceLines(properties), "");
            continue;
        }
        if (firstName === "Broadcaster") {
            broadcaster = property(properties, "Broadcaster");
            continue;
        }
        if (firstName === "Listener") {
            if (!broadcaster) addDiagnostic(diagnostics, "error", "legacy.config.listener.broadcaster", "Listener must follow Broadcaster", line.lineNumber, documentPath);
            output.push(...linkLines(broadcaster, properties));
            continue;
        }
        addDiagnostic(diagnostics, "warning", "legacy.config.line.unknown", `Unsupported CSI.ini line was not imported: ${text}`, line.lineNumber, documentPath);
    }
    if (pageOpen) output.push("}");
    return { diagnostics, source: output.join("\n").replace(/\n{3,}/g, "\n\n") + "\n" };
}
