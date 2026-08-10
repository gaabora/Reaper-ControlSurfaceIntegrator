export const DISPLAY_LOCALE = "en-US";

const english = {
    "action.addToSaveAll": "Add to Save All",
    "action.check": "Check",
    "action.makeEditable": "Make editable copy",
    "action.open": "Open",
    "action.save": "Save",
    "action.saveAll": "Save all",
    "app.title": "{product} Conf Editor",
    "candidate.commandLine": "Command line",
    "candidate.linuxDefault": "Linux default",
    "candidate.macosDefault": "macOS default",
    "candidate.notFound": "{source} - not found",
    "candidate.reaperEnvironment": "REAPER environment",
    "candidate.windowsDefault": "Windows default",
    "details.title": "File details",
    "diagnostic.line": "Line {line}: ",
    "document.editable": "Editable",
    "document.none": "Select a file",
    "document.readOnly": "Read-only",
    "error.browserOpen": "Could not open the browser: {message}",
    "error.configLoad": "Could not load app settings.",
    "error.fixBeforeSave": "Fix the errors before you save.",
    "error.fixBeforeSaveAll": "Fix the errors before you add this file to Save All.",
    "error.missingToken": "The editor session is missing. Start the editor again.",
    "error.request": "The request failed.",
    "error.start": "Could not start the editor: {message}",
    "files.blocked": "Unavailable",
    "files.empty": "Open a REAPER data path to see configuration files.",
    "files.readOnly": "Read-only",
    "files.title": "Files",
    "pending.count": "Files ready to save: {count}",
    "problems.none": "No problems found.",
    "problems.title": "Problems",
    "reaperDataPath.help": "Check the REAPER data path, then click Open.",
    "reaperDataPath.placeholder": "REAPER data path",
    "server.local": "Local server: {url}",
    "server.stop": "Press Ctrl+C to stop.",
    "status.addedToSaveAll": "Added {path} to Save All.",
    "status.checked": "Check complete.",
    "status.openedDataPath": "Opened REAPER data path: {path}",
    "status.openedFile": "Opened {path}",
    "status.ready": "Ready.",
    "status.savedAll": "Saved files: {count}.",
    "status.savedFile": "Saved {path}.",
    "status.userCopyCreated": "Created an editable copy.",
    "tab.guided": "Guided",
    "tab.text": "Text",
} as const;

export type TranslationKey = keyof typeof english;
type TranslationParams = Record<string, number | string>;

export function t(key: TranslationKey, params: TranslationParams = {}): string {
    let text: string = english[key];
    for (const [paramName, paramValue] of Object.entries(params)) text = text.replaceAll(`{${paramName}}`, String(paramValue));
    return text;
}

export function translationCatalog(): Readonly<Record<TranslationKey, string>> {
    return english;
}
