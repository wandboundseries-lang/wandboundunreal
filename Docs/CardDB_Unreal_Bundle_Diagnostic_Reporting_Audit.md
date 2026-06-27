# CardDB Unreal Bundle Diagnostic Reporting Audit

## Scope

This audit covers the test-only CardDB bundle schema validator under `Source/WandboundTests/Private/`.

No production CardDB importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Bundle Diagnostics Behavior

`ValidateBundleJsonString` validates bundle-level fields first, then iterates `cards[]` in array order and validates each card through the existing root-card validator.

Bundle-level diagnostics currently cover:

- malformed bundle JSON
- missing or unsupported `bundle_schema_version`
- missing `carddb_version`
- missing, malformed, or empty `cards`
- duplicate `card_id`
- strict unknown bundle top-level fields
- strict unknown bundle metadata fields

Card-level diagnostics are appended from each root-card validation result.

## Current Card-Level Diagnostic Fields

`FWBCardDBSchemaValidationDiagnostic` currently carries:

- diagnostic code
- message
- card id
- effect id

This preserves public card/effect authoring ids when available, but it does not record which `cards[]` entry produced the diagnostic.

## Current Diagnostic Order Behavior

Bundle-level diagnostics are emitted before card iteration when they are discovered before `cards[]` validation.

Card diagnostics are appended in `cards[]` iteration order. Within each card, root-card validation order is preserved.

Strict unknown-field diagnostics are deterministic where they use sorted field-name iteration.

## Current Duplicate-Card Behavior

Duplicate non-empty card ids fail with `card_id_duplicate`.

The diagnostic currently stores the duplicate id as the diagnostic card id, but it does not identify the second duplicate `cards[]` index or a stable bundle JSON path.

## Missing Per-Card Context Coverage

Missing context:

- zero-based `cards[]` index
- stable bundle card id context separate from root-card `CardId`
- path-like JSON location
- targeted tests for multiple invalid cards in one bundle
- targeted hidden-token safety checks for contextual fields

## Why Stable Diagnostic Context Matters

Future production import work will need to report authoring errors in multi-card data without ambiguity. Stable card index, card id, effect id, and path context make it possible to identify the bad card/effect while keeping diagnostics deterministic and safe for test assertions.

This is especially useful before implementing production import, because fixture diagnostics can define the expected fail-closed reporting contract.

## Why This Remains Test-Only

The validator stays under `WandboundTests`. It may parse fixture JSON, validate fixture shape, and map valid cards into `FWBCardDefinition` values for validation assertions only.

It does not load production CardDB data, create production zones, generate runtime actions, build runtime public summaries, execute effects, or mutate game state.

## Why Production Importer/Loader Remains Deferred

Production import still needs separately designed ownership for data packaging, source versioning, hidden-information observation, zone ownership, runtime provider input, and EffectRunner-backed execution. This pass only improves test fixture diagnostic reporting.
