# Def-Component Spec (Draft v1)

Status: draft for review

Goal: HTML-first component composition with build-time expansion, nested components, attributes, default + named slots, and lexical scoping. No `{{ }}` template language in page files.

## 1. Core Model

1. Authors write normal HTML.
2. Components are defined inline using tags prefixed with `def-`.
3. Components are used via normal custom tags (symbol names).
4. Build step resolves symbols and expands to static HTML.
5. Browser never sees `def-*` tags in final output.

## 2. Syntax

### 2.1 Component definition

A component definition is any element whose tag starts with `def-`.

Example:

```html
<def-card>
  <article class="card">
    <h2><bind name="title"></bind></h2>
    <div class="body"><slot></slot></div>
  </article>
</def-card>
```

Definition name is the suffix after `def-`.
- `def-card` defines symbol `card`
- `def-hero-banner` defines symbol `hero-banner`

### 2.2 Component invocation

Invocation is a matching element tag by symbol name.

```html
<card title="Dog Facts">
  <p>Dogs were domesticated from wolves.</p>
</card>
```

### 2.3 Binds (invocation attributes)

Invocation attributes are available in template using `<bind name="...">`.

- `<bind name="title"></bind>` inserts escaped text value of attribute `title`
- Missing bind yields empty string unless a fallback is provided:

```html
<bind name="title" default="Untitled"></bind>
```

### 2.3.1 Attribute target binds

Template elements may bind invocation values into output attributes with `bind-*`:

```html
<a bind-href="url" bind-aria-label="title">
  <bind name="title"></bind>
</a>
```

Rules:
- `bind-<target>="source"` sets output attribute `<target>` from invocation attribute `source`
- If `<target>` already exists, the bound value replaces it
- Missing source emits warning and leaves target unset
- `bind-*` attributes are removed from final output

### 2.4 Slots

- `<slot></slot>` = default slot (unnamed children)
- `<slot name="header"></slot>` = named slot
- Invocation children route by `slot="..."` attribute

Example:

```html
<card-layout>
  <h1 slot="header">Dogs</h1>
  <p>Body text.</p>
  <a slot="footer" href="/dogs">Read more</a>
</card-layout>
```

## 3. Scoping Rules

Definitions are lexical and scoped by DOM ancestry.

1. Document root is a scope.
2. Each component invocation creates a child scope.
3. `def-*` definitions belong to nearest containing scope.
4. Inner scopes shadow outer definitions with same symbol.
5. Definitions are not exported across files in v1.

Interpretation: “definable anywhere” means a `def-*` can appear in root, inside a layout wrapper, or inside another component body.

## 4. Symbol Resolution

Resolver for an invocation `<name>`:

1. Start from current lexical scope.
2. If symbol `name` is defined there, use it.
3. Else walk parent scopes until root.
4. If still not found: leave tag unchanged and emit warning.

Precedence rule for native HTML:
- Built-in HTML tags always remain native (never treated as components).
- Only non-native tag names participate in component resolution.

## 5. Definition Hoisting

Within a single scope, `def-*` are hoisted for resolution.
- A component may be used before its `def-*` declaration in the same scope.

## 6. Expansion Pipeline

1. Parse source HTML to DOM.
2. Build symbol tables per scope from `def-*` nodes.
3. Expand invocations recursively.
4. Resolve binds and slots.
5. Remove `def-*` nodes from output.
6. Serialize final HTML.

## 7. Recursion and Safety

1. Nested components are supported.
2. Direct or indirect self-recursion is an error.
3. Max expansion depth configurable (default: 64).

## 8. Error/Warning Behavior

Errors (fail build):
- Recursive cycle detected
- Invalid `def-*` name
- Duplicate `def-*` with same name in same scope

Warnings (continue build):
- Unknown invocation symbol
- Unknown named slot payload (no matching `<slot name>`)
- Missing bind with no default

## 9. Mock User Experience

### 9.1 Single-file local components

```html
<!doctype html>
<html>
  <body>
    <def-card>
      <article class="card">
        <header><slot name="header"></slot></header>
        <section><slot></slot></section>
        <footer><slot name="footer"></slot></footer>
      </article>
    </def-card>

    <card>
      <h2 slot="header">Dog Facts</h2>
      <p>Dogs are social animals.</p>
      <a slot="footer" href="/dogs">More</a>
    </card>
  </body>
</html>
```

### 9.2 Scoped override (shadowing)

```html
<def-badge>
  <span class="badge global"><slot></slot></span>
</def-badge>

<badge>Global style</badge>

<section>
  <def-badge>
    <strong class="badge local"><slot></slot></strong>
  </def-badge>

  <badge>Local style in this section</badge>
</section>

<badge>Back to global style</badge>
```

### 9.3 Nested component definitions inside component body

```html
<def-panel>
  <div class="panel">
    <def-row>
      <div class="row"><slot></slot></div>
    </def-row>

    <row><slot></slot></row>
  </div>
</def-panel>

<panel>
  <p>Panel content</p>
</panel>
```

## 10. Non-Goals (v1)

1. No loops/conditionals.
2. No arbitrary expression language.
3. No cross-file imports/exports.
4. No runtime hydration requirements.

## 11. v2 Candidates

1. Explicit cross-file `def-use` import mechanism.
2. Fallback content inside `<slot>` elements.
3. Attribute passthrough helper (`<attrs></attrs>` placeholder).
4. Typed binds (`type="number|bool|string"`).
