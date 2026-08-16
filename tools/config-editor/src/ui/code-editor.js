import { defaultKeymap, history, historyKeymap, indentWithTab } from "@codemirror/commands";
import { bracketMatching, HighlightStyle, StreamLanguage, syntaxHighlighting } from "@codemirror/language";
import { highlightSelectionMatches, searchKeymap } from "@codemirror/search";
import { Compartment, EditorState } from "@codemirror/state";
import { drawSelection, dropCursor, EditorView, highlightActiveLine, highlightActiveLineGutter, keymap, lineNumbers, rectangularSelection } from "@codemirror/view";
import { tags } from "@lezer/highlight";

const BLOCK_KEYWORDS = new Set(["Binding", "BindingEnd", "IncludedZones", "IncludedZonesEnd", "OSKLayout", "OSKLayoutEnd", "Snippet", "SnippetEnd", "SubZones", "SubZonesEnd", "Widget", "WidgetEnd", "Zone", "ZoneEnd"]);
const VALUE_KEYWORDS = new Set(["No", "None", "Yes"]);

const configurationLanguage = StreamLanguage.define({
    copyState(state) { return { firstToken: state.firstToken }; },
    startState() { return { firstToken: true }; },
    token(stream, state) {
        if (stream.sol()) state.firstToken = true;
        if (stream.eatSpace()) return null;
        if (stream.match("//")) {
            stream.skipToEnd();
            state.firstToken = false;
            return "comment";
        }
        if (state.firstToken && stream.match(/#(?:WidgetType|DisplayRow|RingStyle|DisplayFont|SupportsColor)\b/)) {
            state.firstToken = false;
            return "keyword";
        }
        if (stream.peek() === '"') {
            stream.next();
            let escaped = false;
            while (!stream.eol()) {
                const character = stream.next();
                if (character === '"' && !escaped) break;
                escaped = character === "\\" && !escaped;
                if (character !== "\\") escaped = false;
            }
            state.firstToken = false;
            return "string";
        }
        if (stream.match(/#[0-9a-fA-F]{6,8}\b/) || stream.match(/[-+]?\d+(?:\.\d+)?\b/)) {
            state.firstToken = false;
            return "number";
        }
        if (stream.match(/[{}\[\]()+|>]/)) {
            state.firstToken = false;
            return "operator";
        }
        if (stream.match(/[A-Za-z_][A-Za-z0-9_-]*/)) {
            const word = stream.current();
            const firstToken = state.firstToken;
            state.firstToken = false;
            if (BLOCK_KEYWORDS.has(word)) return "keyword";
            if (VALUE_KEYWORDS.has(word)) return "atom";
            if (stream.peek() === "=") return "propertyName";
            return firstToken ? "typeName" : "variableName";
        }
        if (stream.match("=")) {
            state.firstToken = false;
            return "operator";
        }
        stream.next();
        state.firstToken = false;
        return null;
    },
});

const configurationHighlightStyle = HighlightStyle.define([
    { color: "#7d8796", fontStyle: "italic", tag: tags.comment },
    { color: "#c792ea", fontWeight: "600", tag: tags.keyword },
    { color: "#82aaff", tag: tags.atom },
    { color: "#c3e88d", tag: tags.string },
    { color: "#f78c6c", tag: tags.number },
    { color: "#82aaff", tag: tags.propertyName },
    { color: "#89ddff", tag: tags.typeName },
    { color: "#d6deeb", tag: tags.variableName },
    { color: "#89ddff", tag: tags.operator },
]);

const configurationEditorTheme = EditorView.theme({
    "&": { color: "#d6deeb" },
    ".cm-content": { caretColor: "#f3f6ff" },
    ".cm-cursor, .cm-dropCursor": { borderLeftColor: "#f3f6ff", borderLeftWidth: "2px" },
    "&.cm-focused .cm-selectionBackground, .cm-selectionBackground, ::selection": { backgroundColor: "#365f91" },
    ".cm-selectionMatch": { backgroundColor: "#49663f" },
    ".cm-selectionMatch.cm-selectionMatch-main": { backgroundColor: "#5d794f" },
}, { dark: true });

export function createConfigurationEditor(parent, onChange) {
    const editable = new Compartment();
    let readOnly = true;
    let suppressChanges = false;
    const createState = (source) => EditorState.create({
        doc: source,
        extensions: [
            lineNumbers(),
            highlightActiveLineGutter(),
            history(),
            drawSelection(),
            dropCursor(),
            EditorState.allowMultipleSelections.of(true),
            configurationLanguage,
            configurationEditorTheme,
            syntaxHighlighting(configurationHighlightStyle),
            bracketMatching(),
            rectangularSelection(),
            highlightActiveLine(),
            highlightSelectionMatches(),
            keymap.of([indentWithTab, ...defaultKeymap, ...searchKeymap, ...historyKeymap]),
            editable.of([EditorState.readOnly.of(readOnly), EditorView.editable.of(!readOnly)]),
            EditorView.updateListener.of((update) => {
                if (update.docChanged && !suppressChanges && onChange) onChange(update.state.doc.toString());
            }),
        ],
    });
    const view = new EditorView({
        parent,
        state: createState(""),
    });

    return {
        getCursorLine() { return view.state.doc.lineAt(view.state.selection.main.head).number; },
        getValue() { return view.state.doc.toString(); },
        goToLine(lineNumber) {
            const line = view.state.doc.line(Math.max(1, Math.min(Number(lineNumber) || 1, view.state.doc.lines)));
            view.dispatch({ effects: EditorView.scrollIntoView(line.from, { y: "center" }), selection: { anchor: line.from } });
            view.focus();
        },
        setReadOnly(nextReadOnly) {
            if (readOnly === nextReadOnly) return;
            readOnly = nextReadOnly;
            view.dispatch({ effects: editable.reconfigure([EditorState.readOnly.of(readOnly), EditorView.editable.of(!readOnly)]) });
        },
        setValue(value, resetState = false) {
            const source = String(value ?? "");
            if (!resetState && source === view.state.doc.toString()) return;
            suppressChanges = true;
            try {
                if (resetState) view.setState(createState(source));
                else view.dispatch({ changes: { from: 0, insert: source, to: view.state.doc.length } });
            } finally {
                suppressChanges = false;
            }
        },
        setVisible(visible) {
            parent.hidden = !visible;
            if (visible) requestAnimationFrame(() => view.requestMeasure());
        },
    };
}
