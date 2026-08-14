import { createHash, randomUUID } from "node:crypto";
import { mkdir, readFile, readdir, rename, unlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

export interface ConfigurationDraft {
    originalHash: string;
    path: string;
    source: string;
    updatedAt: string;
}

interface StoredConfigurationDraft extends ConfigurationDraft {
    productRoot: string;
    version: 1;
}

export class ConfigurationDraftStore {
    private readonly draftRoot: string;
    private readonly productRoot: string;

    constructor(productId: string, productRoot: string, temporaryRoot = os.tmpdir()) {
        this.draftRoot = path.join(temporaryRoot, `${productId}-conf-editor`, "drafts");
        this.productRoot = path.resolve(productRoot);
    }

    async discard(relativePath: string): Promise<void> {
        await this.removeFileIfPresent(this.draftPath(relativePath));
    }

    async list(): Promise<ConfigurationDraft[]> {
        let fileNames: string[];
        try {
            fileNames = await readdir(this.draftRoot);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") return [];
            throw error;
        }
        const drafts: ConfigurationDraft[] = [];
        for (const fileName of fileNames.filter((candidate) => candidate.endsWith(".json"))) {
            const stored = await this.readStoredDraft(path.join(this.draftRoot, fileName));
            if (!stored || stored.productRoot !== this.productRoot || fileName !== `${this.draftKey(stored.path)}.json`) continue;
            drafts.push({ originalHash: stored.originalHash, path: stored.path, source: stored.source, updatedAt: stored.updatedAt });
        }
        return drafts.sort((left, right) => left.path.localeCompare(right.path));
    }

    async read(relativePath: string): Promise<ConfigurationDraft | undefined> {
        const stored = await this.readStoredDraft(this.draftPath(relativePath));
        if (!stored || stored.productRoot !== this.productRoot || stored.path !== relativePath) return undefined;
        return { originalHash: stored.originalHash, path: stored.path, source: stored.source, updatedAt: stored.updatedAt };
    }

    async write(relativePath: string, originalHash: string, source: string): Promise<ConfigurationDraft> {
        const draft: StoredConfigurationDraft = { originalHash, path: relativePath, productRoot: this.productRoot, source, updatedAt: new Date().toISOString(), version: 1 };
        await mkdir(this.draftRoot, { recursive: true, mode: 0o700 });
        const targetPath = this.draftPath(relativePath);
        const stagedPath = path.join(this.draftRoot, `.${path.basename(targetPath)}.${randomUUID()}.tmp`);
        try {
            await writeFile(stagedPath, JSON.stringify(draft), { encoding: "utf8", flag: "wx", mode: 0o600 });
            await rename(stagedPath, targetPath);
        } catch (error) {
            await this.removeFileIfPresent(stagedPath);
            throw error;
        }
        return { originalHash, path: relativePath, source, updatedAt: draft.updatedAt };
    }

    private draftKey(relativePath: string): string {
        const logicalPath = path.join(this.productRoot, ...relativePath.split("/"));
        return createHash("sha256").update(process.platform === "win32" ? logicalPath.toLowerCase() : logicalPath).digest("hex");
    }

    private draftPath(relativePath: string): string {
        return path.join(this.draftRoot, `${this.draftKey(relativePath)}.json`);
    }

    private async readStoredDraft(filePath: string): Promise<StoredConfigurationDraft | undefined> {
        try {
            const value = JSON.parse(await readFile(filePath, "utf8")) as Partial<StoredConfigurationDraft>;
            if (value.version !== 1 || typeof value.originalHash !== "string" || typeof value.path !== "string" || typeof value.productRoot !== "string" || typeof value.source !== "string" || typeof value.updatedAt !== "string") return undefined;
            return value as StoredConfigurationDraft;
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT" || error instanceof SyntaxError) return undefined;
            throw error;
        }
    }

    private async removeFileIfPresent(filePath: string): Promise<void> {
        try {
            await unlink(filePath);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
        }
    }
}
