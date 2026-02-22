<p align="center">
  <img src="assets/logo/logo-horizontal-1200x240.png" alt="DefSite" width="720" />
</p>

<p align="center">
  <strong>HTML-first static site compiler with component tags and metadata indexing.</strong>
</p>

DefSite compiles plain HTML with inline `def-*` components into browser-ready static output.

No runtime framework. No template braces. No hydration requirement.

## Why DefSite

- Keep authoring in HTML, not in a separate template language.
- Compose reusable components with lexical scoping and slots.
- Generate deterministic static output for easy hosting and caching.
- Build discovery/search indexes from `data-*` metadata when needed.

## Quick Start

Build the compiler:

```bash
make build
```

Build all demos:

```bash
make demos
```

Build a single source directory:

```bash
./scripts/build.sh demos/site/src generated/site
```

Run fixture tests:

```bash
make test
```

Dev loop (watch + local server):

```bash
make dev
```

## Minimal Example

Input (`src/index.html`):

```html
<!doctype html>
<html>
  <body>
    <def-card>
      <article class="card">
        <h2><bind name="title" default="Untitled"></bind></h2>
        <div><slot></slot></div>
      </article>
    </def-card>

    <card title="Hello DefSite">
      <p>Compiled at build time.</p>
    </card>
  </body>
</html>
```

Build:

```bash
./bin/defsite src generated
```

Output: plain expanded HTML with `def-*` removed.

## Component Features

- `def-*` component definitions inline in HTML.
- Invocation by matching custom tag.
- `<bind name="..." default="...">` for attributes.
- `bind-*` attributes for binding invocation values into output attributes.
- Default and named slots via `<slot>` and `<slot name="...">`.
- Lexical scoping + shadowing of definitions.
- Cycle detection and expansion depth guard.

## Discovery Indexing

If a page root contains `data-*` metadata (on `<html ...>`), DefSite emits `search-index.json`.

Each record includes:

- `url`: the generated page path
- `meta`: all root `data-*` fields (without the `data-` prefix), as strings

This supports site-specific search/filter UI logic without custom per-site indexer code.

## Demos

- `demos/site/src`: minimal language demo.
- `demos/blog/src`: developer blog theme + post discovery UI.
- `demos/recipes/src`: recipe site theme + recipe discovery UI.

Build outputs are written to `generated/<demo>/`.

## Docs

- `docs/guide.md`: user/developer guide.
- `docs/devspecs/README.md`: development specs index.
- `docs/devspecs/component-spec.md`: component language draft.
- `docs/devspecs/spec-compliance.md`: fixture coverage map.
- `docs/devspecs/recipes-discovery-ux-plan.md`: discovery design history.

## Project History

- **2024-10-02**: initial ideation and prototype commits.
- **2026-02-22**: modular C rewrite, richer demos, generic discovery indexing.

Legacy prototype artifacts remain in `archive/legacy-2024`.

## Repository Layout

- `src/main.c`: CLI entrypoint.
- `src/defsite/*.c`: parser, DOM, expansion engine, discovery indexer.
- `scripts/*.sh`: build/dev/test helpers.
- `tests/pass`, `tests/fail`: behavior fixtures.
- `assets/logo/`: logo variants (horizontal + small sizes + favicon).

## License

Add a `LICENSE` file if you intend public reuse.
