# CardDB Unreal Importer Diagnostic Summary Audit

## Purpose

This audit records current test-only importer-readiness and diagnostic export behavior before adding a summary adapter.

The summary adapter remains test-only. It is not a production importer, production loader, production diagnostic framework, zone model, runtime activation path, gameplay rule, or EffectRunner path.

## Current Importer-Readiness Behavior

`FWBCardDBImporterReadinessForTests` lives under `Source/WandboundTests/Private`.

It evaluates a bundle by calling the existing test-only bundle validator, copying dependency order, and ordering validated `FWBCardDefinition` values for test inspection.

A bundle is ready only when:

- bundle validation succeeds
- source-version compatibility succeeds when requested
- dependency order exists for the non-empty bundle
- every dependency-order card id resolves to a validated `FWBCardDefinition`

Ready bundles expose ordered definitions only for tests. Nothing is loaded into runtime.

## Current Readiness Failure Reasons

Stable readiness reasons are:

- empty string for ready bundles
- `bundle_validation_failed`
- `dependency_order_missing`
- `ordered_card_missing`

Reasons are stable tokens and should not include hidden source data.

## Current Schema Diagnostic Codes

Bundle readiness failures carry diagnostics from `FWBCardDBBundleSchemaValidationResult`.

Current diagnostic codes include root-card schema errors, bundle schema errors, cross-reference errors, dependency errors, version/metadata errors, and source-version compatibility errors.

Diagnostic code strings are produced by:

```cpp
FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString
```

These strings are stable fixture assertions and are safe to group in summaries.

## Current Export JSON Behavior

Bundle validation exports deterministic JSON with:

- `ok`
- clean `source_path`
- `card_count`
- `dependency_order_card_ids`
- sanitized diagnostic context

Importer readiness exports deterministic JSON with:

- `ready`
- `reason`
- clean `source_path`
- sanitized `carddb_version`
- sanitized `source_version`
- `card_count`
- `dependency_order_card_ids`
- `ordered_card_ids`
- sanitized diagnostic context

Both export paths omit diagnostic messages, full source JSON, public labels, public text, payload bodies, metadata values, and hidden referenced values.

## Current Hidden-Token Safety Policy

Hidden-token checks currently sanitize or reject strings containing terms such as:

- `SECRET`
- `hidden_hand`
- `opponent_hand`
- `deck_card_id`
- `marker_identity`

Readiness exports sanitize source/version fields and diagnostic context. Summary exports should follow the same policy and use clean filenames only.

## Why Summary Grouping Is Useful

Readiness snapshots prove individual bundle outcomes. A summary adapter gives future importer planning a higher-level view:

- how many bundles are ready
- how many failed
- which stable readiness reasons are common
- which schema diagnostics are common
- how many bundles are affected by each diagnostic
- how many card contexts are affected by each diagnostic

This is useful before production importer work because it can shape fail-closed diagnostics without committing to loader/storage/runtime behavior.

## Test-Only Boundary

The summary adapter should consume readiness results and serialize deterministic summaries for fixture comparison.

It must not:

- import or load CardDB data
- create production zones
- generate activation candidates
- generate activation legal actions
- execute effects
- mutate game state
- expose production parser/import APIs
- include `Source/WandboundCore` or `Source/WandboundRuntime`

## Production Importer And Loader Deferred

Production importer and loader work still needs decisions around storage, version compatibility ownership, migration policy, zone visibility, player-perspective observations, provider integration, and activation action integration.

The summary adapter only helps plan diagnostics for those future decisions.

## Hidden-Information Policy

Summary output must not include full source snippets, diagnostic messages, card ids, effect ids, payload bodies, hidden referenced ids, or unsafe source paths.

Summary output may include stable reason tokens, diagnostic code tokens, aggregate counts, and sanitized clean filenames.
