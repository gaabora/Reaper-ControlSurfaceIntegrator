import { describe, expect, test } from "bun:test";
import { parseByPath } from "../src/formats.ts";
import { addLegacyBankContextQuickFixes, addLegacyExitLayerQuickFixes, applyQuickFix, applyQuickFixSet, diagnosticWithQuickFixes, diagnosticsWithQuickFixes, QuickFixError } from "../src/quick-fixes.ts";
import { convertLegacyZoneToFormat2 } from "../src/legacy-zone-format2.ts";
import { validateDocumentSet } from "../src/validation.ts";

const knownActions = new Set(["Bank", "EnterZoneLayer", "ExitZoneLayer", "GoHome", "GoZone", "Play", "TrackPan", "TrackPanL", "TrackPanR"]);

describe("diagnostic quick fix registry", () => {
    test("offers and applies the zone format marker fix without saving", () => {
        const relativePath = "Zones/User/test/Main/Home.zon";
        const source = "\uFEFFZone Home\r\n  Play Play\r\nZoneEnd\r\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "zone.format.missing");

        expect(diagnostic?.fixes).toEqual([{ id: "zone.format.add", label: "Add // @format zone 1" }]);
        const result = applyQuickFix(source, relativePath, knownActions, { diagnostic: { code: diagnostic!.code, line: diagnostic!.line, message: diagnostic!.message }, fix: { id: diagnostic!.fixes![0].id } });
        expect(result.source).toStartWith("\uFEFF// @format zone 1\r\nZone Home");
        expect(result.document.diagnostics.some((candidate) => candidate.code === "zone.format.missing")).toBeFalse();
    });

    test("does not offer edits for a read-only document and rejects stale diagnostics", () => {
        const relativePath = "Zones/Vendor/test/Main/Home.zon";
        const source = "Zone Home\n  Play Play\nZoneEnd\n";
        const document = parseByPath(source, relativePath, knownActions);
        expect(diagnosticsWithQuickFixes(document, knownActions, false).some((diagnostic) => diagnostic.fixes?.length)).toBeFalse();
        expect(() => applyQuickFix("// @format zone 1\n" + source, relativePath, knownActions, { diagnostic: { code: "zone.format.missing", message: "Zone has no // @format zone 1 marker" }, fix: { id: "zone.format.add" } })).toThrow(QuickFixError);
    });

    test("converts an unsupported single-slash comment without saving", () => {
        const relativePath = "Zones/User/test/Main/Home.zon";
        const source = "// @format zone 1\nZone Home\n  / disabled binding\n  Play Play\nZoneEnd\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "comment.single-slash.unsupported");

        expect(diagnostic?.fixes).toEqual([{ id: "comment.single-slash.convert", label: "Convert to // comment" }]);
        const result = applyQuickFix(source, relativePath, knownActions, { diagnostic: { code: diagnostic!.code, line: diagnostic!.line, message: diagnostic!.message }, fix: { id: diagnostic!.fixes![0].id } });
        expect(result.source).toContain("\n  // disabled binding\n");
        expect(result.document.diagnostics.some((candidate) => candidate.code === "comment.single-slash.unsupported")).toBeFalse();
    });

    test("converts an unsupported hash comment without saving", () => {
        const relativePath = "Zones/User/test/Main/Home.zon";
        const source = "// @format zone 1\nZone Home\n  # disabled binding\n  Play Play\nZoneEnd\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "comment.hash.unsupported");

        expect(diagnostic?.fixes).toEqual([{ id: "comment.hash.convert", label: "Convert to // comment" }]);
        const result = applyQuickFix(source, relativePath, knownActions, { diagnostic: { code: diagnostic!.code, line: diagnostic!.line, message: diagnostic!.message }, fix: { id: diagnostic!.fixes![0].id } });
        expect(result.source).toContain("\n  // disabled binding\n");
        expect(result.document.diagnostics.some((candidate) => candidate.code === "comment.hash.unsupported")).toBeFalse();
    });

    test("offers and applies several similar runtime action fixes without changing spacing", () => {
        const relativePath = "Zones/User/test/Main/Channel.zon";
        const source = "// @format zone 1\nZone Channel\n  Rotary1    MCUTrackPan    NoFeedback=Yes // keep\nZoneEnd\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "zone.action.unknown");

        expect(diagnostic?.fixes?.map((fix) => fix.label)).toEqual(["TrackPan", "TrackPanL", "TrackPanR", "Comment out this line"]);
        const result = applyQuickFix(source, relativePath, knownActions, { diagnostic: { code: diagnostic!.code, line: diagnostic!.line, message: diagnostic!.message }, fix: diagnostic!.fixes![0] });
        expect(result.source).toContain("  Rotary1    TrackPan    NoFeedback=Yes // keep\n");
        expect(result.document.diagnostics.some((candidate) => candidate.code === "zone.action.unknown")).toBeFalse();
    });

    test("comments out the exact dependency line that closes a zone cycle", () => {
        const alphaPath = "Zones/User/test/Main/Alpha.zon";
        const betaPath = "Zones/User/test/Main/Beta.zon";
        const alpha = parseByPath("// @format zone 1\nZone Alpha\nIncludedZones\n  Beta\nIncludedZonesEnd\nZoneEnd\n", alphaPath, knownActions);
        const betaSource = "// @format zone 1\nZone Beta\nIncludedZones\n  Alpha\nIncludedZonesEnd\nZoneEnd\n";
        const beta = parseByPath(betaSource, betaPath, knownActions);
        const cycle = validateDocumentSet([alpha, beta]).find((diagnostic) => diagnostic.code === "zones.dependency.cycle")!;
        const diagnostic = diagnosticWithQuickFixes(beta, cycle, knownActions, true);

        expect(diagnostic.path).toBe(betaPath);
        expect(diagnostic.line).toBe(4);
        expect(diagnostic.fixes).toEqual([{ data: { dependency: "Alpha" }, id: "zones.dependency.cycle.comment-out", label: "Comment out dependency on Alpha" }]);
        const result = applyQuickFix(betaSource, betaPath, knownActions, { diagnostic: { code: diagnostic.code, line: diagnostic.line, message: diagnostic.message }, fix: diagnostic.fixes![0] });
        expect(result.source).toContain("\n  // Alpha\n");
    });

    test("can comment out an unknown action line", () => {
        const relativePath = "Zones/User/test/Main/Home.zon";
        const source = "@Meta { Version=2 Role=Home }\nButton the\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "zone.action.unknown")!;
        const fix = diagnostic.fixes!.find((candidate) => candidate.id === "zone.action.comment-out")!;

        expect(fix.label).toBe("Comment out this line");
        expect(applyQuickFix(source, relativePath, knownActions, { diagnostic, fix }).source).toContain("// Button the");
    });

    test("can comment out an unknown lifecycle action line", () => {
        const relativePath = "Zones/User/test/Main/Home.zon";
        const source = "@Meta { Version=2 Role=Home }\nOn Activate {\n  the\n}\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "zone.action.unknown")!;
        const fix = diagnostic.fixes!.find((candidate) => candidate.id === "zone.action.comment-out")!;

        expect(applyQuickFix(source, relativePath, knownActions, { diagnostic, fix }).source).toContain("  // the");
    });

    test("offers independent navigation and layer fixes for an invalid EnterZoneLayer target", () => {
        const homePath = "Zones/User/test/Main/Home.zon";
        const homeSource = "@Meta { Version=2 Role=Home }\nButton EnterZoneLayer Metronome\n";
        const home = parseByPath(homeSource, homePath, knownActions);
        const profileDiagnostic = { code: "format2.zone-profile.layer.role", line: 2, message: "EnterZoneLayer target 'Metronome' must declare Role=Layer", path: homePath, severity: "error" as const };
        const homeDiagnostic = diagnosticWithQuickFixes(home, profileDiagnostic, knownActions, true);
        expect(homeDiagnostic.fixes?.map((fix) => fix.label)).toEqual(["Use GoZone instead"]);
        expect(applyQuickFix(homeSource, homePath, knownActions, { diagnostic: profileDiagnostic, fix: homeDiagnostic.fixes![0] }).source).toContain("Button GoZone Metronome");

        const metronomePath = "Zones/User/test/Main/Metronome.zon";
        const metronomeSource = "@Meta { Version=2 }\nBack GoHome\n";
        const metronome = parseByPath(metronomeSource, metronomePath, knownActions);
        const targetDiagnostic = { code: "format2.zone-profile.layer.role-target", line: 1, message: "Alternative: make Zone 'Metronome' a Layer and use ExitZoneLayer for its exit.", path: metronomePath, severity: "error" as const };
        const metronomeDiagnostic = diagnosticWithQuickFixes(metronome, targetDiagnostic, knownActions, true);
        expect(metronomeDiagnostic.fixes?.map((fix) => fix.label)).toEqual(["Make this Zone a Layer"]);
        const layer = applyQuickFix(metronomeSource, metronomePath, knownActions, { diagnostic: targetDiagnostic, fix: metronomeDiagnostic.fixes![0] });
        expect(layer.source).toContain("@Meta { Version=2 Role=Layer }");
        expect(layer.source).toContain("Back ExitZoneLayer");
        expect(layer.document.diagnostics.filter((diagnostic) => diagnostic.severity === "error")).toEqual([]);
    });

    test("offers GoHome when ExitZoneLayer is used outside a layer", () => {
        const relativePath = "Zones/User/test/Main/Metronome.zon";
        const source = "@Meta { Version=2 }\nBack ExitZoneLayer\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "format2.zone.action.layer-only");
        expect(diagnostic?.fixes?.map((fix) => fix.label)).toEqual(["Use GoHome instead"]);
        expect(applyQuickFix(source, relativePath, knownActions, { diagnostic: diagnostic!, fix: diagnostic!.fixes![0] }).source).toContain("Back GoHome");
    });

    test("offers complete Home and Layer fixes for an ambiguous legacy exit", () => {
        const homePath = "Zones/User/test/Main/Home.zon";
        const targetPath = "Zones/User/test/Main/Metronome.zon";
        const homeSource = "@Meta { Version=2 Role=Home }\nButton GoZone Metronome\n";
        const conversion = convertLegacyZoneToFormat2("Zone Metronome\nBack LeaveSubZone\nZoneEnd\n", { profile: "Main", targetPath });
        const homeDocument = parseByPath(homeSource, homePath, knownActions);
        const targetDocument = parseByPath(conversion.source, targetPath, knownActions);
        targetDocument.diagnostics.push(...conversion.diagnostics);
        addLegacyExitLayerQuickFixes([homeDocument, targetDocument]);
        const diagnostic = diagnosticsWithQuickFixes(targetDocument, knownActions, true).find((candidate) => candidate.code === "legacy.zone.exit.context")!;

        expect(diagnostic.fixes?.map((fix) => fix.label)).toEqual(["Make Metronome a linked Layer", "Use GoHome instead"]);
        const goHome = applyQuickFix(conversion.source, targetPath, knownActions, { diagnostic, fix: diagnostic.fixes![1] });
        expect(goHome.source).toContain("Back GoHome");

        const layer = applyQuickFixSet([{ path: targetPath, source: conversion.source }, { path: homePath, source: homeSource }], knownActions, { diagnostic, fix: diagnostic.fixes![0] });
        expect(layer.changes.find((change) => change.path === homePath)?.source).toContain("ZoneLayers {\n  Metronome\n}");
        expect(layer.changes.find((change) => change.path === homePath)?.source).toContain("Button EnterZoneLayer Metronome");
        expect(layer.changes.find((change) => change.path === targetPath)?.source).toContain("@Meta { Version=2 Role=Layer }");
        expect(layer.changes.find((change) => change.path === targetPath)?.source).toContain("Back ExitZoneLayer");
    });

    test("moves a named Bank binding to the Zone that owns its context", () => {
        const sourcePath = "Zones/User/test/Main/LinkLock.zon";
        const destinationPath = "Zones/User/test/Main/SelectedTrackFXMenu.zon";
        const source = "@Meta { Version=2 Role=Layer }\nPrev Bank SelectedTrackFXMenu -1\n";
        const destination = "@Meta { Version=2 Target=SelectedTrack BankTarget=FX }\nPlay Play\n";
        const diagnostic = { code: "legacy.zone.bank.context", line: 2, message: "Wrong Bank context", path: sourcePath, severity: "error" as const };
        const sourceDocument = parseByPath(source, sourcePath, knownActions);
        const destinationDocument = parseByPath(destination, destinationPath, knownActions);
        sourceDocument.diagnostics.push(diagnostic);
        addLegacyBankContextQuickFixes([sourceDocument, destinationDocument]);
        const decorated = diagnosticsWithQuickFixes(sourceDocument, knownActions, true).find((candidate) => candidate.code === diagnostic.code)!;
        const fix = decorated.fixes!.find((candidate) => candidate.id === "zone.bank.move-to-context")!;
        const result = applyQuickFixSet([{ path: sourcePath, source }, { path: destinationPath, source: destination }], knownActions, { diagnostic: decorated, fix });

        expect(decorated.related).toEqual([{ line: 1, path: destinationPath }]);
        expect(result.changes.find((change) => change.path === sourcePath)?.source).not.toContain("Bank SelectedTrackFXMenu");
        expect(result.changes.find((change) => change.path === destinationPath)?.source).toContain("Prev Bank -1");
    });

    test("moves only the Bank binding selected by its diagnostic line", () => {
        const sourcePath = "Zones/User/test/Main/LinkLock.zon";
        const destinationPath = "Zones/User/test/Main/SelectedTrackFXMenu.zon";
        const source = "@Meta { Version=2 Role=Layer }\nPrev Bank SelectedTrackFXMenu -1\nNext Bank SelectedTrackFXMenu 1\n";
        const destination = "@Meta { Version=2 Target=SelectedTrack BankTarget=FX }\n";
        const diagnostic = { code: "legacy.zone.bank.context", line: 2, message: "Wrong Bank context", path: sourcePath, severity: "error" as const };
        const sourceDocument = parseByPath(source, sourcePath, knownActions);
        const destinationDocument = parseByPath(destination, destinationPath, knownActions);
        sourceDocument.diagnostics.push(diagnostic);
        addLegacyBankContextQuickFixes([sourceDocument, destinationDocument]);
        const decorated = diagnosticsWithQuickFixes(sourceDocument, knownActions, true).find((candidate) => candidate.code === diagnostic.code)!;
        const fix = decorated.fixes!.find((candidate) => candidate.id === "zone.bank.move-to-context")!;
        const result = applyQuickFixSet([{ path: sourcePath, source }, { path: destinationPath, source: destination }], knownActions, { diagnostic: decorated, fix });

        expect(result.changes.find((change) => change.path === sourcePath)?.source).toContain("Next Bank SelectedTrackFXMenu 1");
        expect(result.changes.find((change) => change.path === sourcePath)?.source).not.toContain("Prev Bank SelectedTrackFXMenu -1");
        expect(result.changes.find((change) => change.path === destinationPath)?.source).toContain("Prev Bank -1");
        expect(result.changes.find((change) => change.path === destinationPath)?.source).not.toContain("Next Bank 1");
    });

    test("can comment out a duplicate legacy Learn FX source", () => {
        const relativePath = "Zones/User/test/LearnFX.fxzon";
        const source = "Zone FXWidgetLayout\nButton TrackPan\nZoneEnd\n";
        const diagnostic = { code: "legacy.learn-fx.source.duplicate", message: "Duplicate Learn FX file", path: relativePath, severity: "error" as const };
        const document = parseByPath(source, relativePath, knownActions);
        const decorated = diagnosticWithQuickFixes(document, diagnostic, knownActions, true);
        const fix = decorated.fixes!.find((candidate) => candidate.id === "legacy.learn-fx.duplicate.comment-out")!;

        expect(applyQuickFix(source, relativePath, knownActions, { diagnostic, fix }).source).toBe("// Zone FXWidgetLayout\n// Button TrackPan\n// ZoneEnd\n");
    });

    test("moves a legacy named Bank target into format 2 metadata", () => {
        const relativePath = "Zones/User/test/Main/SelectedTrack.zon";
        const source = "@Meta { Version=2 Target=SelectedTrack }\nPrev Bank SelectedTrackFXMenu -1\nNext Bank SelectedTrackFXMenu 1\n";
        const firstDocument = parseByPath(source, relativePath, knownActions);
        const firstDiagnostic = diagnosticsWithQuickFixes(firstDocument, knownActions, true).find((candidate) => candidate.code === "format2.zone.action.bank-amount" && candidate.line === 2)!;
        const firstResult = applyQuickFix(source, relativePath, knownActions, { diagnostic: firstDiagnostic, fix: firstDiagnostic.fixes![0] });
        const secondDiagnostic = diagnosticsWithQuickFixes(firstResult.document, knownActions, true).find((candidate) => candidate.code === "format2.zone.action.bank-amount" && candidate.line === 3)!;
        const secondResult = applyQuickFix(firstResult.source, relativePath, knownActions, { diagnostic: secondDiagnostic, fix: secondDiagnostic.fixes![0] });

        expect(secondResult.source).toBe("@Meta { Version=2 Target=SelectedTrack BankTarget=FX }\nPrev Bank -1\nNext Bank 1\n");
        expect(secondResult.document.diagnostics.filter((diagnostic) => diagnostic.severity === "error")).toEqual([]);
    });
});
