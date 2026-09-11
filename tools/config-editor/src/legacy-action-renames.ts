export interface LegacyActionRenameContext {
    isLayer: boolean;
}

export interface LegacyActionRenameDefinition {
    legacyAction: string;
    legacyFirstArgument?: string;
    newAction: string;
    removeFirstArgument?: boolean;
    requiresLayer?: boolean;
}

export interface LegacyActionRenameResult {
    action: string;
    arguments: string[];
}

export const LEGACY_ACTION_RENAMES: readonly LegacyActionRenameDefinition[] = [
    { legacyAction: "FixedTexDislay", newAction: "FixedTextDisplay" },
    { legacyAction: "MCUTrackPan", newAction: "TrackPan" },
    { legacyAction: "Raper", newAction: "Reaper" },
    { legacyAction: "TackMute", newAction: "TrackMute" },
    { legacyAction: "TackPan", newAction: "TrackPan" },
    { legacyAction: "GoZone", legacyFirstArgument: "Home", newAction: "GoHome", removeFirstArgument: true },
    { legacyAction: "GoZone", legacyFirstArgument: "SelectedTrackFX", newAction: "ToggleSelectedTrackFX", removeFirstArgument: true },
    { legacyAction: "GoSelectedTrackFX", newAction: "ToggleSelectedTrackFX" },
    { legacyAction: "GoSubZone", newAction: "EnterZoneLayer" },
    { legacyAction: "LeaveSubZone", newAction: "ExitZoneLayer", requiresLayer: true },
    { legacyAction: "SelectedTrackBank", newAction: "Bank" },
    { legacyAction: "TrackReceiveBank", newAction: "Bank" },
    { legacyAction: "TrackSendBank", newAction: "Bank" },
];

export const LEGACY_IGNORED_ACTIONS: ReadonlySet<string> = new Set(["NoFeedback", "NullDisplay"]);

export function isIgnoredLegacyAction(action: string): boolean {
    return LEGACY_IGNORED_ACTIONS.has(action);
}

export function renameLegacyAction(action: string, actionArguments: string[], context: LegacyActionRenameContext): LegacyActionRenameResult {
    const definition = LEGACY_ACTION_RENAMES.find((candidate) => candidate.legacyAction === action
        && (!candidate.legacyFirstArgument || candidate.legacyFirstArgument.toLowerCase() === actionArguments[0]?.toLowerCase())
        && (!candidate.requiresLayer || context.isLayer));
    if (!definition) return { action, arguments: actionArguments };
    return { action: definition.newAction, arguments: definition.removeFirstArgument ? actionArguments.slice(1) : actionArguments };
}

export function missingLegacyActionRenameDestinations(knownActions: ReadonlySet<string>): string[] {
    return [...new Set(LEGACY_ACTION_RENAMES.map((definition) => definition.newAction).filter((action) => !knownActions.has(action)))].sort();
}
