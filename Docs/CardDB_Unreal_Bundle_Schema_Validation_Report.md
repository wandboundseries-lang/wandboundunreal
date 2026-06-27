# CardDB Unreal Bundle Schema Validation Report

## Purpose

Added test-only CardDB bundle schema validation for a future production-shaped `cards[]` wrapper.

The bundle validator remains in `WandboundTests` and is not a production CardDB importer, production loader, production zone model, or runtime activation system.

## Bundle Fields

Required fields:

- `bundle_schema_version`
- `carddb_version`
- `cards`

Optional fields:

- `source_version`
- `migration_notes`
- `metadata`

`bundle_schema_version` must equal `1`. `carddb_version` must be a non-empty string.

## Cards[] Policy

`cards` must exist, must be an array, and must not be empty.

Each card object is validated through the existing root-card fixture validator. Valid cards map into `FWBCardDefinition` values for validation assertions only.

Existing root-card fixture validation behavior is unchanged.

## Duplicate Card Id Policy

Duplicate non-empty `card_id` values across one bundle fail closed with:

```text
card_id_duplicate
```

The duplicate card id may appear in the diagnostic card id field because card ids are public authoring identifiers.

## Strict Bundle Unknown-Field Policy

Strict mode applies to:

- bundle top-level fields
- bundle metadata fields
- each card through the existing strict root-card validator

Unknown bundle top-level fields fail with:

```text
unknown_top_level_field
```

Unknown bundle metadata fields fail with:

```text
unknown_metadata_field
```

Default bundle validation remains non-strict. Unknown bundle fields are ignored by default unless an existing validation path catches malformed data or policy violations.

## Bundle Metadata Policy

Allowed bundle metadata fields:

- `author`
- `notes`
- `source`
- `version`
- `test_only`

Metadata is authoring-only and is not runtime state.

## New Bundle Diagnostics

Added stable diagnostics:

- `carddb_version_missing`
- `cards_missing`
- `cards_malformed`
- `cards_empty`
- `card_id_duplicate`
- `bundle_schema_version_missing`
- `bundle_schema_version_unsupported`

Existing diagnostics are unchanged.

## Valid Bundle Fixtures

Added valid bundle fixtures:

- `valid_bundle_supported_cards.json`
- `valid_bundle_cost_usage_cards.json`
- `strict_valid_bundle_metadata.json`

These cover supported payload families, cost/usage schema shape, and strict bundle metadata allow-listing.

## Invalid Bundle Fixtures

Added invalid bundle fixtures:

- `invalid_bundle_missing_schema_version.json`
- `invalid_bundle_unsupported_schema_version.json`
- `invalid_bundle_missing_carddb_version.json`
- `invalid_bundle_missing_cards.json`
- `invalid_bundle_cards_not_array.json`
- `invalid_bundle_cards_empty.json`
- `invalid_bundle_duplicate_card_id.json`
- `invalid_bundle_contains_invalid_card.json`
- `strict_invalid_bundle_unknown_top_level_field.json`
- `strict_invalid_bundle_unknown_metadata_field.json`
- `strict_invalid_bundle_card_unknown_field.json`
- `non_strict_bundle_unknown_field_allowed.json`

## Relationship To Root-Card Validation

Bundle validation serializes each card object and validates it through the existing root-card `ValidateJsonString` path. This keeps root-card required-field, payload, source gate, cost gate, label, hidden-info, and strict unknown-field behavior centralized.

## Diagnostic Context

Bundle diagnostics now include stable per-card context when diagnostics come from `cards[]` entries:

- zero-based card index
- safe bundle card id
- existing effect id
- stable JSON path

Duplicate card id diagnostics report the second duplicate index and `$.cards` path.

## Cross-Reference Validation

Bundle fixtures may now include test-only `references` objects at card, activated-effect, and payload level.

Root-card validation checks reference shape only. Bundle validation resolves references against the card id/effect id index after child card validation.

Supported reference fields:

- `card_ids`
- `effect_refs`
- `metadata`

Supported effect reference fields:

- `card_id`
- `effect_id`
- `metadata`

Supported reference metadata fields:

- `notes`
- `test_only`

Cross-reference diagnostics:

- `missing_card_reference`
- `missing_effect_reference`
- `reference_malformed`
- `unknown_reference_field`

Duplicate card ids make the reference index ambiguous, so bundle validation preserves duplicate diagnostics and skips missing-reference resolution for that bundle.

## Dependency Ordering

Bundle validation can now produce a test-only `DependencyOrderCardIds` array for acyclic references.

Dependency edges are derived from card, effect, and payload `references` fields after cross-reference validation.

Ordering policy:

- dependencies appear before dependents.
- independent ties follow original `cards[]` order.
- missing references leave dependency order empty.
- duplicate card ids leave dependency order empty.
- self-references fail with `dependency_self_reference`.
- cycles fail with `dependency_cycle_detected`.

This order is validation output only and does not load cards into runtime or map reference data into `FWBCardDefinition`.

## Bundle-Level Export Coverage

Malformed bundle-level diagnostics now have expected export snapshot coverage under:

```text
Reference/GodotCanon/CardDBSchemaFixtures/ExpectedExports/
```

Coverage includes missing or unsupported bundle schema version, missing CardDB version, missing/malformed/empty `cards`, duplicate card ids, strict unknown bundle fields, non-strict unknown fields, and mixed bundle/card diagnostics.

## Confirmations

- this remains test-only
- no production importer was implemented
- no production loader was added
- no production zones were added
- no Core or Runtime source files were changed
- no effects are executed
- no activation candidates or activation actions are generated by production code
- no `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` changes were made
- no UI, response windows, Blueprints, `.uasset`, or `.umap` files were changed
