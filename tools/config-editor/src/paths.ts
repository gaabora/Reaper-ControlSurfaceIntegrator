import { lstat, readdir, realpath } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import type { TranslationKey } from "./i18n.ts";
import type { EditorProductIdentity } from "./product-identity.ts";

export type ConfigOwner = "built-in" | "product" | "user" | "vendor";
export type ConfigKind = "product-config" | "snippet" | "surface" | "zone";

export interface ConfigPathInfo {
    kind: ConfigKind;
    owner: ConfigOwner;
    writable: boolean;
}

export interface ReaperDataPathCandidate {
    exists: boolean;
    path: string;
    source: TranslationKey;
}

export interface ProductTreeEntry {
    children?: ProductTreeEntry[];
    kind: "blocked" | "directory" | "file";
    name: string;
    path: string;
    reason?: string;
    type?: ConfigKind;
    writable?: boolean;
}

const TOP_LEVEL_DIRECTORIES = new Set(["Snippets", "Surfaces", "Zones"]);

export class ProductPathError extends Error {
    constructor(message: string) {
        super(message);
        this.name = "ProductPathError";
    }
}

function isContainedPath(root: string, candidate: string): boolean {
    const relativePath = path.relative(root, candidate);
    return relativePath === "" || (!relativePath.startsWith(`..${path.sep}`) && relativePath !== ".." && !path.isAbsolute(relativePath));
}

function relativeSegments(relativePath: string): string[] {
    if (!relativePath || relativePath.includes("\\") || path.posix.isAbsolute(relativePath)) throw new ProductPathError("Configuration path must be a non-empty relative path with '/' separators");
    const segments = relativePath.split("/");
    if (segments.some((segment) => !segment || segment === "." || segment === "..")) throw new ProductPathError(`Unsafe configuration path: ${relativePath}`);
    return segments;
}

function isStableId(value: string): boolean {
    return /^[a-z0-9][a-z0-9_-]*$/.test(value);
}

export function classifyConfigPath(relativePath: string, identity: EditorProductIdentity): ConfigPathInfo | undefined {
    if (relativePath === identity.configFilename) return { kind: "product-config", owner: "product", writable: true };
    const segments = relativeSegments(relativePath);
    const surfaceId = segments[2]?.endsWith(".txt") ? segments[2].slice(0, -4) : "";
    const snippetId = segments[2]?.endsWith(".snippet") ? segments[2].slice(0, -8) : "";
    if (segments.length === 3 && segments[0] === "Surfaces" && segments[1] === "Vendor" && isStableId(surfaceId)) return { kind: "surface", owner: "vendor", writable: false };
    if (segments.length === 3 && segments[0] === "Surfaces" && segments[1] === "User" && isStableId(surfaceId)) return { kind: "surface", owner: "user", writable: true };
    if (segments.length >= 4 && segments[0] === "Zones" && segments[1] === "Vendor" && isStableId(segments[2]) && segments.at(-1)?.endsWith(".zon")) return { kind: "zone", owner: "vendor", writable: false };
    if (segments.length >= 4 && segments[0] === "Zones" && segments[1] === "User" && isStableId(segments[2]) && segments.at(-1)?.endsWith(".zon")) return { kind: "zone", owner: "user", writable: true };
    if (segments.length === 3 && segments[0] === "Snippets" && segments[1] === "BuiltIn" && isStableId(snippetId)) return { kind: "snippet", owner: "built-in", writable: false };
    if (segments.length === 3 && segments[0] === "Snippets" && segments[1] === "User" && isStableId(snippetId)) return { kind: "snippet", owner: "user", writable: true };
    return undefined;
}

export async function discoverReaperDataPaths(explicitPath?: string): Promise<ReaperDataPathCandidate[]> {
    const windowsAppData = process.env.APPDATA ?? path.join(os.homedir(), "AppData", "Roaming");
    const linuxConfig = process.env.XDG_CONFIG_HOME ?? path.join(os.homedir(), ".config");
    const candidates: Array<{ path: string | undefined; source: TranslationKey }> = [
        { path: explicitPath, source: "candidate.commandLine" },
        { path: process.env.REAPER_RESOURCE_PATH ? path.join(process.env.REAPER_RESOURCE_PATH, "Data") : undefined, source: "candidate.reaperEnvironment" },
        { path: process.platform === "darwin" ? path.join(os.homedir(), "Library", "Application Support", "REAPER", "Data") : undefined, source: "candidate.macosDefault" },
        { path: process.platform === "win32" ? path.join(windowsAppData, "REAPER", "Data") : undefined, source: "candidate.windowsDefault" },
        { path: process.platform === "linux" ? path.join(linuxConfig, "REAPER", "Data") : undefined, source: "candidate.linuxDefault" },
    ];
    const result: ReaperDataPathCandidate[] = [];
    const seen = new Set<string>();
    for (const candidate of candidates) {
        if (!candidate.path) continue;
        const absolutePath = path.resolve(candidate.path);
        const key = process.platform === "win32" ? absolutePath.toLowerCase() : absolutePath;
        if (seen.has(key)) continue;
        seen.add(key);
        let exists = false;
        try {
            exists = (await lstat(absolutePath)).isDirectory();
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
        }
        result.push({ exists, path: absolutePath, source: candidate.source });
    }
    return result;
}

export class ProductRootGuard {
    private constructor(private readonly identity: EditorProductIdentity, private readonly root: string) {}

    static async create(selectedRoot: string, identity: EditorProductIdentity): Promise<ProductRootGuard> {
        if (!path.isAbsolute(selectedRoot)) throw new ProductPathError("Configuration folder must be an absolute path");
        const stats = await lstat(selectedRoot);
        if (!stats.isDirectory()) throw new ProductPathError("Configuration folder is not a directory");
        const canonicalRoot = await realpath(selectedRoot);
        if (path.basename(canonicalRoot) !== identity.resourceDirectory) throw new ProductPathError(`Selected directory must be named ${identity.resourceDirectory}`);
        const entries = new Set((await readdir(canonicalRoot)).map((name) => name));
        if (!entries.has(identity.configFilename) && ![...TOP_LEVEL_DIRECTORIES].some((name) => entries.has(name))) throw new ProductPathError("The app configuration folder is empty or invalid");
        return new ProductRootGuard(identity, canonicalRoot);
    }

    static async createFromReaperDataPath(reaperDataPath: string, identity: EditorProductIdentity): Promise<ProductRootGuard> {
        if (!path.isAbsolute(reaperDataPath)) throw new ProductPathError("REAPER data path must be an absolute path");
        let stats: Awaited<ReturnType<typeof lstat>>;
        try {
            stats = await lstat(reaperDataPath);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") throw new ProductPathError("REAPER data path does not exist");
            throw error;
        }
        if (!stats.isDirectory()) throw new ProductPathError("REAPER data path is not a directory");
        const canonicalDataPath = await realpath(reaperDataPath);
        const configurationPath = path.join(canonicalDataPath, identity.resourceDirectory);
        try {
            return await ProductRootGuard.create(configurationPath, identity);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") throw new ProductPathError(`Could not find ${identity.resourceDirectory} in the selected REAPER data path`);
            throw error;
        }
    }

    getRoot(): string {
        return this.root;
    }

    getReaperDataPath(): string {
        return path.dirname(this.root);
    }

    getIdentity(): EditorProductIdentity {
        return this.identity;
    }

    getPathInfo(relativePath: string): ConfigPathInfo {
        const info = classifyConfigPath(relativePath, this.identity);
        if (!info) throw new ProductPathError(`Unsupported configuration path: ${relativePath}`);
        return info;
    }

    async resolveExisting(relativePath: string): Promise<string> {
        const segments = relativeSegments(relativePath);
        const candidate = path.join(this.root, ...segments);
        await this.rejectCaseConflicts(segments);
        await this.rejectSymlinks(segments, true);
        const canonicalPath = await realpath(candidate);
        if (!isContainedPath(this.root, canonicalPath)) throw new ProductPathError(`Configuration path escapes the app configuration folder: ${relativePath}`);
        return canonicalPath;
    }

    async resolveForWrite(relativePath: string): Promise<string> {
        const segments = relativeSegments(relativePath);
        const candidate = path.join(this.root, ...segments);
        await this.rejectCaseConflicts(segments);
        await this.rejectSymlinks(segments, false);
        if (!isContainedPath(this.root, path.resolve(candidate))) throw new ProductPathError(`Configuration path escapes the app configuration folder: ${relativePath}`);
        return candidate;
    }

    async listTree(): Promise<ProductTreeEntry[]> {
        const entries: ProductTreeEntry[] = [];
        try {
            const configPath = await this.resolveExisting(this.identity.configFilename);
            if ((await lstat(configPath)).isFile()) entries.push({ kind: "file", name: this.identity.configFilename, path: this.identity.configFilename, type: "product-config", writable: true });
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") entries.push({ kind: "blocked", name: this.identity.configFilename, path: this.identity.configFilename, reason: error instanceof Error ? error.message : String(error) });
        }
        for (const directoryName of [...TOP_LEVEL_DIRECTORIES].sort()) {
            const directoryPath = path.join(this.root, directoryName);
            try {
                const stats = await lstat(directoryPath);
                if (stats.isSymbolicLink()) entries.push({ kind: "blocked", name: directoryName, path: directoryName, reason: "Symbolic links are not allowed" });
                else if (stats.isDirectory()) entries.push(await this.listDirectory(directoryName));
            } catch (error) {
                if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
            }
        }
        return entries;
    }

    private async listDirectory(relativeDirectory: string): Promise<ProductTreeEntry> {
        const absoluteDirectory = path.join(this.root, ...relativeDirectory.split("/"));
        const children: ProductTreeEntry[] = [];
        for (const entry of (await readdir(absoluteDirectory, { withFileTypes: true })).sort((left, right) => left.name.localeCompare(right.name))) {
            const relativePath = `${relativeDirectory}/${entry.name}`;
            if (entry.isSymbolicLink()) children.push({ kind: "blocked", name: entry.name, path: relativePath, reason: "Symbolic links are not allowed" });
            else if (entry.isDirectory()) children.push(await this.listDirectory(relativePath));
            else if (entry.isFile()) {
                const info = classifyConfigPath(relativePath, this.identity);
                if (info) children.push({ kind: "file", name: entry.name, path: relativePath, type: info.kind, writable: info.writable });
            }
        }
        return { children, kind: "directory", name: path.posix.basename(relativeDirectory), path: relativeDirectory };
    }

    private async rejectSymlinks(segments: string[], requireLeaf: boolean): Promise<void> {
        let currentPath = this.root;
        for (const [segmentIdx, segment] of segments.entries()) {
            currentPath = path.join(currentPath, segment);
            try {
                const stats = await lstat(currentPath);
                if (stats.isSymbolicLink()) throw new ProductPathError(`Symbolic links are not allowed in configuration paths: ${segments.slice(0, segmentIdx + 1).join("/")}`);
            } catch (error) {
                if ((error as NodeJS.ErrnoException).code === "ENOENT" && !requireLeaf) return;
                throw error;
            }
        }
    }

    private async rejectCaseConflicts(segments: string[]): Promise<void> {
        let currentPath = this.root;
        for (const [segmentIdx, segment] of segments.entries()) {
            let names: string[];
            try {
                names = await readdir(currentPath);
            } catch (error) {
                if ((error as NodeJS.ErrnoException).code === "ENOENT") return;
                throw error;
            }
            const matches = names.filter((name) => name.toLowerCase() === segment.toLowerCase());
            if (matches.length > 1 || matches.length === 1 && matches[0] !== segment) throw new ProductPathError(`Configuration path differs only by letter case: ${segments.slice(0, segmentIdx).concat(matches).join("/")}`);
            if (!matches.length) return;
            currentPath = path.join(currentPath, segment);
        }
    }
}
