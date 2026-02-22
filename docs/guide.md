# DefSite Guide

DefSite is an HTML-first static site compiler.

You define reusable components with `def-*` tags, invoke them as regular custom tags, and compile to plain static HTML. No runtime framework is required for component expansion.

## Mini Tutorial

This complete example uses the core language in one file:
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

## What DefSite Is

- A build-time HTML component engine written in C.
- A static-site workflow for content-heavy sites.
- A metadata indexing pipeline for search/filter UIs.

## What DefSite Is Not

- Not a runtime component framework.
- Not a full template expression language.
- Not a server-side renderer with request-time logic.

## Core Authoring Model

### 1. Define Components Inline

```html
<def-card>
  <article class="card">
    <h2><bind name="title" default="Untitled"></bind></h2>
    <div><slot></slot></div>
  </article>
</def-card>
```

### 2. Invoke Components as Tags

```html
<card title="Dog Facts">
  <p>Dogs were domesticated from wolves.</p>
</card>
```

### 3. Use Named Slots When Needed

```html
<def-layout>
  <header><slot name="header"></slot></header>
  <main><slot></slot></main>
</def-layout>

<layout>
  <h1 slot="header">Hello</h1>
  <p>Body content.</p>
</layout>
```

### 4. Bind Invocation Values Into Output Attributes

```html
<def-cta-link>
  <a class="cta" bind-href="href" bind-aria-label="label">
    <bind name="label"></bind>
  </a>
</def-cta-link>
```

## Resolution and Scoping

- Definitions are lexical (DOM ancestry based).
- Inner scopes can shadow outer definitions.
- Native HTML tags are never component-resolved.
- `def-*` declarations are removed from final output.

## Discovery Metadata and Search Index

DefSite includes a page in `search-index.json` when the root `<html>` has at least one `data-*` attribute.

No metadata keys are required by the engine; schema is site-defined.

For collection pages (recipes, blog posts, docs, etc.), add `data-*` attributes to root `<html>`:

```html
<html
  lang="en"
  data-kind="post"
  data-slug="build-your-own-static-analyzer-pass"
  data-title="Build Your Own Static Analyzer Pass"
  data-summary="A focused static analysis pass can enforce team invariants."
  data-published="2026-02-13"
  data-tags="static-analysis,compiler,tooling"
>
```

Build output includes `search-index.json` in each generated demo/site output directory.

Each record includes:
- `url` for the generated page path.
- `meta` containing all root `data-*` fields (without the `data-` prefix), as strings.

Records are sorted by `url` for deterministic output.

This makes discovery extensible for new sites without changing C code for every new field. Site JS can parse numbers/lists/dates from `meta` as needed.

## Build and Dev Commands

From project root:

```bash
make build            # compile bin/defsite
make demos            # build all demos under demos/*/src
make dev              # rebuild-on-change + local server
make test             # run pass/fail fixture suite
```

Or build one source directory explicitly:

```bash
./scripts/build.sh demos/site/src generated/site
./scripts/build.sh demos/blog/src generated/blog
```

## Repository Map

- `src/defsite/*.c`: parser, DOM, expansion engine, discovery indexer.
- `demos/*/src`: source demos.
- `generated/*`: build output.
- `tests/pass`, `tests/fail`: fixture-based behavior contract.
- `docs/devspecs`: design and compliance development specs.

## Current Limitations

- No loops or conditionals in templates.
- No cross-file component import/export mechanism (v1).
- Discovery filtering/sorting behavior is implemented in per-site JS.

## Where to Go Next

- Language and behavior details: `docs/devspecs/component-spec.md`
- Fixture coverage map: `docs/devspecs/spec-compliance.md`
- Discovery design history: `docs/devspecs/recipes-discovery-ux-plan.md`
