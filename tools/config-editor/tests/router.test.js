import { describe, expect, test } from "bun:test";
import { editorRouteUrl, readEditorRoute } from "../src/ui/router.js";

describe("editor router", () => {
    test("reads an edit route with a file, line, and panel", () => {
        const route = readEditorRoute(new URL("http://127.0.0.1:47868/?view=edit&file=Zones%2FUser%2Ffp2%2FMain%2FHome.zon&line=27&panel=details"));
        expect(route).toEqual({ file: "Zones/User/fp2/Main/Home.zon", line: 27, panel: "details", view: "edit" });
    });

    test("uses safe defaults for unknown route values", () => {
        const route = readEditorRoute(new URL("http://127.0.0.1:47868/?view=unknown&file=ignored&line=-2&panel=unknown"));
        expect(route).toEqual({ file: "", line: undefined, panel: "problems", view: "home" });
    });

    test("writes only editor parameters and preserves unrelated parameters", () => {
        const currentUrl = new URL("http://127.0.0.1:47868/?keep=yes&view=legacy#section");
        const nextUrl = editorRouteUrl({ file: "Zones/User/fp2/Main/Home.zon", line: 3, panel: "", view: "edit" }, currentUrl);
        expect(nextUrl.searchParams.get("keep")).toBe("yes");
        expect(nextUrl.searchParams.get("view")).toBe("edit");
        expect(nextUrl.searchParams.get("file")).toBe("Zones/User/fp2/Main/Home.zon");
        expect(nextUrl.searchParams.get("line")).toBe("3");
        expect(nextUrl.searchParams.get("panel")).toBe("closed");
        expect(nextUrl.hash).toBe("");
    });

    test("removes editor parameters for the home route", () => {
        const currentUrl = new URL("http://127.0.0.1:47868/?keep=yes&view=edit&file=Surface.txt&line=3&panel=details");
        const nextUrl = editorRouteUrl({ file: "", line: undefined, panel: "problems", view: "home" }, currentUrl);
        expect(nextUrl.search).toBe("?keep=yes");
    });
});
