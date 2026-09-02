import { addDiagnostic, type Diagnostic } from "./model.ts";
import { analysisText, splitSourceLines, tokenizeLine } from "./text.ts";
import { parseSurface, type OskLayoutCell, type SurfaceWidget } from "./surface.ts";

export interface LegacySurfaceConversion {
    diagnostics: Diagnostic[];
    source: string;
}

export interface LegacySurfaceProcessorTarget {
    direction: "Feedback" | "Input";
    encoding: string;
    primitive: string;
    protocol: "MIDI" | "OSC";
}

export type LegacyMcuMeterMode = "IconV1M" | "MCU" | "SSLNucleus2" | "XTouch";

type LegacySurfaceProcessorConversionKind = "anyPress" | "bar" | "encoder" | "fader7" | "fader7Feedback" | "fader14" | "fader14Feedback" | "faderportMeter" | "faderportRgb" | "faderportScribble" | "faderportTwoStateRgb" | "mcuDisplay" | "mcuMeter" | "midiPalette" | "oscControl" | "oscFeedback" | "press" | "ring" | "sce24Ring" | "sce24State" | "sce24Text" | "state" | "touch";

interface LegacySurfaceProcessorConversionDefinition {
    kind: LegacySurfaceProcessorConversionKind;
    targets: LegacySurfaceProcessorTarget[];
}

const LEGACY_SURFACE_PROCESSOR_CONVERSIONS = new Map<string, LegacySurfaceProcessorConversionDefinition>([
    ["anypress", { kind: "anyPress", targets: [{ direction: "Input", encoding: "MIDIPrefix", primitive: "Press", protocol: "MIDI" }] }],
    ["control", { kind: "oscControl", targets: [{ direction: "Input", encoding: "OSCFloat", primitive: "Value", protocol: "OSC" }] }],
    ["encoder", { kind: "encoder", targets: [{ direction: "Input", encoding: "MIDI7", primitive: "Encoder", protocol: "MIDI" }] }],
    ["fader7bit", { kind: "fader7", targets: [{ direction: "Input", encoding: "MIDI7", primitive: "Value", protocol: "MIDI" }] }],
    ["fader14bit", { kind: "fader14", targets: [{ direction: "Input", encoding: "MIDI14", primitive: "Value", protocol: "MIDI" }] }],
    ["fb_encoder", { kind: "ring", targets: [{ direction: "Feedback", encoding: "MIDI7", primitive: "Ring", protocol: "MIDI" }] }],
    ["fb_fader7bit", { kind: "fader7Feedback", targets: [{ direction: "Feedback", encoding: "MIDI7", primitive: "Value", protocol: "MIDI" }] }],
    ["fb_fader14bit", { kind: "fader14Feedback", targets: [{ direction: "Feedback", encoding: "MIDI14", primitive: "Value", protocol: "MIDI" }] }],
    ["fb_faderportrgb", { kind: "faderportRgb", targets: [{ direction: "Feedback", encoding: "MIDIRGB", primitive: "Color", protocol: "MIDI" }] }],
    ["fb_fp8scribbleline1", { kind: "faderportScribble", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_fp8scribbleline2", { kind: "faderportScribble", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_fp8scribbleline3", { kind: "faderportScribble", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_fp8scribbleline4", { kind: "faderportScribble", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_fp16scribbleline1", { kind: "faderportScribble", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_fp16scribbleline2", { kind: "faderportScribble", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_fp16scribbleline3", { kind: "faderportScribble", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_fp16scribbleline4", { kind: "faderportScribble", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_faderporttwostatergb", { kind: "faderportTwoStateRgb", targets: [{ direction: "Feedback", encoding: "MIDIRGB", primitive: "Color", protocol: "MIDI" }] }],
    ["fb_faderportvaluebar", { kind: "bar", targets: [{ direction: "Feedback", encoding: "MIDI7", primitive: "Bar", protocol: "MIDI" }] }],
    ["fb_fpvumeter", { kind: "faderportMeter", targets: [{ direction: "Feedback", encoding: "MIDI7", primitive: "Meter", protocol: "MIDI" }] }],
    ["fb_mft_rgb", { kind: "midiPalette", targets: [{ direction: "Feedback", encoding: "MIDIPalette", primitive: "Color", protocol: "MIDI" }] }],
    ["fb_c4displaylower", { kind: "mcuDisplay", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_c4displayupper", { kind: "mcuDisplay", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_mcudisplaylower", { kind: "mcuDisplay", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_mcudisplayupper", { kind: "mcuDisplay", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_mcuvumeter", { kind: "mcuMeter", targets: [{ direction: "Feedback", encoding: "MIDI7", primitive: "Meter", protocol: "MIDI" }] }],
    ["fb_mcuxtdisplaylower", { kind: "mcuDisplay", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_mcuxtdisplayupper", { kind: "mcuDisplay", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_mcuxtvumeter", { kind: "mcuMeter", targets: [{ direction: "Feedback", encoding: "MIDI7", primitive: "Meter", protocol: "MIDI" }] }],
    ["fb_sce24encoder", { kind: "sce24Ring", targets: [{ direction: "Feedback", encoding: "MIDI7", primitive: "Ring", protocol: "MIDI" }] }],
    ["fb_sce24ledbutton", { kind: "sce24State", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "State", protocol: "MIDI" }] }],
    ["fb_sce24encodertext", { kind: "sce24Text", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_sce24oledbutton", { kind: "sce24Text", targets: [{ direction: "Feedback", encoding: "MIDISysEx", primitive: "Text", protocol: "MIDI" }] }],
    ["fb_processor", {
        kind: "oscFeedback",
        targets: [
            { direction: "Feedback", encoding: "OSCFloat", primitive: "Value", protocol: "OSC" },
            { direction: "Feedback", encoding: "OSCString", primitive: "Text", protocol: "OSC" },
            { direction: "Feedback", encoding: "OSCString", primitive: "Color", protocol: "OSC" },
        ],
    }],
    ["fb_twostate", { kind: "state", targets: [{ direction: "Feedback", encoding: "MIDIExact", primitive: "State", protocol: "MIDI" }] }],
    ["press", { kind: "press", targets: [{ direction: "Input", encoding: "MIDIExact", primitive: "Press", protocol: "MIDI" }] }],
    ["touch", { kind: "touch", targets: [{ direction: "Input", encoding: "MIDIExact", primitive: "Touch", protocol: "MIDI" }] }],
]);

export function legacySurfaceProcessorTargets(processor: string): LegacySurfaceProcessorTarget[] | undefined {
    return LEGACY_SURFACE_PROCESSOR_CONVERSIONS.get(processor.toLowerCase())?.targets;
}

function midiByte(value: string): string {
    const normalized = value.replace(/^0x/i, "");
    return `0x${normalized.padStart(2, "0").toUpperCase()}`;
}

function midiList(values: string[]): string {
    return `[ ${values.map(midiByte).join(", ")} ]`;
}

function numberList(values: string[]): string {
    return `[ ${values.join(", ")} ]`;
}

function propertyText(properties: Map<string, string>, ignored = new Set<string>()): string {
    const result: string[] = [];
    for (const [name, rawValue] of properties) {
        if (ignored.has(name.toLowerCase())) continue;
        let value = rawValue;
        if (name.toLowerCase() === "color") value = `#${rawValue.replace(/^#/, "").slice(0, 6).toUpperCase()}`;
        if (name.toLowerCase() === "label") value = JSON.stringify(rawValue);
        result.push(`${name}=${value}`);
    }
    return result.length ? ` ${result.join(" ")}` : "";
}

function legacyBlocks(source: string): Map<string, string[][]> {
    const result = new Map<string, string[][]>();
    let blockName = "";
    for (const line of splitSourceLines(source)) {
        const text = analysisText(line);
        const tokens = tokenizeLine(text);
        if (["StepSize", "AccelerationValues", "ColorCalibration"].includes(tokens[0] ?? "")) {
            blockName = tokens[0];
            result.set(blockName, []);
        } else if (tokens[0] === `${blockName}End`) blockName = "";
        else if (blockName && tokens.length) result.get(blockName)!.push(tokens);
    }
    return result;
}

function encoderProfiles(blocks: Map<string, string[][]>): string[] {
    const profiles = new Map<string, { decrease: string[]; delta?: string; increase: string[]; values: string[] }>();
    for (const tokens of blocks.get("StepSize") ?? []) profiles.set(tokens[0], { decrease: [], delta: tokens[1], increase: [], values: [] });
    for (const tokens of blocks.get("AccelerationValues") ?? []) {
        const profile = profiles.get(tokens[0]) ?? { decrease: [], increase: [], values: [] };
        if (tokens[1]?.toLowerCase() === "dec") profile.decrease = tokens.slice(2);
        else if (tokens[1]?.toLowerCase() === "inc") profile.increase = tokens.slice(2);
        else if (tokens[1]?.toLowerCase() === "val") profile.values = tokens.slice(2);
        profiles.set(tokens[0], profile);
    }
    const result: string[] = [];
    for (const [name, profile] of profiles) {
        result.push(`EncoderProfile ${name} {`);
        if (profile.delta) result.push(`  Delta=${profile.delta}`);
        if (profile.increase.length) result.push(`  Increase=${midiList(profile.increase)}`);
        if (profile.decrease.length) result.push(`  Decrease=${midiList(profile.decrease)}`);
        if (profile.values.length) result.push(`  AccelerationDeltas=${numberList(profile.values)}`);
        result.push("}", "");
    }
    return result;
}

function colorCalibration(blocks: Map<string, string[][]>): string[] {
    const entries = (blocks.get("ColorCalibration") ?? []).filter((tokens) => tokens[0]?.toLowerCase() !== "enabled");
    if (!entries.length) return [];
    return ["ColorCalibration {", ...entries.map((tokens) => `  ${tokens[0]}=${tokens[1]}`), "}", ""];
}

function ringProfiles(widgets: SurfaceWidget[]): string[] {
    const result: string[] = [];
    if (widgets.some((widget) => widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase() === "fb_encoder"))) {
        result.push("RingProfile RotaryRing {", "  Segments=11", "  Quantize=Floor", "  ValueOffset=1", "  Style Dot Code=0 Steps=11", "  Style BoostCut Code=1 Steps=11", "  Style Fill Code=2 Steps=11", "  Style Spread Code=3 Steps=6", "}", "");
    }
    if (widgets.some((widget) => widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase() === "fb_sce24encoder"))) {
        result.push("RingProfile SCE24Ring {", "  Segments=18", "  DefaultColor=#000000", "  Quantize=Floor", "  ValueOffset=65", "  Style Dot Code=0 Steps=15", "  Style BoostCut Code=1 Steps=15", "  Style Fill Code=2 Steps=15", "  Style Spread Code=3 Steps=8", "}", "");
    }
    return result;
}

function barProfiles(widgets: SurfaceWidget[]): string[] {
    if (!widgets.some((widget) => widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase() === "fb_faderportvaluebar"))) return [];
    return [
        "BarProfile StandardBar {",
        "  Default=Off",
        "  Style Normal Code=0",
        "  Style Bipolar Code=1",
        "  Style Fill Code=2",
        "  Style Spread Code=3",
        "  Style Off Code=4",
        "}",
        "",
    ];
}

function rgbHex(red: number, green: number, blue: number): string {
    return `#${[red, green, blue].map((channel) => channel.toString(16).padStart(2, "0").toUpperCase()).join("")}`;
}

function mftPaletteEntries(): Array<{ blue: number; green: number; red: number }> {
    const entries: Array<{ blue: number; green: number; red: number }> = [{ blue: 0, green: 0, red: 0 }];
    for (const green of [0, 21, 34, 46, 59, 68, 80, 93, 106, 119, 127, 140, 153, 165, 178, 191, 199, 212, 225, 238, 250]) entries.push({ blue: 255, green, red: 0 });
    for (const blue of [250, 237, 225, 212, 199, 191, 178, 165, 153, 140, 127, 119, 106, 93, 80, 67, 59, 46, 33, 21, 8, 0]) entries.push({ blue, green: 255, red: 0 });
    for (const red of [12, 25, 38, 51, 63, 72, 84, 97, 110, 123, 131, 144, 157, 170, 182, 191, 203, 216, 229, 242, 255]) entries.push({ blue: 0, green: 255, red });
    for (const green of [246, 233, 220, 208, 195, 187, 174, 161, 148, 135, 127, 114, 102, 89, 76, 63, 55, 42, 29, 16, 4]) entries.push({ blue: 0, green, red: 255 });
    for (const blue of [4, 16, 29, 42, 55, 63, 76, 89, 102, 114, 127, 135, 148, 161, 174, 186, 195, 208, 221, 233, 246, 255]) entries.push({ blue, green: 0, red: 255 });
    for (const red of [242, 229, 216, 204, 191, 182, 169, 157, 144, 131, 123, 110, 97, 85, 72, 63, 50, 38, 25]) entries.push({ blue: 255, green: 0, red });
    entries.push({ blue: 225, green: 240, red: 240 });
    return entries;
}

function paletteProfiles(widgets: SurfaceWidget[]): string[] {
    if (!widgets.some((widget) => widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase() === "fb_mft_rgb"))) return [];
    const entries = mftPaletteEntries();
    return ["ColorProfile Palette128 {", "  Match=Nearest", "  Default=0", ...entries.map((color, value) => `  Entry Color=${rgbHex(color.red, color.green, color.blue)} Value=${value}`), "}", ""];
}

function textProfiles(widgets: SurfaceWidget[]): string[] {
    const displayProcessors = new Set(["fb_c4displaylower", "fb_c4displayupper", "fb_mcudisplaylower", "fb_mcudisplayupper", "fb_mcuxtdisplaylower", "fb_mcuxtdisplayupper"]);
    if (!widgets.some((widget) => widget.body.some((line) => displayProcessors.has((line.tokens[0] ?? "").toLowerCase())))) return [];
    return ["TextProfile Display7 {", "  Encoding=ASCII7", "  Width=7", "  Padding=Space", '  ClearText=""', "  SilenceAsEmpty=true", "}", ""];
}

function dynamicTextProfiles(widgets: SurfaceWidget[]): string[] {
    if (!widgets.some((widget) => widget.body.some((line) => ["fb_sce24encodertext", "fb_sce24oledbutton"].includes((line.tokens[0] ?? "").toLowerCase())))) return [];
    return ["TextProfile DynamicText {", "  Encoding=ASCII7", "  Padding=None", '  ClearText=""', "}", ""];
}

function faderportScribbleProfiles(widgets: SurfaceWidget[]): string[] {
    if (!widgets.some((widget) => widget.body.some((line) => /^fb_fp(?:8|16)scribbleline[1-4]$/i.test(line.tokens[0] ?? "")))) return [];
    return ["TextProfile FaderPortScribble {", "  Encoding=ASCII7", "  Width=30", "  Padding=None", '  ClearText="                            "', "  DefaultAlignment=Center", "  Alignment Center Code=0", "  Alignment Left Code=1", "  Alignment Right Code=2", "  InvertCode=4", "  PresentationCombine=BitOr", "}", ""];
}

function meterProfile(widgets: SurfaceWidget[], mode: LegacyMcuMeterMode): string[] {
    if (!widgets.some((widget) => widget.body.some((line) => ["fb_mcuvumeter", "fb_mcuxtvumeter"].includes((line.tokens[0] ?? "").toLowerCase())))) return [];
    const profiles: Record<LegacyMcuMeterMode, { inputUnit: "Decibels" | "Normalized"; steps: Array<[number, number]> }> = {
        IconV1M: { inputUnit: "Decibels", steps: [[-60.1, 1], [-48.1, 2], [-42.1, 3], [-36.1, 4], [-30.1, 5], [-24.1, 6], [-18.1, 7], [-12.1, 8], [-9.1, 9], [-6.1, 10], [-3.1, 11], [0.1, 14]] },
        MCU: { inputUnit: "Normalized", steps: Array.from({ length: 14 }, (_, stepIdx) => [(stepIdx + 1) / 15, stepIdx + 1]) },
        SSLNucleus2: { inputUnit: "Decibels", steps: [[-40.5, 3], [-30.5, 4], [-20.5, 5], [-14.5, 6], [-10.5, 7], [-8.5, 8], [-6.5, 9], [-4.5, 10], [-2.5, 11], [0, 12]] },
        XTouch: { inputUnit: "Decibels", steps: [[-60.3, 1], [-54.1, 2], [-48.2, 3], [-42.1, 4], [-36.2, 5], [-30.1, 6], [-18.1, 7], [-15.1, 8], [-12.1, 9], [-9.1, 10], [-6.1, 11], [-4.6, 12], [-3.1, 13], [0.1, 14]] },
    };
    const profile = profiles[mode];
    return ["MeterProfile SurfaceMeter {", "  Mode=Steps", `  InputUnit=${profile.inputUnit}`, "  Default=0", ...profile.steps.map(([minimum, output]) => `  Step Minimum=${minimum} Output=${output}`), "}", ""];
}

function faderportMeterProfile(widgets: SurfaceWidget[]): string[] {
    if (!widgets.some((widget) => widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase() === "fb_fpvumeter"))) return [];
    return ["MeterProfile FaderPortPeakMeter {", "  Mode=Linear", "  InputUnit=Normalized", "  InputRange=[ 0, 1 ]", "  OutputRange=[ 0, 127 ]", "  Quantize=Floor", "}", ""];
}

function meterInitialization(widgets: SurfaceWidget[]): string[] {
    const deviceTypes = new Set<number>();
    for (const widget of widgets) for (const line of widget.body) {
        const processor = (line.tokens[0] ?? "").toLowerCase();
        if (processor === "fb_mcuvumeter") deviceTypes.add(0x14);
        else if (processor === "fb_mcuxtvumeter") deviceTypes.add(0x15);
    }
    if (!deviceTypes.size) return [];
    const messages: number[][] = [[0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7]];
    for (const deviceType of deviceTypes) {
        messages.push([0xF0, 0x00, 0x00, 0x66, deviceType, 0x00, 0xF7]);
        messages.push([0xF0, 0x00, 0x00, 0x66, deviceType, 0x21, 0x01, 0xF7]);
        for (let channelIdx = 0; channelIdx < 8; channelIdx++) messages.push([0xF0, 0x00, 0x00, 0x66, deviceType, 0x20, channelIdx, 0x01, 0xF7]);
    }
    return ["Initialize {", ...messages.map((message) => `  MIDI Bytes=${midiList(message.map((byte) => byte.toString(16)))}`), "}", ""];
}

function sysExTextPayload(values: string[]): string {
    return `[ ${values.map(midiByte).join(", ")}, Text ]`;
}

function convertProcessor(widget: SurfaceWidget, tokens: string[], lineNumber: number, diagnostics: Diagnostic[], documentPath: string): string | undefined {
    const processor = (tokens[0] ?? "").toLowerCase();
    const conversion = LEGACY_SURFACE_PROCESSOR_CONVERSIONS.get(processor);
    if (!conversion) {
        addDiagnostic(diagnostics, "error", "legacy.surface.processor.unsupported", `Legacy Surface processor is not converted yet: ${tokens[0]}`, lineNumber, documentPath);
        return undefined;
    }
    if (conversion.kind === "anyPress" && tokens[1] && tokens[2]) return `Input Press { Encoding=MIDIPrefix Message=${midiList(tokens.slice(1, 3))} }`;
    if (conversion.kind === "oscControl" && tokens[1]) return `Input Value { Encoding=OSCFloat Address=${JSON.stringify(tokens[1])} }`;
    if (conversion.kind === "oscFeedback" && tokens[1]) {
        const address = JSON.stringify(tokens[1]);
        const colorAddress = JSON.stringify(`${tokens[1]}/Color`);
        return `Feedback Value { Encoding=OSCFloat Address=${address} }\n  Feedback Text { Encoding=OSCString Address=${address} }\n  Feedback Color { Encoding=OSCString Address=${colorAddress} Format=HexRGBA }`;
    }
    if (conversion.kind === "press" && tokens.length >= 4) {
        const off = tokens.length >= 7 ? ` Off=${midiList(tokens.slice(4, 7))}` : "";
        return `Input Press { Encoding=MIDIExact On=${midiList(tokens.slice(1, 4))}${off} }`;
    }
    if (conversion.kind === "touch" && tokens.length >= 7) return `Input Touch { Encoding=MIDIExact On=${midiList(tokens.slice(1, 4))} Off=${midiList(tokens.slice(4, 7))} }`;
    if (conversion.kind === "fader7" && tokens[1] && tokens[2]) return `Input Value { Encoding=MIDI7 Message=${midiList(tokens.slice(1, 3))} }`;
    if (conversion.kind === "fader7Feedback" && tokens[1] && tokens[2]) return `Feedback Value { Encoding=MIDI7 Message=${midiList(tokens.slice(1, 3))} }`;
    if (conversion.kind === "fader14" && tokens[1]) return `Input Value { Encoding=MIDI14 Status=${midiByte(tokens[1])} }`;
    if (conversion.kind === "fader14Feedback" && tokens[1]) return `Feedback Value { Encoding=MIDI14 Status=${midiByte(tokens[1])} SuppressWhileTouched=true }`;
    if (conversion.kind === "encoder" && tokens[1] && tokens[2]) {
        const profile = widget.widgetClass ? ` Profile=${widget.widgetClass}` : " Mode=SignedBit";
        return `Input Encoder { Encoding=MIDI7 Message=${midiList(tokens.slice(1, 3))}${profile} }`;
    }
    if (conversion.kind === "ring" && tokens[1] && tokens[2]) {
        const dataByte = Number.parseInt(tokens[2].replace(/^0x/i, ""), 16) + 0x20;
        if (!Number.isInteger(dataByte) || dataByte > 0x7F) {
            addDiagnostic(diagnostics, "error", "legacy.surface.fb-encoder.message", `Legacy FB_Encoder data byte cannot be converted to its ring output address: ${tokens[2]}`, lineNumber, documentPath);
            return undefined;
        }
        return `Feedback Ring { Encoding=MIDI7 Message=${midiList([tokens[1], dataByte.toString(16)])} RingProfile=RotaryRing StyleTarget=Value StyleShift=4 StyleCombine=BitOr }`;
    }
    if (conversion.kind === "sce24Ring" && tokens.length >= 4) {
        const status = Number.parseInt(tokens[1].replace(/^0x/i, ""), 16);
        const address = Number.parseInt(tokens[2].replace(/^0x/i, ""), 16);
        const legacyValue = Number.parseInt(tokens[3].replace(/^0x/i, ""), 16);
        if (Number.isInteger(status) && status >= 0x80 && status <= 0xEF && Number.isInteger(address) && address >= 0 && address <= 0x7F && Number.isInteger(legacyValue) && legacyValue >= 0 && legacyValue <= 0x7F) {
            const payload = `[ 0x00, 0x02, 0x38, 0x01, ${midiByte(address.toString(16))}, SegmentMasks, SegmentRed7, SegmentGreen7, SegmentBlue7 ]`;
            return `Feedback Ring {\n    Encoding=MIDI7\n    Message=${midiList([tokens[1], tokens[2]])}\n    RingProfile=SCE24Ring\n    StyleTarget=Value\n    StyleShift=4\n    StyleCombine=BitOr\n    Configure { Encoding=MIDISysEx Payload=${payload} }\n  }`;
        }
    }
    if (conversion.kind === "sce24State" && tokens.length >= 4) {
        const address = Number.parseInt(tokens[2].replace(/^0x/i, ""), 16) + 0x60;
        if (Number.isInteger(address) && address >= 0 && address <= 0x7F) return `Feedback State { Encoding=MIDISysEx Payload=[ 0x00, 0x02, 0x38, 0x01, ${midiByte(address.toString(16))}, Red7, Green7, Blue7 ] }`;
    }
    if (conversion.kind === "state" && tokens.length >= 7) return `Feedback State { Encoding=MIDIExact On=${midiList(tokens.slice(1, 4))} Off=${midiList(tokens.slice(4, 7))} }`;
    if (conversion.kind === "faderportRgb" && tokens.length >= 4) {
        const dataByte = midiByte(tokens[2]);
        return `Feedback Color { Encoding=MIDIRGB Enable=${midiList(tokens.slice(1, 4))} Red=${midiList(["91", dataByte])} Green=${midiList(["92", dataByte])} Blue=${midiList(["93", dataByte])} }`;
    }
    if (conversion.kind === "faderportTwoStateRgb" && tokens.length >= 4) {
        const dataByte = midiByte(tokens[2]);
        return `Feedback Color { Encoding=MIDIRGB Enable=${midiList(["90", dataByte, "7f"])} Red=${midiList(["91", dataByte])} Green=${midiList(["92", dataByte])} Blue=${midiList(["93", dataByte])} InactiveBrightness=0.1111111111111111 ActiveBrightness=1 }`;
    }
    if (conversion.kind === "faderportScribble" && tokens[1] && /^fb_fp(?:8|16)scribbleline[1-4]$/.test(processor)) {
        const channel = Number(tokens[1]);
        const row = Number(processor.at(-1)) - 1;
        const displayType = processor.startsWith("fb_fp16") ? 0x16 : 0x02;
        if (Number.isInteger(channel) && channel >= 0 && channel <= 15) {
            const prefix = ["00", "01", "06", displayType.toString(16), "12", channel.toString(16), row.toString(16)].map(midiByte).join(", ");
            return `Feedback Text { Encoding=MIDISysEx TextProfile=FaderPortScribble Payload=[ ${prefix}, TextPresentationCode, Text ] }`;
        }
    }
    if (conversion.kind === "bar" && tokens[1] && /^\d+$/.test(tokens[1])) {
        const channel = Number(tokens[1]);
        if (channel >= 0 && channel <= 15) {
            const valueData = channel < 8 ? 0x30 + channel : 0x40 + channel - 8;
            const styleData = channel < 8 ? 0x38 + channel : 0x48 + channel - 8;
            return `Feedback Bar { Encoding=MIDI7 Message=${midiList(["b0", valueData.toString(16)])} StyleMessage=${midiList(["b0", styleData.toString(16)])} BarProfile=StandardBar }`;
        }
    }
    if (conversion.kind === "midiPalette" && tokens.length >= 4) {
        const status = Number.parseInt(tokens[1].replace(/^0x/i, ""), 16);
        if (Number.isInteger(status) && status >= 0x80 && status < 0xEF) return `Feedback Color { Encoding=MIDIPalette Message=${midiList(tokens.slice(1, 3))} ColorProfile=Palette128 Companion=${midiList([(status + 1).toString(16), tokens[2], "2f"])} CompanionOrder=After }`;
    }
    if (conversion.kind === "mcuDisplay") {
        const lower = processor.includes("lower");
        const c4 = processor.startsWith("fb_c4");
        const channelToken = c4 ? tokens[2] : tokens[1];
        const rowToken = c4 ? tokens[1] : undefined;
        if (channelToken && /^\d+$/.test(channelToken) && (!c4 || (rowToken && /^\d+$/.test(rowToken)))) {
            const channel = Number(channelToken);
            const row = rowToken ? Number(rowToken) : 0;
            if (channel >= 0 && channel <= 7 && (!c4 || (row >= 0 && row <= 3))) {
                const displayType = c4 ? 0x17 : processor.startsWith("fb_mcuxt") ? 0x15 : 0x14;
                const displayRow = c4 ? 0x30 + row : 0x12;
                const offset = channel * 7 + (lower ? 56 : 0);
                return `Feedback Text { Encoding=MIDISysEx Payload=${sysExTextPayload(["00", "00", "66", displayType.toString(16), displayRow.toString(16), offset.toString(16)])} TextProfile=Display7 }`;
            }
        }
    }
    if (conversion.kind === "mcuMeter" && tokens[1] && /^\d+$/.test(tokens[1])) {
        const channel = Number(tokens[1]);
        if (channel >= 0 && channel <= 7) return `Feedback Meter { Encoding=MIDI7 Message=[ 0xD0 ] MeterProfile=SurfaceMeter ValueBase=${midiByte((channel << 4).toString(16))} Combine=BitOr Refresh=Continuous RefreshIntervalMs=10 }`;
    }
    if (conversion.kind === "faderportMeter" && tokens[1] && /^\d+$/.test(tokens[1])) {
        const channel = Number(tokens[1]);
        if (channel >= 0 && channel <= 15) {
            const status = channel < 8 ? 0xD0 + channel : 0xC0 + channel - 8;
            return `Feedback Meter { Encoding=MIDI7 Message=[ ${midiByte(status.toString(16))} ] MeterProfile=FaderPortPeakMeter Refresh=Continuous RefreshIntervalMs=10 }`;
        }
    }
    if (conversion.kind === "sce24Text" && tokens.length >= 7) {
        const address = Number.parseInt(tokens[2].replace(/^0x/i, ""), 16) + (processor === "fb_sce24oledbutton" ? 0x60 : 0);
        const topMargin = Number(tokens[4]);
        const bottomMargin = Number(tokens[5]);
        const font = Number(tokens[6]);
        if ([address, topMargin, bottomMargin, font].every((value) => Number.isInteger(value) && value >= 0 && value <= 0x7F)) {
            const payload = `[ 0x00, 0x02, 0x38, 0x01, ${midiByte(address.toString(16))}, TopMargin7, BottomMargin7, Font7, BackgroundRed7, BackgroundGreen7, BackgroundBlue7, TextRed7, TextGreen7, TextBlue7, Text ]`;
            return `Feedback Text { Encoding=MIDISysEx TextProfile=DynamicText TopMargin=${topMargin} BottomMargin=${bottomMargin} Font=${font} BackgroundColor=#000000 TextColor=#000000 Payload=${payload} }`;
        }
    }
    addDiagnostic(diagnostics, "error", "legacy.surface.processor.parameters", `Legacy Surface processor has missing or invalid parameters: ${tokens[0]}`, lineNumber, documentPath);
    return undefined;
}

function isHiddenWidget(sourceLines: ReturnType<typeof splitSourceLines>, widget: SurfaceWidget): boolean {
    return /\bOSKHidden\b/i.test(sourceLines[widget.line - 1]?.text ?? "");
}

function visibleWidgets(sourceLines: ReturnType<typeof splitSourceLines>, widgets: SurfaceWidget[]): SurfaceWidget[] {
    return widgets.filter((widget) => {
        if (isHiddenWidget(sourceLines, widget)) return false;
        return widget.body.some((line) => ["press", "anypress", "fader14bit", "encoder", "x32fader", "x32rotarytoencoder"].includes((line.tokens[0] ?? "").toLowerCase()));
    });
}

function legacyWidgetChannel(widget: SurfaceWidget): number | undefined {
    for (const line of widget.body) {
        if (!/^fb_fp(?:8|16)scribbleline[1-4]$/i.test(line.tokens[0] ?? "") && (line.tokens[0] ?? "").toLowerCase() !== "fb_fpvumeter") continue;
        const channel = Number(line.tokens[1]);
        if (Number.isInteger(channel) && channel >= 0 && channel <= 15) return channel + 1;
    }
    return undefined;
}

function convertedLayoutCell(cell: OskLayoutCell): string {
    if (cell.type === "spacer") return `    Spacer${propertyText(cell.properties)}`;
    return `    Widget ${cell.widgetName}${propertyText(cell.properties)}`;
}

function explicitLayout(source: string): string[] | undefined {
    const parsed = parseSurface(source);
    if (!parsed.semantic.oskLayout) return undefined;
    const result = ["OSKLayout {"];
    for (const row of parsed.semantic.oskLayout.rows) result.push("  Row {", ...row.cells.map(convertedLayoutCell), "  }");
    result.push("}");
    return result;
}

function automaticLayoutProperties(sourceLines: ReturnType<typeof splitSourceLines>, widget: SurfaceWidget, widgets: SurfaceWidget[], fader: boolean): Map<string, string> {
    const properties = new Map(widget.oskProperties);
    if (fader) {
        properties.set("Shape", "Fader");
        properties.set("Height", "7");
        if (widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase() === "touch")) properties.set("TouchTarget", widget.name);
        properties.set("ValueTarget", widget.name);
    }
    if (widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase().includes("encoder"))) {
        properties.set("ScrollTarget", widget.name);
        const group = [...widget.oskProperties].find(([name]) => name.toLowerCase() === "group")?.[1];
        const pressTarget = group ? widgets.find((candidate) => candidate !== widget && isHiddenWidget(sourceLines, candidate) && [...candidate.oskProperties].some(([name, value]) => name.toLowerCase() === "group" && value.toLowerCase() === group.toLowerCase()) && candidate.body.some((line) => ["press", "anypress"].includes((line.tokens[0] ?? "").toLowerCase()))) : undefined;
        if (pressTarget) properties.set("PressTarget", pressTarget.name);
    }
    return properties;
}

function isFaderWidget(widget: SurfaceWidget): boolean {
    return widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase().includes("fader") && !(line.tokens[0] ?? "").toLowerCase().startsWith("fb_"));
}

function legacyCommentLayout(source: string, widgets: SurfaceWidget[]): string[] | undefined {
    const sourceLines = splitSourceLines(source);
    type LegacyLayoutCell = { properties?: Map<string, string>; type: "spacer" | "widget"; widget?: SurfaceWidget };
    const rows: LegacyLayoutCell[][] = [];
    let currentRow: LegacyLayoutCell[] | undefined;
    const widgetsByLine = new Map(widgets.map((widget) => [widget.line, widget]));
    for (const line of sourceLines) {
        const text = analysisText(line);
        if (/^\/\/\s*OSKRow\b/i.test(text)) {
            currentRow = [];
            rows.push(currentRow);
            continue;
        }
        const spacerMatch = text.match(/^\/\/\s*OSKSpacer(?:\s+Width=([^\s]+))?/i);
        if (spacerMatch && currentRow) {
            currentRow.push({ properties: new Map([["Width", spacerMatch[1] ?? "0.5"]]), type: "spacer" });
            continue;
        }
        const widget = widgetsByLine.get(line.lineNumber);
        if (widget && currentRow && !isHiddenWidget(sourceLines, widget) && !isFaderWidget(widget)) currentRow.push({ type: "widget", widget });
    }
    if (!rows.length) return undefined;
    const faders = widgets.filter((widget) => !isHiddenWidget(sourceLines, widget) && isFaderWidget(widget));
    for (let faderIdx = faders.length - 1; faderIdx >= 0; faderIdx--) rows[0].unshift({ type: "widget", widget: faders[faderIdx] });
    const result = ["OSKLayout {"];
    for (const row of rows) {
        result.push("  Row {");
        for (const cell of row) {
            if (cell.type === "spacer") result.push(`    Spacer${propertyText(cell.properties!)}`);
            else if (cell.widget) {
                const properties = automaticLayoutProperties(sourceLines, cell.widget, widgets, isFaderWidget(cell.widget));
                if (isFaderWidget(cell.widget)) properties.set("Height", String(Math.max(7, rows.length)));
                result.push(`    Widget ${cell.widget.name}${propertyText(properties)}`);
            }
        }
        result.push("  }");
    }
    result.push("}");
    return result;
}

function automaticLayout(source: string, widgets: SurfaceWidget[]): string[] {
    const sourceLines = splitSourceLines(source);
    const visible = visibleWidgets(sourceLines, widgets);
    if (!visible.length) return [];
    const faders = visible.filter(isFaderWidget);
    const normal = visible.filter((widget) => !faders.includes(widget));
    const rowCount = faders.length ? 7 : Math.max(1, Math.ceil(Math.sqrt(normal.length || visible.length)));
    const normalColumnCount = Math.max(1, Math.ceil(normal.length / rowCount));
    const rows: string[][] = Array.from({ length: Math.max(rowCount, Math.ceil(normal.length / normalColumnCount)) }, () => []);
    for (let faderIdx = 0; faderIdx < faders.length; faderIdx++) {
        const widget = faders[faderIdx];
        const metadata = automaticLayoutProperties(sourceLines, widget, widgets, true);
        metadata.set("Height", String(Math.max(7, rows.length)));
        rows[0].push(`    Widget ${widget.name}${propertyText(metadata)}`);
    }
    for (let widgetIdx = 0; widgetIdx < normal.length; widgetIdx++) {
        const rowIdx = widgetIdx % rows.length;
        const widget = normal[widgetIdx];
        rows[rowIdx].push(`    Widget ${widget.name}${propertyText(automaticLayoutProperties(sourceLines, widget, widgets, false))}`);
    }
    return ["OSKLayout {", ...rows.flatMap((row) => ["  Row {", ...row, "  }"]), "}"];
}

function legacyProtocol(document: ReturnType<typeof parseSurface>): "MIDI" | "OSC" {
    for (const widget of document.semantic.widgets) for (const line of widget.body) {
        const processor = (line.tokens[0] ?? "").toLowerCase();
        if (processor.startsWith("x32") || line.tokens.slice(1).some((token) => token.startsWith("/"))) return "OSC";
    }
    return "MIDI";
}

function inferredChannelCount(document: ReturnType<typeof parseSurface>): number {
    const suffixesByFamily = new Map<string, Set<number>>();
    for (const widget of document.semantic.widgets) {
        const match = widget.name.match(/^(.*?)(\d+)$/);
        if (!match) continue;
        const suffixes = suffixesByFamily.get(match[1]) ?? new Set<number>();
        suffixes.add(Number(match[2]));
        suffixesByFamily.set(match[1], suffixes);
    }
    let channels = 1;
    for (const suffixes of suffixesByFamily.values()) {
        let contiguousCount = 0;
        while (suffixes.has(contiguousCount + 1)) contiguousCount++;
        channels = Math.max(channels, contiguousCount);
    }
    return channels;
}

export function convertLegacySurfaceToFormat2(source: string, surfaceName: string, documentPath: string, meterMode: LegacyMcuMeterMode = "XTouch"): LegacySurfaceConversion {
    if (/^\s*(?:\uFEFF)?@Meta\s*\{[^}]*\bVersion=2\b/i.test(source)) {
        const currentDocument = parseSurface(source, documentPath);
        return { diagnostics: currentDocument.diagnostics, source };
    }
    const document = parseSurface(source, documentPath);
    const diagnostics: Diagnostic[] = document.diagnostics.filter((diagnostic) => diagnostic.code !== "surface.format.missing");
    const blocks = legacyBlocks(source);
    const output: string[] = [`@Meta { Version=2 Protocol=${legacyProtocol(document)} Channels=${inferredChannelCount(document)} Name=${JSON.stringify(surfaceName)} }`, "", ...encoderProfiles(blocks), ...colorCalibration(blocks), ...ringProfiles(document.semantic.widgets), ...barProfiles(document.semantic.widgets), ...paletteProfiles(document.semantic.widgets), ...textProfiles(document.semantic.widgets), ...dynamicTextProfiles(document.semantic.widgets), ...faderportScribbleProfiles(document.semantic.widgets), ...faderportMeterProfile(document.semantic.widgets), ...meterProfile(document.semantic.widgets, meterMode), ...meterInitialization(document.semantic.widgets)];
    for (const widget of document.semantic.widgets) {
        output.push(`Widget ${widget.name} {`);
        const channel = legacyWidgetChannel(widget);
        if (channel) output.push(`  Channel=${channel}`);
        for (const line of widget.body) {
            const converted = convertProcessor(widget, line.tokens, line.lineNumber, diagnostics, documentPath);
            if (converted) output.push(`  ${converted}`);
        }
        output.push("}", "");
    }
    output.push(...(explicitLayout(source) ?? legacyCommentLayout(source, document.semantic.widgets) ?? automaticLayout(source, document.semantic.widgets)), "");
    return { diagnostics, source: output.join("\n") };
}
