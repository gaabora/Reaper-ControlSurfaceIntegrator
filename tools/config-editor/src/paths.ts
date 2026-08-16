import { readdir, realpath, stat } from "node:fs/promises";
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

export async function discoverReaperDataPaths(explicitPath?: string, rememberedPath?: string): Promise<ReaperDataPathCandidate[]> {
    const windowsAppData = process.env.APPDATA ?? path.join(os.homedir(), "AppData", "Roaming");
    const linuxConfig = process.env.XDG_CONFIG_HOME ?? path.join(os.homedir(), ".config");
    const candidates: Array<{ path: string | undefined; source: TranslationKey }> = [
        { path: explicitPath, source: "candidate.commandLine" },
        { path: rememberedPath, source: "candidate.remembered" },
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
            exists = (await stat(absolutePath)).isDirectory();
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
        const stats = await stat(selectedRoot);
        if (!stats.isDirectory()) throw new ProductPathError("Configuration folder is not a directory");
        const resolvedRoot = path.resolve(selectedRoot);
        if (path.basename(resolvedRoot) !== identity.resourceDirectory) throw new ProductPathError(`Selected directory must be named ${identity.resourceDirectory}`);
        const entries = new Set((await readdir(resolvedRoot)).map((name) => name));
        if (!entries.has(identity.configFilename) && ![...TOP_LEVEL_DIRECTORIES].some((name) => entries.has(name))) throw new ProductPathError("The app configuration folder is empty or invalid");
        return new ProductRootGuard(identity, resolvedRoot);
    }

    static async createFromReaperDataPath(reaperDataPath: string, identity: EditorProductIdentity): Promise<ProductRootGuard> {
        if (!path.isAbsolute(reaperDataPath)) throw new ProductPathError("REAPER data path must be an absolute path");
        let stats: Awaited<ReturnType<typeof lstat>>;
        try {
            stats = await stat(reaperDataPath);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") throw new ProductPathError("REAPER data path does not exist");
            throw error;
        }
        if (!stats.isDirectory()) throw new ProductPathError("REAPER data path is not a directory");
        const resolvedDataPath = path.resolve(reaperDataPath);
        const configurationPath = path.join(resolvedDataPath, identity.resourceDirectory);
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
        if (!isContainedPath(this.root, path.resolve(candidate))) throw new ProductPathError(`Configuration path escapes the app configuration folder: ${relativePath}`);
        return realpath(candidate);
    }

    async resolveForWrite(relativePath: string): Promise<string> {
        const segments = relativeSegments(relativePath);
        const candidate = path.join(this.root, ...segments);
        await this.rejectCaseConflicts(segments);
        if (!isContainedPath(this.root, path.resolve(candidate))) throw new ProductPathError(`Configuration path escapes the app configuration folder: ${relativePath}`);
        return this.resolveWriteTarget(candidate);
    }

    async listTree(): Promise<ProductTreeEntry[]> {
        const entries: ProductTreeEntry[] = [];
        try {
            const configPath = await this.resolveExisting(this.identity.configFilename);
            if ((await stat(configPath)).isFile()) entries.push({ kind: "file", name: this.identity.configFilename, path: this.identity.configFilename, type: "product-config", writable: true });
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") entries.push({ kind: "blocked", name: this.identity.configFilename, path: this.identity.configFilename, reason: error instanceof Error ? error.message : String(error) });
        }
        for (const directoryName of [...TOP_LEVEL_DIRECTORIES].sort()) {
            const directoryPath = path.join(this.root, directoryName);
            try {
                const stats = await stat(directoryPath);
                if (stats.isDirectory()) entries.push(await this.listDirectory(directoryName, new Set()));
            } catch (error) {
                if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
            }
        }
        return entries;
    }

    private async listDirectory(relativeDirectory: string, ancestorDirectories: Set<string>): Promise<ProductTreeEntry> {
        const absoluteDirectory = path.join(this.root, ...relativeDirectory.split("/"));
        const canonicalDirectory = await realpath(absoluteDirectory);
        if (ancestorDirectories.has(canonicalDirectory)) return { kind: "blocked", name: path.posix.basename(relativeDirectory), path: relativeDirectory, reason: "Directory link cycle" };
        const currentAncestors = new Set(ancestorDirectories);
        currentAncestors.add(canonicalDirectory);
        const children: ProductTreeEntry[] = [];
        for (const entry of (await readdir(absoluteDirectory, { withFileTypes: true })).sort((left, right) => left.name.localeCompare(right.name))) {
            const relativePath = `${relativeDirectory}/${entry.name}`;
            const entryPath = path.join(absoluteDirectory, entry.name);
            let entryStats: Awaited<ReturnType<typeof stat>>;
            try {
                entryStats = await stat(entryPath);
            } catch (error) {
                if ((error as NodeJS.ErrnoException).code === "ENOENT") {
                    children.push({ kind: "blocked", name: entry.name, path: relativePath, reason: "Link target is unavailable" });
                    continue;
                }
                throw error;
            }
            if (entryStats.isDirectory()) children.push(await this.listDirectory(relativePath, currentAncestors));
            else if (entryStats.isFile()) {
                const info = classifyConfigPath(relativePath, this.identity);
                if (info) children.push({ kind: "file", name: entry.name, path: relativePath, type: info.kind, writable: info.writable });
            }
        }
        return { children, kind: "directory", name: path.posix.basename(relativeDirectory), path: relativeDirectory };
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

    private async resolveWriteTarget(candidate: string): Promise<string> {
        let existingAncestor = candidate;
        const missingSegments: string[] = [];
        while (true) {
            try {
                return path.join(await realpath(existingAncestor), ...missingSegments);
            } catch (error) {
                if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
                const parent = path.dirname(existingAncestor);
                if (parent === existingAncestor) throw error;
                missingSegments.unshift(path.basename(existingAncestor));
                existingAncestor = parent;
            }
        }
    }
}
