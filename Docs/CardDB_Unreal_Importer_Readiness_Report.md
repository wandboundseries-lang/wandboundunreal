# CardDB Unreal Importer Readiness Report

## Purpose

Added a validation-only, test-only CardDB importer-readiness helper.

The helper proves that validated compatible bundle fixtures can produce deterministic ordered card definitions for future importer planning. It is not a production importer, loader, runtime provider, zone model, migration system, gameplay rule, or EffectRunner path.

## Validation-Only Import Readiness

A bundle is import-ready when:

- bundle validation passes
- source-version compatibility passes when requested
- no diagnostics are present
- dependency order is available for the non-empty bundle
- each dependency-order card id resolves to a validated `FWBCardDefinition`
- ordered card definitions can be produced deterministically

Readiness does not load data into runtime.

## Readiness Helper Behavior

Added:

```cpp
FWBCardDBImporterReadinessForTests
```

The helper lives under `Source/WandboundTests/Private`.

It calls the existing bundle validator, copies dependency order, orders validated `FWBCardDefinition` values by that order, and exports sanitized readiness snapshots for tests.

## Readiness Result Fields

`FWBCardDBImporterReadinessResultForTest` contains:

- `bReady`
- `Reason`
- `SourcePath`
- `CardDBVersion`
- `SourceVersion`
- `BundleValidationResult`
- `DependencyOrderCardIds`
- `OrderedCardDefinitions`

`CardDBVersion` and `SourceVersion` are sanitized for hidden-token safety before export.

## Ordered Card Definition Behavior

Ready bundles preserve validated `FWBCardDefinition` values in dependency order.

`OrderedCardDefinitions` is validation-only inspection data. It is not a production CardDB cache and is not loaded into runtime.

## Dependency Order Requirement

Readiness requires dependency order for non-empty valid bundles.

If bundle validation succeeds but dependency order is missing, readiness fails with:

```text
dependency_order_missing
```

The current public bundle validator produces dependency order for valid non-empty fixtures, so the missing-order branch is guarded in helper logic and documented rather than represented by a natural public fixture.

If a dependency-order id cannot be resolved to a validated definition, readiness fails with:

```text
ordered_card_missing
```

## Source-Version Compatibility Requirement

Readiness uses the existing validation options. If source-version compatibility is enabled, unsupported source versions and unsupported transitions fail bundle validation before readiness succeeds.

Compatibility failures use the existing diagnostics and readiness reason:

```text
bundle_validation_failed
```

## Failure Reasons

Stable readiness reasons:

- empty string for ready bundles
- `bundle_validation_failed`
- `dependency_order_missing`
- `ordered_card_missing`

Reasons do not include hidden source data.

## Readiness Export Shape

Readiness snapshots export deterministic condensed JSON:

```json
{
  "ready": true,
  "reason": "",
  "source_path": "valid_bundle_dependency_order_simple_chain.json",
  "carddb_version": "fixture_dependency_order_simple_chain",
  "source_version": "",
  "card_count": 3,
  "dependency_order_card_ids": ["base_card", "middle_card", "top_card"],
  "ordered_card_ids": ["base_card", "middle_card", "top_card"],
  "diagnostics": []
}
```

Exports omit diagnostic messages, full source JSON, public labels, public text, payload bodies, and hidden referenced values.

## Expected Export Coverage

Readiness snapshots cover:

- ready current source-version bundle
- ready supported transition bundle
- ready dependency-order bundle
- unsupported source version
- unsupported transition
- missing reference
- dependency cycle
- duplicate card id
- invalid card payload
- hidden-token-safe not-ready bundle

Expected exports live under:

```text
Reference/GodotCanon/CardDBSchemaFixtures/ExpectedExports/
```

## Hidden-Token Export Safety

Hidden-token source versions fail closed through existing validation.

Readiness export sanitizes version fields and diagnostic context. The hidden-token snapshot verifies `SECRET` does not appear in output.

## Diagnostic Summary Integration

Readiness results can now be grouped by the test-only importer diagnostic summary helper.

The summary helper counts ready/not-ready bundles, groups not-ready reasons, groups schema diagnostic codes, counts affected bundles/cards, and exports sanitized aggregate JSON for fixture comparison.

## Batch Readiness Integration

Readiness results can now be evaluated in named test-only batches.

The batch helper calls this readiness helper for each named bundle entry, preserves per-bundle readiness output in input order, and pairs those results with one grouped diagnostic summary.

Batch readiness is still validation-only. It does not load CardDB data into runtime, create zones, execute effects, or generate activation candidates/actions.

## Confirmations

- this remains test-only
- this is not a production importer
- no production loader was added
- no production zones were added
- no Core or Runtime source files were changed
- no effects are executed
- no activation candidates or activation actions are generated by production code
- no runtime activation behavior was changed
- no `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed
- no UI, response windows, Blueprints, `.uasset`, or `.umap` files were changed
