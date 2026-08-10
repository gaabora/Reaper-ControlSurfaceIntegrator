import { readFile } from "node:fs/promises";

export interface EditorProductIdentity {
    configFilename: string;
    displayName: string;
    packagePrefix: string;
    productId: string;
    resourceDirectory: string;
}

const REQUIRED_KEYS = ["PRODUCT_CONFIG_FILENAME", "PRODUCT_DISPLAY_NAME", "PRODUCT_PACKAGE_PREFIX", "PRODUCT_ID", "PRODUCT_RESOURCE_DIRECTORY"] as const;

export function parseProductIdentity(source: string): EditorProductIdentity {
    const values = new Map<string, string>();
    for (const [lineIdx, rawLine] of source.split(/\r?\n/).entries()) {
        const line = rawLine.trim();
        if (!line || line.startsWith("#")) continue;
        const separator = line.indexOf("=");
        if (separator <= 0) throw new Error(`Invalid product identity line ${lineIdx + 1}: ${line}`);
        const key = line.slice(0, separator);
        const value = line.slice(separator + 1);
        if (values.has(key)) throw new Error(`Duplicate product identity key: ${key}`);
        values.set(key, value);
    }
    for (const key of REQUIRED_KEYS) if (!values.get(key)) throw new Error(`Missing product identity value: ${key}`);
    for (const key of ["PRODUCT_CONFIG_FILENAME", "PRODUCT_PACKAGE_PREFIX", "PRODUCT_RESOURCE_DIRECTORY"] as const) {
        const value = values.get(key) ?? "";
        if (value === "." || value === ".." || value.includes("/") || value.includes("\\")) throw new Error(`${key} must be one path component`);
    }
    const productId = values.get("PRODUCT_ID") ?? "";
    if (!/^[a-z0-9][a-z0-9_-]*$/.test(productId)) throw new Error("PRODUCT_ID must be a stable lowercase ASCII ID");
    return {
        configFilename: values.get("PRODUCT_CONFIG_FILENAME") ?? "",
        displayName: values.get("PRODUCT_DISPLAY_NAME") ?? "",
        packagePrefix: values.get("PRODUCT_PACKAGE_PREFIX") ?? "",
        productId,
        resourceDirectory: values.get("PRODUCT_RESOURCE_DIRECTORY") ?? "",
    };
}

export async function loadProductIdentity(identityPath: string): Promise<EditorProductIdentity> {
    return parseProductIdentity(await readFile(identityPath, "utf8"));
}
