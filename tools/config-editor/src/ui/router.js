const EDITOR_ROUTE_KEYS = ["view", "file", "line", "panel"];
const EDITOR_VIEWS = new Set(["edit", "legacy"]);
const EDITOR_PANELS = new Set(["problems", "details"]);

function routeUrl() {
    return new URL(window.location.href);
}

export function readEditorRoute(currentUrl = routeUrl()) {
    const requestedView = currentUrl.searchParams.get("view") || "";
    const view = EDITOR_VIEWS.has(requestedView) ? requestedView : "home";
    const requestedLine = Number(currentUrl.searchParams.get("line"));
    const line = Number.isInteger(requestedLine) && requestedLine > 0 ? requestedLine : undefined;
    const requestedPanel = currentUrl.searchParams.get("panel") || "problems";
    const panel = requestedPanel === "closed" ? "" : EDITOR_PANELS.has(requestedPanel) ? requestedPanel : "problems";
    return { file: view === "edit" ? currentUrl.searchParams.get("file") || "" : "", line: view === "edit" ? line : undefined, panel, view };
}

export function editorRouteUrl(route, currentUrl = routeUrl()) {
    const nextUrl = new URL(currentUrl.href);
    nextUrl.hash = "";
    for (const key of EDITOR_ROUTE_KEYS) nextUrl.searchParams.delete(key);
    if (route.view === "edit") {
        nextUrl.searchParams.set("view", "edit");
        if (route.file) nextUrl.searchParams.set("file", route.file);
        if (Number.isInteger(route.line) && route.line > 0) nextUrl.searchParams.set("line", String(route.line));
        nextUrl.searchParams.set("panel", route.panel || "closed");
    } else if (route.view === "legacy") nextUrl.searchParams.set("view", "legacy");
    return nextUrl;
}

export function updateEditorRoute(route, replace = false) {
    const nextUrl = editorRouteUrl(route);
    if (nextUrl.href === window.location.href) return;
    const method = replace ? "replaceState" : "pushState";
    window.history[method]({ editorRoute: route }, "", nextUrl);
}

export function onEditorRouteChange(listener) {
    const handlePopState = () => listener(readEditorRoute());
    window.addEventListener("popstate", handlePopState);
    return () => window.removeEventListener("popstate", handlePopState);
}
