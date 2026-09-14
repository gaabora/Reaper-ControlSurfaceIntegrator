export interface LegacyActionRenameContext {
    bankTarget?: string;
    isLayer: boolean;
    target?: string;
}

export interface LegacyActionRenameDefinition {
    legacyAction: string;
    legacyFirstArgument?: string;
    newAction: string;
    removeFirstArgument?: boolean;
    requiredBankTarget?: string;
    requiredTarget?: string;
    requiredZoneName?: string;
    requiresLayer?: boolean;
}

export interface LegacyActionRenameResult {
    action: string;
    arguments: string[];
}

export interface LegacyActionConfirmation {
    legacyAction: string;
    replacementAction: string;
}

export const LEGACY_ACTION_RENAMES: readonly LegacyActionRenameDefinition[] = [
    { legacyAction: "FixedTexDislay", newAction: "FixedTextDisplay" },
    { legacyAction: "MCUTrackPan", newAction: "TrackPan" },
    { legacyAction: "Raper", newAction: "Reaper" },
    { legacyAction: "TackMute", newAction: "TrackMute" },
    { legacyAction: "TackPan", newAction: "TrackPan" },
    { legacyAction: "TracksMute", newAction: "TrackMute" },
    { legacyAction: "TracksSolo", newAction: "TrackSolo" },
    { legacyAction: "ToggleAutoFocusedFXMapping", newAction: "ToggleEnableFocusedFXMapping" },
    { legacyAction: "ToggleAutoFXMapping", newAction: "ToggleEnableFocusedFXMapping" },
    { legacyAction: "GoAssociatedZone", newAction: "GoZone" },
    { legacyAction: "GoZone", legacyFirstArgument: "Home", newAction: "GoHome", removeFirstArgument: true },
    { legacyAction: "GoZone", legacyFirstArgument: "SelectedTrackFX", newAction: "ToggleSelectedTrackFX", removeFirstArgument: true },
    { legacyAction: "GoSelectedTrackFX", newAction: "ToggleSelectedTrackFX" },
    { legacyAction: "GoSubZone", newAction: "EnterZoneLayer" },
    { legacyAction: "LeaveSubZone", newAction: "ExitZoneLayer", requiresLayer: true },
    { legacyAction: "SelectedTrackBank", newAction: "Bank", requiredTarget: "SelectedTrack", requiredZoneName: "SelectedTrack" },
    { legacyAction: "SelectedTrackFXMenuBank", newAction: "Bank", requiredBankTarget: "FX", requiredTarget: "SelectedTrack" },
    { legacyAction: "TrackReceiveBank", newAction: "Bank", requiredBankTarget: "Receives", requiredTarget: "Tracks" },
    { legacyAction: "TrackSendBank", newAction: "Bank", requiredBankTarget: "Sends", requiredTarget: "Tracks" },
    { legacyAction: "ToggleEnableFocusedFXParamMapping", newAction: "ToggleEnableLastTouchedFXParamMapping" },
];

export const LEGACY_IGNORED_ACTIONS: ReadonlySet<string> = new Set(["NoFeedback", "NullDisplay"]);
export const LEGACY_ACTION_CONFIRMATIONS: readonly LegacyActionConfirmation[] = [
    { legacyAction: "AutoLearnFocusedFX", replacementAction: "LearnFocusedFX" },
    { legacyAction: "AutoMapFocusedFX", replacementAction: "LearnFocusedFX" },
];
export const LEGACY_UNSUPPORTED_ACTIONS: ReadonlySet<string> = new Set([
    "DumpHex",
    "EraseLastTouchedControl",
    "MetronomePrimaryVolume",
    "MetronomePrimaryVolumeDisplay",
    "MetronomeSecondaryVolume",
    "MetronomeSecondaryVolumeDisplay",
    "RemapAutoZone",
    "SaveLearnedFXParams",
]);

export function isIgnoredLegacyAction(action: string): boolean {
    return LEGACY_IGNORED_ACTIONS.has(action);
}

export function legacyActionConfirmation(action: string): LegacyActionConfirmation | undefined {
    return LEGACY_ACTION_CONFIRMATIONS.find((candidate) => candidate.legacyAction === action);
}

export function isUnsupportedLegacyAction(action: string): boolean {
    return LEGACY_UNSUPPORTED_ACTIONS.has(action);
}

export function renameLegacyAction(action: string, actionArguments: string[], context: LegacyActionRenameContext): LegacyActionRenameResult {
    const definition = LEGACY_ACTION_RENAMES.find((candidate) => candidate.legacyAction === action && (!candidate.legacyFirstArgument || candidate.legacyFirstArgument.toLowerCase() === actionArguments[0]?.toLowerCase()) && (!candidate.requiresLayer || context.isLayer));
    if (!definition) return { action, arguments: actionArguments };
    const contextMatches = (!definition.requiredTarget || definition.requiredTarget === context.target) && (!definition.requiredBankTarget || definition.requiredBankTarget === context.bankTarget);
    if (!contextMatches && definition.newAction === "Bank" && definition.requiredTarget && definition.requiredBankTarget) return { action: "Bank", arguments: [...actionArguments, `Target=${definition.requiredTarget}`, `BankTarget=${definition.requiredBankTarget}`] };
    if (!contextMatches) return { action, arguments: actionArguments };
    return { action: definition.newAction, arguments: definition.removeFirstArgument ? actionArguments.slice(1) : actionArguments };
}

export function conflictingLegacyActionRename(action: string, actionArguments: string[], context: LegacyActionRenameContext): LegacyActionRenameDefinition | undefined {
    return LEGACY_ACTION_RENAMES.find((candidate) => candidate.legacyAction === action
        && (!candidate.legacyFirstArgument || candidate.legacyFirstArgument.toLowerCase() === actionArguments[0]?.toLowerCase())
        && (candidate.newAction !== "Bank" || !candidate.requiredBankTarget)
        && (Boolean(candidate.requiredTarget && candidate.requiredTarget !== context.target) || Boolean(candidate.requiredBankTarget && candidate.requiredBankTarget !== context.bankTarget)));
}

export function missingLegacyActionRenameDestinations(knownActions: ReadonlySet<string>): string[] {
    return [...new Set([...LEGACY_ACTION_RENAMES.map((definition) => definition.newAction), ...LEGACY_ACTION_CONFIRMATIONS.map((definition) => definition.replacementAction)].filter((action) => !knownActions.has(action)))].sort();
}
