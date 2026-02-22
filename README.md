# DefSite

DefSite is an HTML-first static site compiler.

It expands inline `def-*` component definitions at build time into plain browser-ready HTML, with optional metadata indexing for search/filter UIs.

## Project History

- **2024-10-02**: initial ideation and prototype commits (`41d7d3f`, `5c56362`).
- **2026-02-22**: major rebuild into a modular C engine with richer demos and discovery indexing.

Legacy prototype artifacts are kept in `archive/legacy-2024`.

## Documentation

- `docs/guide.md`: primary user/developer guide (what DefSite is, how it works, how to use it).
- `docs/devspecs/README.md`: index for development specs and design-history docs.
- `docs/devspecs/component-spec.md`: component language design draft.
- `docs/devspecs/spec-compliance.md`: fixture coverage matrix against the component spec.
- `docs/devspecs/recipes-discovery-ux-plan.md`: discovery UX design-history notes.

## Core Features (v1)

- Inline component definitions with `def-*` tags.
- Component invocation via matching custom tags.
- Props via `<prop name="..." default="...">`.
- Default and named slots via `<slot>` and `<slot name="...">`.
- Lexical scoping and shadowing for component definitions.
- Recursive expansion with cycle detection and depth guard.
- Fixture-based pass/fail test suite.

## Discovery Indexing

When pages include root `<html data-*>` metadata:

- Build emits `generated/<site>/search-index.json`.
- Records include common top-level fields (`kind`, `slug`, `title`, etc.).
- Records also include a generic `meta` object with all root `data-*` keys (minus `data-`).

This keeps the authoring model HTML-first while allowing site-specific filters without hardcoding every new field in docs or per-site schema conventions.

## Quick Start

Build compiler:

```bash
make build
```

Build all demos:

```bash
make demos
```

Build one source directory:

```bash
./scripts/build.sh demos/blog/src generated/blog
./scripts/build.sh demos/recipes/src generated/recipes
```

Dev mode (watch + local server):

```bash
make dev
```

Run fixtures:

```bash
make test
```

## Repository Layout

- `src/main.c`: CLI entry point.
- `src/defsite/*.c`: parser, DOM, expansion engine, discovery indexer.
- `demos/*/src`: demo sources.
- `generated/*`: built output.
- `tests/pass`, `tests/fail`: fixture suites.
- `scripts/*.sh`: build/dev/test helper scripts.
- `archive/legacy-2024`: archived earlier prototype.
