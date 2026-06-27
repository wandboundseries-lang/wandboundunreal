# CardDB Unreal Bundle Schema Validation Audit

## Scope

This audit covers a test-only extension to the CardDB schema fixture validator under `Source/WandboundTests/Private/`.

No production CardDB importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Root-Card Fixture Validation

`FWBCardDBSchemaFixtureValidator` currently validates root-card-shaped JSON fixtures from `Reference/GodotCanon/CardDBSchemaFixtures/`.

Root-card fixtures validate:

- required `schema_version`, `card_id`, and `public_name`
- supported card kinds
- activated effect ids and duplicate effect ids within one card
- supported target requirements, source zones, timings, payload types, and payload operations
- malformed `activated_effects`, `source_gate`, `cost_gate`, and `payloads` fields
- numeric payload and RR fields
- player-facing label policy
- hidden-information label policy

Valid root-card fixtures may map into one `FWBCardDefinition` for test assertions only.

## Current Strict Validation Behavior

Strict unknown-field validation is opt-in through `FWBCardDBSchemaValidationOptions`.

Default validation remains non-strict. Strict root-card validation rejects unknown top-level/card, stats, effect, source gate, cost gate, payload, and metadata fields.

## Why A Future Bundle Shape Is Needed

A future production CardDB is expected to be a collection of cards, not one card per file. A top-level bundle wrapper gives production import work a stable place for:

- bundle schema version
- CardDB content version
- source/import version
- migration notes
- authoring metadata
- `cards[]`

Validating that shape now keeps importer planning deterministic without creating a production importer.

## Proposed Top-Level Bundle Fields

Required bundle fields:

- `bundle_schema_version`
- `carddb_version`
- `cards`

Optional bundle fields:

- `source_version`
- `migration_notes`
- `metadata`

`bundle_schema_version` must equal `1`. `carddb_version` must be a non-empty string.

## Proposed Cards[] Behavior

`cards` must exist, must be an array, and must not be empty.

Each element in `cards[]` is validated through the existing root-card validator. Bundle validation aggregates card diagnostics and maps valid cards into `FWBCardDefinition` values for validation only.

## Duplicate Card Policy

Duplicate non-empty `card_id` values across a bundle fail with `card_id_duplicate`.

The diagnostic may include the duplicate card id because card ids are public authoring identifiers, not hidden values.

## Strict Bundle Unknown-Field Policy

In strict mode, bundle top-level fields are allow-listed:

- `bundle_schema_version`
- `carddb_version`
- `source_version`
- `migration_notes`
- `metadata`
- `cards`

Unknown bundle top-level fields fail with `unknown_top_level_field`.

Bundle metadata may contain:

- `author`
- `notes`
- `source`
- `version`
- `test_only`

Unknown bundle metadata fields fail with `unknown_metadata_field`.

Strict mode also propagates to each card through the existing root-card strict validator.

## Why This Pass Remains Test-Only

Bundle validation proves the future production data shape with small Unreal-owned fixtures. It does not load production data, create production zones, generate runtime actions, build runtime public summaries, execute effects, or mutate game state.

The validator remains under `WandboundTests` and must not be included by `WandboundCore` or `WandboundRuntime`.

## Why Production Importer/Loader Remains Deferred

Production import still needs a separately designed boundary for asset packaging, source versioning, hidden-information observation, zone ownership, runtime provider input, and EffectRunner-backed execution. This pass only validates fixture JSON shape and diagnostics so future importer work has a clearer fail-closed contract.
