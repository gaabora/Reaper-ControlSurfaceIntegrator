import type { ActionTraits } from "./action-catalog.ts";
import { parseByPath, type AnyDocument } from "./formats.ts";
import { serializeDocument, type Diagnostic, type DiagnosticQuickFix } from "./model.ts";
import { legacyMainBankContext } from "./legacy-zone-format2.ts";
import type { SettingsSchema } from "./settings-schema.ts";
import { suggestSimilarStrings } from "./string-distance.ts";
import { convertHashCommentLine, convertSingleSlashCommentLine } from "./text.ts";
import type { ZoneSemantic } from "./zone.ts";

export class QuickFixError extends Error {
    constructor(public readonly code: string, message: string) {
        super(message);
        this.name = "QuickFixError";
    }
}

export interface QuickFixRequest {
    diagnostic: Pick<Diagnostic, "code" | "line" | "message">;
    fix: Pick<DiagnosticQuickFix, "data" | "id">;
}

interface QuickFixContext {
    diagnostic: Diagnostic;
    document: AnyDocument;
    knownActions: Set<string>;
}

interface QuickFixDefinition {
    apply: (context: QuickFixContext, fix: DiagnosticQuickFix) => string;
    acceptsSetDiagnostic?: boolean;
    fixes: (context: QuickFixContext) => DiagnosticQuickFix[];
    id: string;
}

function prependMarker(source: string, marker: string, document: AnyDocument): string {
    const lineEnding = document.lines.find((line) => line.ending)?.ending ?? "\n";
    const byteOrderMark = source.startsWith("\uFEFF") ? "\uFEFF" : "";
    return `${byteOrderMark}${marker}${lineEnding}${source.slice(byteOrderMark.length)}`;
}

function convertSingleSlashComment(context: QuickFixContext): string {
    const lineNumber = context.diagnostic.line;
    const line = lineNumber ? context.document.lines[lineNumber - 1] : undefined;
    if (!line) throw new QuickFixError("quick-fix.line", "The single-slash comment line is no longer available");
    const convertedText = convertSingleSlashCommentLine(line.text);
    if (convertedText === line.text) throw new QuickFixError("quick-fix.source", "The line is no longer a single-slash comment");
    line.text = convertedText;
    return serializeDocument(context.document);
}

function convertHashComment(context: QuickFixContext): string {
    const lineNumber = context.diagnostic.line;
    const line = lineNumber ? context.document.lines[lineNumber - 1] : undefined;
    if (!line) throw new QuickFixError("quick-fix.line", "The hash comment line is no longer available");
    const convertedText = convertHashCommentLine(line.text);
    if (convertedText === line.text) throw new QuickFixError("quick-fix.source", "The line is no longer a hash comment");
    line.text = convertedText;
    return serializeDocument(context.document);
}

function dependencyAtDiagnosticLine(context: QuickFixContext): string | undefined {
    if (context.document.format !== "zone" || !context.diagnostic.line) return undefined;
    const semantic = context.document.semantic as ZoneSemantic;
    return semantic.dependencyReferences.find((reference) => reference.line === context.diagnostic.line)?.name;
}

function commentOutDependency(context: QuickFixContext, fix: DiagnosticQuickFix): string {
    const dependency = dependencyAtDiagnosticLine(context);
    if (!dependency || dependency.toLowerCase() !== fix.data?.dependency?.toLowerCase()) throw new QuickFixError("quick-fix.source", "The dependency line no longer matches this cycle");
    const line = context.document.lines[context.diagnostic.line! - 1];
    if (!line) throw new QuickFixError("quick-fix.line", "The dependency line is no longer available");
    line.text = line.text.replace(/^(\s*)/, "$1// ");
    return serializeDocument(context.document);
}

function unknownZoneActionAtDiagnosticLine(context: QuickFixContext): string | undefined {
    if (context.document.format !== "zone" || !context.diagnostic.line) return undefined;
    const semantic = context.document.semantic as ZoneSemantic;
    return semantic.bindings.find((binding) => binding.line === context.diagnostic.line)?.action;
}

function replaceUnknownZoneAction(context: QuickFixContext, fix: DiagnosticQuickFix): string {
    const currentAction = unknownZoneActionAtDiagnosticLine(context);
    const originalAction = fix.data?.originalAction;
    const replacementAction = fix.data?.replacementAction;
    if (!currentAction || currentAction !== originalAction) throw new QuickFixError("quick-fix.source", "The unknown action line no longer matches this suggestion");
    if (!replacementAction || !context.knownActions.has(replacementAction)) throw new QuickFixError("quick-fix.action", "The suggested action is no longer available");
    const line = context.document.lines[context.diagnostic.line! - 1];
    if (!line) throw new QuickFixError("quick-fix.line", "The unknown action line is no longer available");
    const bindingMatch = line.text.match(/^(\s*\S+\s+)(\S+)(.*)$/);
    if (!bindingMatch || bindingMatch[2] !== originalAction) throw new QuickFixError("quick-fix.source", "The action token is no longer available at the expected position");
    line.text = bindingMatch[1] + replacementAction + bindingMatch[3];
    return serializeDocument(context.document);
}

function commentOutDiagnosticLine(context: QuickFixContext): string {
    const line = context.diagnostic.line ? context.document.lines[context.diagnostic.line - 1] : undefined;
    if (!line || line.kind === "comment") throw new QuickFixError("quick-fix.line", "The diagnostic line is no longer available");
    line.text = line.text.replace(/^(\s*)/, "$1// ");
    return serializeDocument(context.document);
}

function commentOutDocument(context: QuickFixContext): string {
    for (const line of context.document.lines) if (line.text.trim() && line.kind !== "comment") line.text = line.text.replace(/^(\s*)/, "$1// ");
    return serializeDocument(context.document);
}

export interface QuickFixDocumentSource {
    path: string;
    source: string;
}

function bindingActionAtDiagnosticLine(context: QuickFixContext): string | undefined {
    if (context.document.format !== "zone" || !context.diagnostic.line) return undefined;
    return (context.document.semantic as ZoneSemantic).bindings.find((binding) => binding.line === context.diagnostic.line)?.action;
}

function replaceBindingAction(context: QuickFixContext, expectedAction: string, replacementAction: string): string {
    if (bindingActionAtDiagnosticLine(context) !== expectedAction) throw new QuickFixError("quick-fix.source", `The ${expectedAction} action is no longer available at this line`);
    const line = context.document.lines[context.diagnostic.line! - 1];
    const bindingMatch = line?.text.match(/^(\s*\S+\s+)(\S+)(.*)$/);
    if (!line || !bindingMatch || bindingMatch[2] !== expectedAction) throw new QuickFixError("quick-fix.source", `The ${expectedAction} action token is no longer available`);
    line.text = bindingMatch[1] + replacementAction + bindingMatch[3];
    return serializeDocument(context.document);
}

function makeZoneLayer(context: QuickFixContext): string {
    if (context.document.format !== "zone" || context.document.version !== "2") throw new QuickFixError("quick-fix.format", "Only a format 2 Zone can become a layer");
    const semantic = context.document.semantic as ZoneSemantic;
    if (semantic.target || semantic.role && semantic.role !== "Layer") throw new QuickFixError("quick-fix.context", "This Zone already has an incompatible Role or Target");
    const metadataLine = context.document.lines.find((line) => line.text.trimStart().startsWith("@Meta"));
    if (!metadataLine) throw new QuickFixError("quick-fix.source", "The Zone metadata line is not available");
    if (semantic.role !== "Layer") metadataLine.text = metadataLine.text.replace(/\s*\}(\s*(?:\/\/.*)?)$/, " Role=Layer }$1");
    for (const binding of semantic.bindings) {
        if (binding.action !== "LeaveSubZone" && binding.action !== "GoHome") continue;
        const line = context.document.lines[binding.line - 1];
        const bindingMatch = line?.text.match(/^(\s*\S+\s+)(LeaveSubZone|GoHome)(.*)$/);
        if (line && bindingMatch) line.text = bindingMatch[1] + "ExitZoneLayer" + bindingMatch[3];
    }
    return serializeDocument(context.document);
}

function addZoneLayerRelationship(homeDocument: AnyDocument, zoneName: string): string {
    if (homeDocument.format !== "zone" || homeDocument.version !== "2" || (homeDocument.semantic as ZoneSemantic).role !== "Home") throw new QuickFixError("quick-fix.context", "The related Home Zone is no longer available");
    const semantic = homeDocument.semantic as ZoneSemantic;
    const matchingBindings = semantic.bindings.filter((binding) => ["GoZone", "EnterZoneLayer"].includes(binding.action) && binding.params[0]?.toLowerCase() === zoneName.toLowerCase());
    if (!matchingBindings.length) throw new QuickFixError("quick-fix.context", `Home has no navigation binding for ${zoneName}`);
    for (const binding of matchingBindings.filter((candidate) => candidate.action === "GoZone")) {
        const line = homeDocument.lines[binding.line - 1];
        const bindingMatch = line?.text.match(/^(\s*\S+\s+)GoZone(\s+.*)$/);
        if (!line || !bindingMatch) throw new QuickFixError("quick-fix.source", `The GoZone binding for ${zoneName} is no longer available`);
        line.text = bindingMatch[1] + "EnterZoneLayer" + bindingMatch[2];
    }
    if (semantic.subZones.some((name) => name.toLowerCase() === zoneName.toLowerCase())) return serializeDocument(homeDocument);
    const relationStartIdx = homeDocument.lines.findIndex((line) => line.tokens[0] === "ZoneLayers");
    if (relationStartIdx >= 0) {
        const relationStart = homeDocument.lines[relationStartIdx];
        const openBraceIdx = relationStart.text.indexOf("{");
        const closeBraceIdx = relationStart.text.lastIndexOf("}");
        if (closeBraceIdx > openBraceIdx) relationStart.text = relationStart.text.slice(0, closeBraceIdx).trimEnd() + ` ${zoneName} ` + relationStart.text.slice(closeBraceIdx);
        else {
            const relationEndIdx = homeDocument.lines.findIndex((line, lineIdx) => lineIdx > relationStartIdx && line.text.trim() === "}");
            if (relationEndIdx < 0) throw new QuickFixError("quick-fix.source", "The ZoneLayers block has no closing brace");
            const lineEnding = homeDocument.lines[relationEndIdx].ending || homeDocument.lines.find((line) => line.ending)?.ending || "\n";
            homeDocument.lines.splice(relationEndIdx, 0, { ending: lineEnding, kind: "entry", lineNumber: relationEndIdx + 1, text: `  ${zoneName}`, tokens: [zoneName] });
        }
        return serializeDocument(homeDocument);
    }
    const metadataEndIdx = homeDocument.lines.findIndex((line, lineIdx) => lineIdx > 0 && line.kind !== "format" && line.kind !== "block-end");
    const insertIdx = metadataEndIdx < 0 ? homeDocument.lines.length : metadataEndIdx;
    const lineEnding = homeDocument.lines.find((line) => line.ending)?.ending || "\n";
    const relationLines = [`ZoneLayers {`, `  ${zoneName}`, "}", ""].map((text, lineIdx) => ({ ending: lineEnding, kind: lineIdx === 0 ? "block-start" as const : lineIdx === 2 ? "block-end" as const : lineIdx === 3 ? "blank" as const : "entry" as const, lineNumber: insertIdx + lineIdx + 1, text, tokens: [] }));
    homeDocument.lines.splice(insertIdx, 0, ...relationLines);
    return serializeDocument(homeDocument);
}

export function addLegacyExitLayerQuickFixes(documents: AnyDocument[]): void {
    const selectedHomes = documents.filter((document) => document.format === "zone" && (document.semantic as ZoneSemantic).role === "Home");
    for (const targetDocument of documents.filter((document) => document.format === "zone")) {
        const zoneName = (targetDocument.semantic as ZoneSemantic).name;
        if (!zoneName || !targetDocument.path) continue;
        const matchingHomes = selectedHomes.filter((homeDocument) => (homeDocument.semantic as ZoneSemantic).bindings.some((binding) => ["GoZone", "EnterZoneLayer"].includes(binding.action) && binding.params[0]?.toLowerCase() === zoneName.toLowerCase()));
        const homePath = matchingHomes.length === 1 ? matchingHomes[0].path : undefined;
        if (!homePath) continue;
        for (const diagnostic of targetDocument.diagnostics.filter((candidate) => candidate.code === "legacy.zone.exit.context")) {
            diagnostic.fixes = [...(diagnostic.fixes ?? []), { data: { homePath, targetPath: targetDocument.path, zoneName }, id: "zone.relationship.make-layer", label: `Make ${zoneName} a linked Layer` }];
        }
    }
}

export function addLegacyBankContextQuickFixes(documents: AnyDocument[]): void {
    for (const sourceDocument of documents.filter((document) => document.format === "zone" && document.path)) {
        const semantic = sourceDocument.semantic as ZoneSemantic;
        for (const diagnostic of sourceDocument.diagnostics.filter((candidate) => candidate.code === "legacy.zone.bank.context" && candidate.line)) {
            const binding = semantic.bindings.find((candidate) => candidate.line === diagnostic.line && candidate.action === "Bank" && candidate.params.length === 2);
            const targetName = binding?.params[0];
            if (!targetName) continue;
            const requiredContext = legacyMainBankContext(targetName);
            const destinations = documents.filter((document) => {
                if (!requiredContext || !document.path || document.path === sourceDocument.path || document.format !== "zone") return false;
                const destination = document.semantic as ZoneSemantic;
                return destination.name?.toLowerCase() === targetName.toLowerCase() && destination.target === requiredContext.target && destination.bankTarget === requiredContext.bankTarget;
            });
            const destinationPath = destinations.length === 1 ? destinations[0].path : undefined;
            if (!destinationPath) continue;
            diagnostic.related = [{ line: 1, path: destinationPath }];
            diagnostic.fixes = [...(diagnostic.fixes ?? []), { data: { destinationPath, sourcePath: sourceDocument.path!, targetName }, id: "zone.bank.move-to-context", label: `Move this Bank binding to ${targetName}` }];
        }
    }
}

function applyBankContextMove(documents: QuickFixDocumentSource[], knownActions: Set<string>, request: QuickFixRequest): { changes: QuickFixDocumentSource[] } {
    const sourcePath = request.fix.data?.sourcePath;
    const destinationPath = request.fix.data?.destinationPath;
    const targetName = request.fix.data?.targetName;
    if (!sourcePath || !destinationPath || !targetName || !request.diagnostic.line) throw new QuickFixError("quick-fix.data", "The Bank move fix is missing a required path, target, or line");
    const source = documents.find((document) => document.path.toLowerCase() === sourcePath.toLowerCase());
    const destination = documents.find((document) => document.path.toLowerCase() === destinationPath.toLowerCase());
    if (!source || !destination) throw new QuickFixError("quick-fix.source", "The Bank source or destination Zone is no longer available");
    const sourceDocument = parseByPath(source.source, source.path, knownActions);
    const destinationDocument = parseByPath(destination.source, destination.path, knownActions);
    const binding = (sourceDocument.semantic as ZoneSemantic).bindings.find((candidate) => candidate.line === request.diagnostic.line && candidate.action === "Bank" && candidate.params[0]?.toLowerCase() === targetName.toLowerCase() && /^-?\d+$/.test(candidate.params[1] ?? ""));
    const sourceLineIdx = request.diagnostic.line - 1;
    const sourceLine = sourceDocument.lines[sourceLineIdx];
    const bindingMatch = sourceLine?.text.match(/^(\s*\S+\s+Bank\s+)\S+\s+(-?\d+.*)$/);
    if (!binding || !sourceLine || !bindingMatch) throw new QuickFixError("quick-fix.source", "The named Bank binding is no longer available");
    const movedLine = { ...sourceLine, lineNumber: destinationDocument.lines.length + 1, text: bindingMatch[1] + bindingMatch[2] };
    sourceDocument.lines.splice(sourceLineIdx, 1);
    let destinationIdx = destinationDocument.lines.length;
    while (destinationIdx > 0 && !destinationDocument.lines[destinationIdx - 1].text.trim()) destinationIdx--;
    destinationDocument.lines.splice(destinationIdx, 0, movedLine);
    return { changes: [{ path: source.path, source: serializeDocument(sourceDocument) }, { path: destination.path, source: serializeDocument(destinationDocument) }] };
}

export function applyQuickFixSet(documents: QuickFixDocumentSource[], knownActions: Set<string>, request: QuickFixRequest): { changes: QuickFixDocumentSource[] } {
    if (request.fix.id === "zone.bank.move-to-context") return applyBankContextMove(documents, knownActions, request);
    if (request.fix.id !== "zone.relationship.make-layer") throw new QuickFixError("quick-fix.unknown", `Unknown multi-file quick fix: ${request.fix.id}`);
    const homePath = request.fix.data?.homePath;
    const targetPath = request.fix.data?.targetPath;
    const zoneName = request.fix.data?.zoneName;
    if (!homePath || !targetPath || !zoneName) throw new QuickFixError("quick-fix.data", "The Layer relationship fix is missing a required path or Zone name");
    const targetSource = documents.find((document) => document.path.toLowerCase() === targetPath.toLowerCase());
    const homeSource = documents.find((document) => document.path.toLowerCase() === homePath.toLowerCase());
    if (!targetSource || !homeSource) throw new QuickFixError("quick-fix.source", "The Layer relationship files are no longer available");
    const targetDocument = parseByPath(targetSource.source, targetSource.path, knownActions);
    const targetDiagnostic: Diagnostic = { ...request.diagnostic, path: targetSource.path, severity: "error" };
    if (bindingActionAtDiagnosticLine({ diagnostic: targetDiagnostic, document: targetDocument, knownActions }) !== "LeaveSubZone") throw new QuickFixError("quick-fix.source", "The LeaveSubZone binding is no longer available");
    const fixedTargetSource = makeZoneLayer({ diagnostic: targetDiagnostic, document: targetDocument, knownActions });
    const fixedHomeSource = addZoneLayerRelationship(parseByPath(homeSource.source, homeSource.path, knownActions), zoneName);
    return { changes: [{ path: homeSource.path, source: fixedHomeSource }, { path: targetSource.path, source: fixedTargetSource }] };
}

function legacyNamedBankAtDiagnosticLine(context: QuickFixContext): { amount: string; bankTarget: string; target: string } | undefined {
    if (context.document.format !== "zone" || context.document.version !== "2" || !context.diagnostic.line) return undefined;
    const binding = (context.document.semantic as ZoneSemantic).bindings.find((candidate) => candidate.line === context.diagnostic.line && candidate.action === "Bank" && candidate.params.length === 2 && /^-?\d+$/.test(candidate.params[1]));
    if (!binding) return undefined;
    const bankContext = legacyMainBankContext(binding.params[0]);
    if (!bankContext) return undefined;
    const semantic = context.document.semantic as ZoneSemantic;
    if (semantic.role || (semantic.target && semantic.target !== bankContext.target) || (semantic.bankTarget && semantic.bankTarget !== bankContext.bankTarget)) return undefined;
    return { amount: binding.params[1], ...bankContext };
}

function convertLegacyNamedBank(context: QuickFixContext): string {
    const bank = legacyNamedBankAtDiagnosticLine(context);
    if (!bank) throw new QuickFixError("quick-fix.source", "The legacy named Bank action is no longer available");
    const line = context.document.lines[context.diagnostic.line! - 1];
    const bindingMatch = line?.text.match(/^(\s*\S+\s+Bank\s+)\S+\s+-?\d+(.*)$/);
    if (!line || !bindingMatch) throw new QuickFixError("quick-fix.source", "The legacy named Bank parameters are no longer available");
    line.text = bindingMatch[1] + bank.amount + bindingMatch[2];
    const metadataStartIdx = context.document.lines.findIndex((candidate) => candidate.text.trimStart().startsWith("@Meta"));
    const metadataLine = metadataStartIdx >= 0 ? context.document.lines.slice(metadataStartIdx).find((candidate) => candidate.text.includes("}")) : undefined;
    if (!metadataLine) throw new QuickFixError("quick-fix.source", "The Zone metadata block is not available");
    const additions = [!(context.document.semantic as ZoneSemantic).target ? `Target=${bank.target}` : "", !(context.document.semantic as ZoneSemantic).bankTarget ? `BankTarget=${bank.bankTarget}` : ""].filter(Boolean).join(" ");
    if (additions) metadataLine.text = metadataLine.text.replace(/\s*\}(\s*(?:\/\/.*)?)$/, ` ${additions} }$1`);
    return serializeDocument(context.document);
}

const QUICK_FIX_DEFINITIONS: QuickFixDefinition[] = [
    {
        apply: (context) => convertSingleSlashComment(context),
        fixes: (context) => context.diagnostic.code === "comment.single-slash.unsupported" ? [{ id: "comment.single-slash.convert", label: "Convert to // comment" }] : [],
        id: "comment.single-slash.convert",
    },
    {
        apply: (context) => convertHashComment(context),
        fixes: (context) => context.diagnostic.code === "comment.hash.unsupported" ? [{ id: "comment.hash.convert", label: "Convert to // comment" }] : [],
        id: "comment.hash.convert",
    },
    {
        apply: (context) => prependMarker(context.document.source, "// @format zone 1", context.document),
        fixes: (context) => context.diagnostic.code === "zone.format.missing" && context.document.format === "zone" ? [{ id: "zone.format.add", label: "Add // @format zone 1" }] : [],
        id: "zone.format.add",
    },
    {
        apply: (context, fix) => replaceUnknownZoneAction(context, fix),
        fixes: (context) => {
            if (context.diagnostic.code !== "zone.action.unknown") return [];
            const originalAction = unknownZoneActionAtDiagnosticLine(context);
            if (!originalAction) return [];
            return suggestSimilarStrings(originalAction, context.knownActions).map((replacementAction) => ({ data: { originalAction, replacementAction }, id: "zone.action.replace", label: replacementAction }));
        },
        id: "zone.action.replace",
    },
    {
        acceptsSetDiagnostic: true,
        apply: (context) => commentOutDiagnosticLine(context),
        fixes: (context) => context.diagnostic.code === "zone.action.unknown" && context.diagnostic.line ? [{ id: "zone.action.comment-out", label: "Comment out this line" }] : [],
        id: "zone.action.comment-out",
    },
    {
        acceptsSetDiagnostic: true,
        apply: (context) => commentOutDiagnosticLine(context),
        fixes: (context) => {
            if (!context.diagnostic.line) return [];
            if (context.diagnostic.code === "format2.zone.gesture.action.duplicate") return [{ id: "zone.gesture.comment-out", label: "Comment out this duplicate binding" }];
            if (context.diagnostic.code === "format2.zone.gesture.unreachable") return [{ id: "zone.gesture.comment-out", label: "Comment out this conflicting binding" }];
            return [];
        },
        id: "zone.gesture.comment-out",
    },
    {
        acceptsSetDiagnostic: true,
        apply: (context) => commentOutDocument(context),
        fixes: (context) => context.diagnostic.code === "legacy.learn-fx.source.duplicate" ? [{ id: "legacy.learn-fx.duplicate.comment-out", label: "Comment out this duplicate file" }] : [],
        id: "legacy.learn-fx.duplicate.comment-out",
    },
    {
        acceptsSetDiagnostic: true,
        apply: (context, fix) => commentOutDependency(context, fix),
        fixes: (context) => {
            if (context.diagnostic.code !== "zones.dependency.cycle") return [];
            const dependency = dependencyAtDiagnosticLine(context);
            return dependency ? [{ data: { dependency }, id: "zones.dependency.cycle.comment-out", label: `Comment out dependency on ${dependency}` }] : [];
        },
        id: "zones.dependency.cycle.comment-out",
    },
    {
        acceptsSetDiagnostic: true,
        apply: (context) => replaceBindingAction(context, "EnterZoneLayer", "GoZone"),
        fixes: (context) => context.diagnostic.code === "format2.zone-profile.layer.role" && bindingActionAtDiagnosticLine(context) === "EnterZoneLayer" ? [{ id: "zone.navigation.use-go-zone", label: "Use GoZone instead" }] : [],
        id: "zone.navigation.use-go-zone",
    },
    {
        acceptsSetDiagnostic: true,
        apply: (context) => makeZoneLayer(context),
        fixes: (context) => {
            if (context.diagnostic.code !== "format2.zone-profile.layer.role-target" || context.document.format !== "zone") return [];
            const semantic = context.document.semantic as ZoneSemantic;
            return !semantic.target && (!semantic.role || semantic.role === "Layer") ? [{ id: "zone.role.make-layer", label: "Make this Zone a Layer" }] : [];
        },
        id: "zone.role.make-layer",
    },
    {
        apply: (context) => replaceBindingAction(context, "ExitZoneLayer", "GoHome"),
        fixes: (context) => context.diagnostic.code === "format2.zone.action.layer-only" && bindingActionAtDiagnosticLine(context) === "ExitZoneLayer" ? [{ id: "zone.navigation.exit-to-home", label: "Use GoHome instead" }] : [],
        id: "zone.navigation.exit-to-home",
    },
    {
        acceptsSetDiagnostic: true,
        apply: (context) => replaceBindingAction(context, "LeaveSubZone", "GoHome"),
        fixes: (context) => context.diagnostic.code === "legacy.zone.exit.context" && bindingActionAtDiagnosticLine(context) === "LeaveSubZone" ? [{ id: "legacy.zone.exit.use-go-home", label: "Use GoHome instead" }] : [],
        id: "legacy.zone.exit.use-go-home",
    },
    {
        apply: (context) => convertLegacyNamedBank(context),
        fixes: (context) => context.diagnostic.code === "format2.zone.action.bank-amount" && legacyNamedBankAtDiagnosticLine(context) ? [{ id: "zone.bank.convert-legacy-target", label: "Move the Bank target to @Meta" }] : [],
        id: "zone.bank.convert-legacy-target",
    },
];

function fixesForDiagnostic(document: AnyDocument, diagnostic: Diagnostic, knownActions: Set<string>): DiagnosticQuickFix[] {
    const context = { diagnostic, document, knownActions };
    return QUICK_FIX_DEFINITIONS.flatMap((definition) => definition.fixes(context));
}

function dataMatches(left: Record<string, string> | undefined, right: Record<string, string> | undefined): boolean {
    return JSON.stringify(left ?? {}) === JSON.stringify(right ?? {});
}

export function diagnosticsWithQuickFixes(document: AnyDocument, knownActions: Set<string>, writable: boolean): Diagnostic[] {
    if (!writable) return document.diagnostics;
    return document.diagnostics.map((diagnostic) => diagnosticWithQuickFixes(document, diagnostic, knownActions, true));
}

export function diagnosticWithQuickFixes(document: AnyDocument, diagnostic: Diagnostic, knownActions: Set<string>, writable: boolean): Diagnostic {
    if (!writable) return diagnostic;
    const fixes = fixesForDiagnostic(document, diagnostic, knownActions);
    const combinedFixes = [...(diagnostic.fixes ?? []), ...fixes];
    return combinedFixes.length ? { ...diagnostic, fixes: combinedFixes } : diagnostic;
}

export function applyQuickFix(source: string, relativePath: string, knownActions: Set<string>, request: QuickFixRequest, settingsSchema?: SettingsSchema, actionTraits?: ReadonlyMap<string, ActionTraits>): { document: AnyDocument; source: string } {
    const document = parseByPath(source, relativePath, knownActions, settingsSchema, actionTraits);
    const requestedDefinition = QUICK_FIX_DEFINITIONS.find((candidate) => candidate.id === request.fix.id);
    if (!requestedDefinition) throw new QuickFixError("quick-fix.unknown", `Unknown quick fix: ${request.fix.id}`);
    const documentDiagnostic = document.diagnostics.find((candidate) => candidate.code === request.diagnostic.code && candidate.line === request.diagnostic.line && candidate.message === request.diagnostic.message);
    const diagnostic = documentDiagnostic ?? (requestedDefinition.acceptsSetDiagnostic ? { ...request.diagnostic, path: relativePath, severity: "error" as const } : undefined);
    if (!diagnostic) throw new QuickFixError("quick-fix.diagnostic", "The diagnostic is no longer present in the current source");
    const availableFixes = fixesForDiagnostic(document, diagnostic, knownActions);
    const selectedFix = availableFixes.find((fix) => fix.id === request.fix.id && dataMatches(fix.data, request.fix.data));
    if (!selectedFix) throw new QuickFixError("quick-fix.unavailable", "The selected quick fix is not available for this diagnostic");
    const fixedSource = requestedDefinition.apply({ diagnostic, document, knownActions }, selectedFix);
    return { document: parseByPath(fixedSource, relativePath, knownActions, settingsSchema, actionTraits), source: fixedSource };
}
