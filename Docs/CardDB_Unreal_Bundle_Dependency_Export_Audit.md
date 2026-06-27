# CardDB Unreal Bundle Dependency Export Audit

## Scope

This audit covers a test-only export helper for CardDB bundle validation results under `Source/WandboundTests/Private/`.

No production CardDB importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Dependency Order Behavior

`FWBCardDBBundleSchemaValidationResult` currently includes `DependencyOrderCardIds`.

Bundle dependency validation derives edges from existing fixture-only `references` fields:

- card-level `references.card_ids`
- effect-level `references.card_ids`
- effect-level `references.effect_refs`
- payload-level `references.card_ids`

If card A depends on card B, B appears before A in `DependencyOrderCardIds`.

Ordering is populated only for unambiguous acyclic bundles. Duplicate card ids, missing card references, missing effect references, malformed card ids, self-references, and cycles leave dependency order empty.

Stable tie-breaks use original `cards[]` order.

## Current Diagnostic Context Fields

`FWBCardDBSchemaValidationDiagnostic` carries:

- `Code`
- `Message`
- `CardId`
- `EffectId`
- `CardIndex`
- `BundleCardId`
- `JsonPath`

Bundle diagnostics preserve validation order and use zero-based `cards[]` indices, safe bundle card ids, safe effect ids, and stable JSON paths.

## Current Dependency Cycle And Self-Reference Behavior

Self-dependencies fail with:

```text
dependency_self_reference
```

Self-reference diagnostics point at the reference path and do not also emit a dependency-cycle diagnostic.

Cycles fail with:

```text
dependency_cycle_detected
```

Cycle diagnostics report the first deterministic cycle-closing edge found by validation order. They use generic text and safe owner context only.

## Current Hidden-Token Diagnostic Safety

Diagnostics sanitize unsafe card/effect ids before exposing them as context. Missing-reference diagnostics do not echo referenced ids.

Hidden-token fixture coverage verifies diagnostic message, card id, bundle card id, effect id, and JSON path do not leak `SECRET` tokens.

## Why Stable JSON Export Fixtures Are Useful

Future CardDB importer planning needs reviewable examples of:

- computed dependency order
- fail-closed self-reference diagnostics
- fail-closed cycle diagnostics
- multi-card diagnostic context
- hidden-token-safe export output

JSON export fixtures give stable snapshots without introducing a production loader or runtime API.

## Why This Remains Test-Only

The export helper serializes test-only bundle validation results for automation comparison. It does not load CardDB data into production code, execute effects, mutate state, generate actions, or build runtime summaries.

The helper belongs to `WBCardDBSchemaFixtureValidator` under `WandboundTests` and must not be referenced by `WandboundCore` or `WandboundRuntime`.

## Why Production Importer/Loader Remains Deferred

Production import still needs separate design for data ownership, packaging, source versioning, hidden-information observation, zone ownership, runtime provider input, and EffectRunner-backed execution.

Export snapshots are planning artifacts only.

## Export Hidden-Information Policy

Export JSON must not contain:

- hidden source values
- referenced ids that include hidden tokens
- full source JSON snippets
- public text or labels unless needed
- diagnostic messages if context fields are enough

Preferred export fields are diagnostic code, card index, safe card id, safe effect id, and stable JSON path.
