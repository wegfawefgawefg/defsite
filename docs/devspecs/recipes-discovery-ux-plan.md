# Recipes Discovery UX Plan (Draft)

Status: design-history draft from discovery prototyping. Kept for context; current implementation now emits a generic discovery index with per-entry `meta` data.

## 1. Goal

Keep recipe authoring HTML-first while adding practical discovery features that cover most use cases:

1. Paginated recipe index
2. Client-side search
3. Client-side filter/sort

No backend required.

## 2. Constraints

1. Recipe pages remain plain HTML files under `demos/recipes/src/recipes/`.
2. Metadata is stored in `data-*` attributes in each recipe page.
3. Build process generates static HTML + a compact JSON index.
4. No arbitrary code execution tags (`c-tag`, plugin runtime) in v1.

## 3. User Experience

### 3.1 Reader UX

On `generated/recipes/index.html`, reader can:

1. Type search text (title/summary/ingredients keywords).
2. Filter by tags (diet, method, time bucket).
3. Sort by newest or fastest.
4. Page through results (`k` items per page).

The page updates results immediately in-browser from a static JSON index.

### 3.2 Author UX

To add a recipe, author:

1. Creates `demos/recipes/src/recipes/<slug>.html`.
2. Adds required `data-*` metadata to the root `<html>` tag.
3. Runs `make demos`.
4. Recipe automatically appears in index/search/filter pages.

No manual edits to master list required.

## 4. Metadata Contract (v1)

Recommended root element shape:

```html
<html
  lang="en"
  data-kind="recipe"
  data-slug="smoky-bean-chili"
  data-title="Smoky Bean Chili"
  data-summary="Pantry-friendly chili with deep flavor in 35 minutes."
  data-time-min="35"
  data-serves="4"
  data-difficulty="easy"
  data-diets="vegan,gluten-free"
  data-method="stovetop"
  data-published="2026-02-22"
>
```

### 4.1 Required fields

1. `data-kind` (`recipe`)
2. `data-slug`
3. `data-title`
4. `data-summary`
5. `data-time-min`
6. `data-published`

### 4.2 Optional fields

1. `data-serves`
2. `data-difficulty`
3. `data-diets` (comma-separated)
4. `data-method`
5. `data-featured` (`true|false`)

## 5. Build Outputs

Given source pages in `demos/recipes/src/recipes/`, build produces:

1. `generated/recipes/recipes/<slug>.html` (recipe pages)
2. `generated/recipes/search-index.json` (search/filter dataset)
3. `generated/recipes/page/<n>/index.html` (paginated static pages)
4. `generated/recipes/index.html` (alias of page 1)

Optional v2:

1. `generated/recipes/filter/<facet>/<value>/page/<n>/index.html`

## 6. Search Index Shape

Example `search-index.json`:

```json
[
  {
    "slug": "smoky-bean-chili",
    "url": "recipes/smoky-bean-chili.html",
    "title": "Smoky Bean Chili",
    "summary": "Pantry-friendly chili with deep flavor in 35 minutes.",
    "time_min": 35,
    "published": "2026-02-22",
    "diets": ["vegan", "gluten-free"],
    "difficulty": "easy",
    "method": "stovetop"
  }
]
```

Original prototype assumption was a whitelisted index shape. Current implementation keeps common top-level fields and also includes a generic `meta` object with all root `data-*` values.

## 7. Pagination Model

1. `k` items per page configurable (default `k=12`).
2. URLs use path-based pages for static hosting:
- `/recipes/`
- `/recipes/page/2/`
- `/recipes/page/3/`
3. Out-of-range pages redirect or render empty state with link to page 1.

## 8. Client-Side State Model

Use query params so filters are shareable:

1. `q` search query
2. `diet` diet filter
3. `method` method filter
4. `sort` sort key (`newest|time_asc|time_desc`)
5. `page` current page

Example:

`/recipes/?q=chili&diet=vegan&sort=time_asc&page=2`

Behavior:

1. Parse URL params on load.
2. Load `search-index.json` once.
3. Apply filter/sort/pagination client-side.
4. Update URL when controls change.

## 9. No-JS Fallback

If JS is unavailable:

1. Static paginated index pages still work.
2. Search box can degrade to non-functional text input or link to simple all-recipes page.

This keeps core navigation intact.

## 10. Validation and Warnings (Build Time)

Build warnings for malformed metadata:

1. Missing required `data-*` field
2. Invalid `data-time-min` (non-numeric)
3. Invalid `data-published` format
4. Duplicate `data-slug`

Invalid recipes can be skipped from index with warning while still generating page HTML.

## 11. Why This Covers Most Needs

This design handles the common 90% case:

1. Growing content collections
2. Fast finding/filtering
3. Predictable static deploys
4. HTML-first authoring with minimal additional rules

Advanced plugin/tag execution can be deferred until a clear real use case appears.
