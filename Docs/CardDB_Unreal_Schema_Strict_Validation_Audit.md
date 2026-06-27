# CardDB Unreal Schema Strict Validation Audit

## Scope

This audit covers the test-only CardDB schema fixture validator in `Source/WandboundTests/Private/`.

No production importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Validator Behavior

`FWBCardDBSchemaFixtureValidator` reads Unreal-owned JSON fixtures from `Reference/GodotCanon/CardDBSchemaFixtures/` and validates them only inside the `WandboundTests` module.

Current validation covers:

- required schema/card/effect identifiers
- supported card kinds
- supported target requirements
- supported source zones and timing values
- supported payload types and operations
- payload array presence and shape
- malformed activated effect, source gate, and cost gate fields
- numeric validation for payload amounts, status duration, and RR cost
- duplicate effect ids
- player-facing label policy
- hidden-information label policy

Valid fixtures may be mapped into `FWBCardDefinition` for test assertions only.

## Current Diagnostics

Existing diagnostics are stable snake-case strings, including required-field, unsupported-value, malformed-field, numeric-field, player-facing-label, hidden-information, and JSON parse failures.

The validator already fails closed when any diagnostic is emitted: `FWBCardDBSchemaValidationResult::bOk` is false whenever `Diagnostics.Num() > 0`.

## Current Unknown-Field Behavior

Before strict mode, unknown fields are ignored by the test-only fixture validator unless their values are read by an existing validation path such as public labels, hidden-info scans, malformed source/cost gate checks, malformed payload checks, or numeric validation.

This non-strict behavior lets early schema examples carry temporary authoring notes while the Unreal schema is still being shaped.

## Current Metadata Policy

Schema docs reserve `metadata` for explicit authoring-only notes. Metadata is not runtime state, is not a rules input, and is not hidden-info storage.

The current validator does not enforce a metadata allow-list. That is acceptable for exploratory fixture work, but it is too loose for pre-import schema hardening.

## Why Strict Mode Is Useful

Strict unknown-field rejection lets fixtures prove that future importer-facing schema objects fail closed when an authoring field is misspelled, misplaced, or unsupported.

This catches drift before production CardDB import work exists, and it keeps unsupported card behavior from silently becoming an ignored no-op.

## Why Strict Mode Is Explicit And Test-Only

Strict mode should be opt-in because existing non-strict fixtures remain useful compatibility examples. Default validation must continue to behave as it already does.

The validator stays under `WandboundTests` so this pass does not create a production loader, production importer API, production zones, runtime generation path, or gameplay behavior.

## Why Existing Fixtures Must Keep Passing

Existing tests establish the current fail-closed contract for supported payloads, malformed fields, source gates, cost gates, labels, and hidden-info policy. Changing default behavior would make this strict schema pass bigger than intended and could blur the boundary between test fixtures and future production import.

## Why Production Importer Remains Deferred

Production import still needs a separately designed boundary for CardDB ownership, packaging, source versioning, zone visibility, runtime provider input, and EffectRunner-backed execution. This pass only validates fixture shape and diagnostics so that later importer work has a clearer contract to build against.
