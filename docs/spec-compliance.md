# Spec Compliance Checklist (v1)

Tracks `docs/component-spec.md` requirements against automated fixtures.

## Coverage Matrix

| Spec area | Requirement | Coverage | Fixture(s) |
|---|---|---|---|
| 2.1 Definition syntax | `def-*` declares symbol from suffix | Covered | `tests/pass/basic_slots` |
| 2.2 Invocation syntax | matching symbol tag expands template | Covered | `tests/pass/basic_slots` |
| 2.3 Props | `<prop name>` reads invocation attributes | Covered | `tests/pass/nested_components` |
| 2.3 Prop fallback | `<prop default>` used when missing | Covered | `tests/pass/nested_components` |
| 2.3 Missing prop warning | missing prop without default warns | Covered | `tests/pass/missing_prop_warning` |
| 2.4 Default slot | unnamed children projected into `<slot>` | Covered | `tests/pass/basic_slots` |
| 2.4 Named slots | `slot="name"` routes to `<slot name="...">` | Covered | `tests/pass/basic_slots` |
| 3 Scoping | nearest lexical scope wins | Covered | `tests/pass/scoping_shadow` |
| 3 Shadowing | inner def overrides outer def | Covered | `tests/pass/scoping_shadow` |
| 4 Symbol fallback | unresolved symbol remains unchanged + warning | Covered | `tests/pass/unknown_symbol_warning` |
| 4 Native tag precedence | built-in HTML tags are never component-resolved | Covered | `tests/pass/native_tag_precedence` |
| 5 Hoisting | use before local `def-*` declaration works | Covered | `tests/pass/use_before_def` |
| 6 Remove defs | `def-*` nodes omitted from output | Covered | all pass fixtures (via expected output) |
| 7 Nested components | components can expand other components | Covered | `tests/pass/nested_components` |
| 7 Recursion guard | direct/indirect cycles fail build | Covered | `tests/fail/recursive_cycle` |
| 8 Duplicate defs error | duplicate symbol in same scope fails build | Covered | `tests/fail/duplicate_def` |
| 8 Invalid def name error | invalid `def-*` symbol fails build | Covered | `tests/fail/invalid_def_name` |
| 8 Unknown named slot warning | unmatched named slot payload warns | Covered | `tests/pass/unknown_named_slot_warning` |

## Notes

- Warnings for pass cases are asserted via `stderr_contains.txt` in fixture directories.
- Fail cases assert stderr patterns via `error_contains.txt` and require non-zero exit status.
- Cross-file imports/exports are explicitly non-goals in v1 and are not tested.
