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

## Mini Tutorial

This example shows all core language features in one place:
- `def-*` definition
- component invocation
- `<bind>` text binding
- `bind-*` attribute binding
- default slot + named slot

```html
<!doctype html>
<html>
  <body>
    <def-card-link>
      <article class="card">
        <h2><bind name="title" default="Untitled"></bind></h2>
        <a class="cta" bind-href="href" bind-aria-label="title">Open</a>
        <section><slot></slot></section>
        <footer><slot name="meta"></slot></footer>
      </article>
    </def-card-link>

    <card-link title="Rust FFI Notes" href="/posts/rust-ffi-c.html">
      <p>Ownership rules at the boundary are part of your ABI contract.</p>
      <small slot="meta">11 min read</small>
    </card-link>
  </body>
</html>
```

Rules:
- `<bind name="x">` reads invocation attribute `x` as escaped text.
- `bind-target="x"` sets output attribute `target` from invocation attribute `x`.
- `<slot>` receives unnamed children.
- `<slot name="meta">` receives children with `slot="meta"`.

## Component Features

- `def-*` component definitions inline in HTML.
- Invocation by matching custom tag.
- `<bind name="..." default="...">` for attributes.
- `bind-*` attributes for binding invocation values into output attributes.
- Default and named slots via `<slot>` and `<slot name="...">`.
- Lexical scoping + shadowing of definitions.
- Cycle detection and expansion depth guard.

## Discovery Indexing

DefSite includes a page in `search-index.json` when the root `<html>` has at least one `data-*` attribute.

No metadata keys are required by the engine; schema is site-defined.

Each record includes:

- `url`: the generated page path
- `meta`: all root `data-*` fields (without the `data-` prefix), as strings

Records are sorted by `url` for deterministic output.

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
