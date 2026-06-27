# CardDB Unreal Bundle-Level Export Diagnostics Audit

## Scope

This audit covers test-only CardDB bundle validation export snapshots for malformed bundle-level diagnostics.

No production CardDB importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Export Helper Behavior

`FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest` serializes `FWBCardDBBundleSchemaValidationResult` under `Source/WandboundTests/Private/`.

The helper writes deterministic condensed JSON and exports only:

- `ok`
- clean-file-name `source_path`
- `card_count`
- `dependency_order_card_ids`
- `diagnostics`

Diagnostic exports include only:

- diagnostic `code`
- `card_index`
- safe `card_id`
- safe `effect_id`
- stable `json_path`

Diagnostic `Message`, full source JSON, public text, public labels, payload bodies, and referenced hidden values are intentionally omitted.

## Current Export JSON Shape

Current snapshots use this shape:

```json
{"ok":false,"source_path":"invalid_bundle_missing_cards.json","card_count":0,"dependency_order_card_ids":[],"diagnostics":[{"code":"cards_missing","card_index":-1,"card_id":"","effect_id":"","json_path":"$.cards"}]}
```

Bundle-level diagnostics export `card_index = -1`, empty `card_id`, and empty `effect_id` unless validation has safe card or effect context.

## Current Expected Export Coverage

Existing expected export snapshots cover:

- valid dependency order
- dependency self-reference
- dependency cycles
- missing reference hidden-token safety
- multiple invalid card diagnostics

Those snapshots prove dependency and per-card context, but they do not yet cover malformed bundle fields.

## Current Bundle-Level Diagnostics

The bundle validator currently emits stable bundle-level diagnostics for:

- `bundle_schema_version_missing`
- `bundle_schema_version_unsupported`
- `carddb_version_missing`
- `cards_missing`
- `cards_malformed`
- `cards_empty`
- `card_id_duplicate`
- strict `unknown_top_level_field`
- strict `unknown_metadata_field`

Duplicate card ids use the second duplicate index, the safe duplicate card id, and `$.cards`.

## Missing Export Coverage

Expected export snapshots were missing for:

- missing or unsupported bundle schema version
- missing CardDB version
- missing, malformed, or empty `cards`
- duplicate card ids
- strict unknown bundle fields
- strict unknown bundle metadata fields
- non-strict unknown fields that remain valid
- mixed bundle-level and card-level diagnostics
- non-dependency hidden-token reference diagnostics

## Why Expected Export Snapshots Are Useful

Stable JSON snapshots give future importer work reviewable, deterministic examples of fail-closed diagnostics before production data loading exists.

They also protect diagnostic field shape, source-path sanitization, ordering, and hidden-information policy while keeping runtime behavior untouched.

## Why This Remains Test-Only

The validator and export helper live under `WandboundTests`.

They do not import CardDB data into production modules, do not generate activation candidates or actions, do not execute effects, and do not mutate game state.

Production importer and loader work remains deferred until a separate milestone defines data ownership, packaging, zone visibility, hidden-information observation, and EffectRunner-backed payload resolution.

## Hidden-Information Policy

Export JSON must not leak hidden tokens through source values, referenced ids, diagnostic messages, labels, source snippets, logs, tests, or docs.

Hidden reference fixtures should prove that unsafe referenced values such as `SECRET` do not appear in exported JSON.
