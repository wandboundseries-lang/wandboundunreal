# CardDB Unreal Bundle Cross-Reference Diagnostics Audit

## Scope

This audit covers a test-only extension to CardDB bundle fixture validation under `Source/WandboundTests/Private/`.

No production CardDB importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Bundle Validation Behavior

`ValidateBundleJsonString` validates bundle structure, then validates each `cards[]` entry through the existing root-card validator.

Current bundle validation covers:

- malformed bundle JSON
- missing or unsupported `bundle_schema_version`
- missing `carddb_version`
- missing, malformed, or empty `cards`
- duplicate card ids
- strict unknown bundle top-level and metadata fields
- root-card validation for every card object

Valid bundle cards may map into `FWBCardDefinition` values for test assertions only.

## Current Diagnostic Context Fields

`FWBCardDBSchemaValidationDiagnostic` currently carries:

- `Code`
- `Message`
- `CardId`
- `EffectId`
- `CardIndex`
- `BundleCardId`
- `JsonPath`

For bundle card diagnostics, `CardIndex` is the zero-based `cards[]` index, `BundleCardId` is a safe public card id when available, `EffectId` is preserved from root-card validation, and `JsonPath` is a stable path such as `$.cards[1].activated_effects[0].payloads[0]`.

Root-card validation outside bundle mode keeps bundle context absent.

## Current Strict-Mode Allowed Fields

Strict mode currently allow-lists bundle top-level fields, card fields, stats fields, effect fields, source gate fields, cost gate fields, payload fields, and their documented metadata objects.

There is currently no `references` object in the strict allow-lists.

## Current Object Validation

Current root-card validation checks:

- required card fields
- activated effect shape
- source gate and cost gate shape
- payload array shape
- supported payload families and operations
- numeric fields
- player-facing label and hidden-info policies
- strict unknown fields

Current bundle validation aggregates those diagnostics and adds bundle-level diagnostics.

## Why Cross-Card Reference Diagnostics Are Needed

Future CardDB import work will likely need authoring-time references from one card/effect/payload to another card or effect. Those references need deterministic diagnostics before production import exists so missing or malformed references fail closed instead of silently becoming no-ops.

This pass defines test-only reference diagnostics and context without assigning gameplay semantics.

## Proposed Reference Schema

Card-level:

```json
{
  "references": {
    "card_ids": ["other_card_id"]
  }
}
```

Effect-level:

```json
{
  "references": {
    "card_ids": ["other_card_id"],
    "effect_refs": [
      {
        "card_id": "other_card_id",
        "effect_id": "some_effect"
      }
    ]
  }
}
```

Payload-level:

```json
{
  "references": {
    "card_ids": ["other_card_id"]
  }
}
```

The `references` object is schema/test metadata only. It does not map into `FWBCardDefinition` and has no gameplay behavior in this pass.

## Hidden-Information Policy

Diagnostics must not include hidden values or full JSON snippets. If a referenced id contains a hidden token, diagnostics must use generic messages and safe context only.

## Why This Remains Test-Only

Reference validation is limited to Unreal-owned bundle fixtures. It does not load production data, create production zones, generate runtime actions, build runtime public summaries, execute effects, or mutate game state.

The validator remains under `WandboundTests` and must not be included by `WandboundCore` or `WandboundRuntime`.

## Why Production Importer/Loader Remains Deferred

Production import still needs separate design for data ownership, packaging, source versioning, hidden-information observation, zone ownership, runtime provider input, and EffectRunner-backed execution. This pass only improves test fixture diagnostics.
