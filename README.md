# site-templating-experiment

An experiment in HTML-pure static site templating.

This project now focuses on a C-based build-time engine that expands inline `def-*`
component definitions into plain browser-ready HTML.

## Current layout

- `src/main.c` + `src/templater/*.c`: modular C engine implementation.
- `docs/component-spec.md`: draft spec the implementation targets.
- `docs/spec-compliance.md`: checklist mapping spec rules to fixtures.
- `demos/site/src`: minimal feature demo.
- `demos/blog/src`: larger Tailwind blog demo (`posts/` holds article pages).
- `demos/recipes/src`: larger Tailwind recipe demo (`recipes/` holds recipe detail pages).
- `generated/<demo>`: built demo output folders.
- `tests/pass`: fixtures with known-correct output.
- `tests/fail`: adversarial fixtures expected to fail.
- `scripts/build.sh`: compile and build a source folder.
- `scripts/build-all-demos.sh`: compile and build every demo under `demos/*/src`.
- `scripts/dev.sh`: rebuild-on-change + local server workflow.
- `scripts/test.sh`: run pass/fail fixture suite.
- `archive/legacy-2024`: previous Rust/C prototype and old sample pages.

## Engine behavior (v1)

- Define components inline with tags like `<def-card> ... </def-card>`.
- Invoke with matching symbol tags like `<card> ... </card>`.
- Supports nested components.
- Supports props via `<prop name="..." default="...">`.
- Supports default and named slots via `<slot>` and `<slot name="...">`.
- Uses lexical scoping with shadowing for component definitions.
- Resolves symbols from nearest scope outward.
- Supports warning/error assertions in fixture tests.

## Recipes Discovery Prototype

`demos/recipes` now includes a metadata-driven discovery prototype:

- Recipe detail pages in `demos/recipes/src/recipes/*.html` declare metadata on the root `<html>` tag with `data-*` attributes.
- Build generates `generated/recipes/search-index.json`.
- `generated/recipes/index.html` loads `recipes-search.js` to provide client-side search, filter, sort, and pagination from that JSON.

## Usage

Build default demo:

```bash
./scripts/build.sh
```

Build all demos:

```bash
make demos
```

Build a specific demo (examples):

```bash
./scripts/build.sh demos/blog/src generated/blog
./scripts/build.sh demos/recipes/src generated/recipes
```

Dev watch + local server:

```bash
./scripts/dev.sh
```

Run test fixtures:

```bash
./scripts/test.sh
```
