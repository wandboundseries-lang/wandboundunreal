# CardDB Unreal Bundle Dependency Export Report

## Purpose

Added test-only JSON export snapshots for CardDB bundle validation results.

This remains fixture validation only. It is not a production CardDB importer, loader, zone model, runtime activation path, gameplay rule, or EffectRunner path.

## Export Helper

Added:

```cpp
FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest
```

The helper serializes `FWBCardDBBundleSchemaValidationResult` into deterministic JSON for fixture comparison.

## Export JSON Shape

Export fields:

```json
{
  "ok": true,
  "source_path": "valid_bundle_dependency_order_simple_chain.json",
  "card_count": 3,
  "dependency_order_card_ids": [
    "base_card",
    "middle_card",
    "top_card"
  ],
  "diagnostics": [
    {
      "code": "dependency_cycle_detected",
      "card_index": 1,
      "card_id": "cycle_b",
      "effect_id": "",
      "json_path": "$.cards[1].references.card_ids[0]"
    }
  ]
}
```

The writer uses stable field order and condensed JSON output.

## Deterministic Ordering Policy

`dependency_order_card_ids` preserves the computed dependency order.

Diagnostics preserve validation order. The export helper does not sort or regroup diagnostics.

`source_path` exports only the clean file name, not the full path.

## Diagnostic Export Field Policy

Diagnostic export includes only:

- `code`
- `card_index`
- safe `card_id`
- safe `effect_id`
- safe `json_path`

Diagnostic `Message` is intentionally omitted.

The export does not include full source JSON, public labels, public text, payload objects, or referenced hidden values.

## Hidden-Token Export Safety

Exported identifiers are sanitized through the existing hidden-token policy.

Hidden-token fixture coverage verifies `SECRET` does not appear anywhere in export JSON.

## Expected Export Fixtures

Expected exports live under:

```text
Reference/GodotCanon/CardDBSchemaFixtures/ExpectedExports/
```

These are Unreal-owned expected snapshots, not production data and not Godot project files.

## Valid Export Fixture Coverage

Added expected export snapshots for:

- `valid_bundle_dependency_order_simple_chain.export.json`
- `valid_bundle_dependency_order_reverse_input.export.json`
- `valid_bundle_dependency_order_diamond.export.json`

These cover simple chain, reverse input dependency sorting, and diamond dependency tie-breaks.

## Invalid Export Fixture Coverage

Added expected export snapshots for:

- `invalid_bundle_dependency_self_reference.export.json`
- `invalid_bundle_dependency_cycle_two_cards.export.json`
- `invalid_bundle_dependency_cycle_effect_ref.export.json`
- `invalid_bundle_dependency_hidden_token_safe.export.json`
- `invalid_bundle_multiple_invalid_cards.export.json`

These cover self-reference diagnostics, cycle diagnostics, effect/path context, hidden-token safety, and multi-card diagnostic context.

## Bundle-Level Export Fixture Coverage

Expected export snapshots now also cover malformed bundle-level diagnostics:

- missing or unsupported bundle schema version
- missing CardDB version
- missing, malformed, or empty `cards`
- duplicate card ids
- strict unknown bundle fields
- strict unknown bundle metadata fields
- non-strict unknown fields that remain valid
- mixed bundle-level and card-level errors

These snapshots extend dependency export coverage without changing the export helper into a production API.

## Dependency Edge-Case Export Coverage

Expected export snapshots now cover importer-ready dependency ordering edge cases:

- duplicate references
- mixed card/effect/payload reference levels
- repeated direct and transitive paths
- disconnected components
- independent card input order
- effect and payload references to the same dependency
- invalid and missing-reference bundles that skip dependency order
- strict/non-strict unknown-field ordering behavior
- hidden-token-safe missing references

Invalid bundles now fail closed by exporting an empty `dependency_order_card_ids` array when validation diagnostics already exist before dependency ordering.

## Relationship To Future Importer Planning

These exports give future importer work stable review artifacts for dependency order and authoring diagnostics.

They do not load CardDB data into runtime and do not become a production export API.

## Importer Readiness Snapshots

Dependency order exports now feed test-only importer-readiness snapshots.

The readiness helper requires a valid non-empty bundle to have dependency order, then emits `ordered_card_ids` from validated `FWBCardDefinition` values in the same order.

Invalid dependency bundles remain not ready and export empty ordered card ids.

## Confirmations

- this remains test-only
- no production importer was implemented
- no production loader was added
- no production zones were added
- no Core or Runtime source files were changed
- no Godot project files were changed
- no effects are executed
- no activation candidates or activation actions are generated by production code
- no `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` changes were made
- no UI, response windows, Blueprints, `.uasset`, or `.umap` files were changed
