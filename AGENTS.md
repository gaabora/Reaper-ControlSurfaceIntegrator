# AGENTS.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

- write in ASD-STE100 Simplified Technical English
- speak in simplified english please, so even a foreigner without tech background can understand what you mean, without fancy metaphors but using simple practical examples
- no newlines between params in methods and conditions if line <250 symbols
- Never use one-letter variable names. Use `idx` for index variables.
- Use `this` when accessing class members from class methods.
- never ever use Em dash `—` but use `-` instead
- in C++ never create nameless namespace { ... } in the middle of code, use coresponding helper files
- Before you change code or documentation, briefly describe the planned changes. Make changes only after approval.
- do not build anything and run tests until asked, and no need to notify about that it in every response
- no need for backward compatibility, app is not yet released
- when writing md plans, mark incomplete items/stages/phases with `[ ]` so user can easily find them using global search
- when implementing tasks from md todo/fixme/plan document, mark completed sections or steps with ✅ emoji, and when the entire doc is completed, make its main header start with "# ✅ COMPLETED: "
- do not do staging-unstaging/commiting until asked, user manages git himself. never push changes
- never hard-wrap text at a fixed line width
- never delete in code todo/fixme comments until they were implemented/fixed
- if summarizing a completed work iteration, also say very briefly what will be the next step/stage to complete and if something is required from user (like review unclear decisions in doc) then say what is expected to be done by user now
- never delete files/folders but rather move them to ./.deleted/ folder in workspace root

Read `DOX.md` before editing. This file is the repository-wide contract.

# ReaControlSurface Agent Guide

Read `DOX.md` before editing. This file is the repository-wide contract.

## Purpose

- Build and maintain the cross-platform Control Surface Integrator (CSI) extension for REAPER.
- Keep the C++ plugin, bundled ReaScripts, and user documentation aligned with the configuration and ExtState contracts they share.

## Ownership

- Root build and packaging files: `CMakeLists.txt`, `config.h.in`, `CMakeSettings.json`, and `Makefile`.
- Root landing documentation plus repository-wide architecture/reference material in `docs/` and active engineering backlog material in `todo/`.
- Development configuration resources in `resources/`, with local contracts in `resources/AGENTS.md`.
- Project-wide engineering rules and the top-level DOX hierarchy.

## Local Contracts

- CMake is the supported build system; `Makefile` is legacy compatibility material unless a task explicitly targets it.
- The plugin target is `ControlSurfaceIntegrator`, emitted as `reaper_csurf_integrator`.
- `Scripts/product_identity.conf` is the only source for public product names, stable product action IDs, install paths, and the release repository URL. Lua and ReaPack tooling read it directly; CMake validates it and generates values for C++, TypeScript, CI, and CPack.
- `Scripts/settings_schema.conf` is the only source for setting names, types, compiled defaults, ranges, scopes, categories, and cross-setting constraints. It currently contains input behavior and timing metadata and can later contain other product configuration metadata. Lua and the Bun editor read the metadata directly; CMake validates it and generates C++ metadata. User-selected values belong in the product INI, not in this schema.
- Current configuration lives under `Data/ReaControlSurface/` in the REAPER resource directory. Runtime code must use `ProductPaths` instead of constructing product paths manually.
- Developers may manually link `resources/Surfaces`, `resources/Zones`, and `resources/Snippets` into the local product root, and link `Scripts/` into `REAPER/Scripts/<ProductScriptDirectory>`, with the commands in `Readme.md`. Build and install steps must not create these development links. Install and ReaPack packages use normal files and directories.
- Treat REAPER APIs, WDL/SWELL, MIDI/OSC protocols, zone files, surface templates, and Lua ExtState keys as external or cross-component contracts.
- Do not index or create DOX files under `.git`, `.github`, `.vscode`, `build`, `cmake`, or `lib`.
- Preserve user changes in the working tree and keep generated build output out of source ownership docs.

## Work Guidance

### C++

- No newlines between params in methods and conditions if line <200 symbols.
- Never use one-letter variable names. Use `idx` for index variables.
- Use `this->` when accessing class members from class methods.
- Keep the project on C++17 and follow the existing WDL/SWELL portability patterns.
- Maintain the precompiled-header exceptions for `src/main.cpp` and `src/controls/integrator.cpp`.

### Documentation

- Verify behavioral claims against current source before updating reference documentation.
- Keep `Wiki/`, `docs/`, and `todo/` navigation consistent when public behavior or backlog structure changes.

## Verification

- Configure and build with the repository CMake presets/settings appropriate to the platform.
- On Windows, the existing local build can be checked with `cmake --build build --config Debug`.
- There is no automated unit-test suite in the included source tree; runtime changes require focused manual verification in REAPER.
- Documentation-only changes should at minimum check links, paths, and DOX index coverage.

## Child DOX Index

- `Scripts/AGENTS.md` - REAPER Lua scripts and their shared runtime contracts.
- `src/AGENTS.md` - C++ plugin source, resources, and component boundaries.
- `Wiki/AGENTS.md` - User-facing wiki pages, navigation, and documentation roadmap.
- `docs/AGENTS.md` - Developer-facing reference documentation.
- `resources/AGENTS.md` - Development surface, zone, and snippet resources.
- `todo/AGENTS.md` - Active implementation backlog and unfinished plans.
- `tools/AGENTS.md` - Developer, release, and future configuration editor tools.
