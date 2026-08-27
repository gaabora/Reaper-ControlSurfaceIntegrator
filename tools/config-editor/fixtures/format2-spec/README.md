# Format 2 specification fixtures

These fixtures are the executable examples for `todo/ZONE_FORMAT_V2_PLAN.md`.

`valid/` and `invalid/` contain one representative document for every format 2 top-level document type. `golden/` contains legacy input, expected format 2 output, and expected unresolved diagnostics for migration decisions.

The current format 1 validator must not load this directory. Phase 3 and Phase 4 will connect these files to the shared format 2 parser and migration tests. A fixture becomes normative only when its syntax is also described in the plan.

Fixture rules:

- Keep one logical error per invalid fixture when practical.
- Keep related Main zones together because references require a complete profile index.
- Keep each migration case in its own directory with `legacy/`, `expected/`, and optional `diagnostics.txt` entries.
- Add a golden case whenever the conversion matrix gains a new branch.
