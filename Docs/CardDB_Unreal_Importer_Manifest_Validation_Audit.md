# CardDB Unreal Importer Manifest Validation Audit

## Purpose

This audit records the current test-only CardDB importer readiness stack before adding manifest validation above named batch-readiness sets.

Manifest validation remains test-only. It is not a production CardDB importer, production loader, production zone model, runtime activation path, gameplay rule, schema migration system, or EffectRunner path.

## Current Importer-Readiness Behavior

`FWBCardDBImporterReadinessForTests` lives under `Source/WandboundTests/Private`.

It evaluates one bundle fixture through the test-only bundle validator, applies optional source-version compatibility options, copies dependency order, and produces ordered validated `FWBCardDefinition` values for tests.

Readiness succeeds only when bundle validation passes, dependency order exists for non-empty bundles, and every dependency-order card id resolves to a validated card definition. Not-ready bundles are ordinary readiness results, not crashes or production import attempts.

## Current Diagnostic-Summary Behavior

`FWBCardDBImporterDiagnosticSummaryForTests` consumes readiness results and groups:

- total bundle count
- ready bundle count
- not-ready bundle count
- readiness reason counts
- schema diagnostic code counts
- affected bundle counts
- affected card-context counts

Summary output is aggregate-only and deterministic. It omits diagnostic messages, full source JSON, card ids, effect ids, payload bodies, and hidden values.

## Current Batch-Readiness Behavior

`FWBCardDBImporterBatchReadinessForTests` evaluates a named list of bundle entries.

It preserves caller-supplied bundle order, calls the readiness helper for each entry, and uses the diagnostic-summary helper to build grouped batch output.

Batch `bOk` means the batch evaluation ran. A batch may be `bOk=true` while some bundles are not ready. Missing bundle paths are batch-level failures.

## Current Expected Export Behavior

Expected exports under `Reference/GodotCanon/CardDBSchemaFixtures/ExpectedExports/` use condensed deterministic JSON snapshots.

Readiness exports provide per-bundle readiness details. Diagnostic summary exports provide aggregate failure grouping. Batch readiness exports combine input-ordered per-bundle readiness with grouped summary arrays.

All exports sanitize hidden tokens and avoid full JSON snippets.

## Proposed Manifest Shape

A manifest should describe named batch-readiness sets:

- `manifest_schema_version`
- `manifest_id`
- optional `description`
- optional `metadata`
- optional `default_compatibility`
- `batches`

Each batch has a `batch_name`, optional metadata/description/compatibility, and a non-empty `bundles` array.

Each bundle entry has a stable `name`, relative fixture `path`, optional metadata, and optional compatibility override.

Compatibility flows from manifest default to batch override to bundle override. The resolved compatibility object maps into `FWBCardDBSchemaValidationOptions`.

## Why Manifest Validation Is Useful

Single-bundle readiness proves individual bundle behavior. Batch readiness proves named bundle groups.

Manifest validation adds the next planning layer before production importer work:

- multiple batch sets can live in one fixture
- duplicate batch names fail before evaluation
- duplicate bundle aliases fail before evaluation
- missing or unsafe bundle paths fail before evaluation
- compatibility override behavior is tested at manifest scope
- one deterministic manifest export can summarize all batch outputs

## Hidden-Information Policy

Manifest validation must reject hidden tokens in manifest ids, batch names, bundle names, descriptions, metadata strings, paths, and compatibility string values.

Diagnostics and exports must not echo hidden values. Manifest output should include stable diagnostic codes, safe context fields, batch summaries, bundle names, and grouped summary arrays only.

Manifest exports must not include full source JSON, hidden values, diagnostic messages, payload bodies, card text, or internal runtime labels.

## Test-Only Boundary

The manifest helper should live under `WandboundTests`, parse fixture JSON, validate manifest schema fields, resolve safe fixture paths, and call the batch-readiness helper.

It must not be included by `Source/WandboundCore` or `Source/WandboundRuntime`.

It must not execute effects, mutate game state, generate activation candidates, generate activation legal actions, create production zones, or expose production parser/import APIs.

## Production Importer And Loader Deferred

Production importer work still needs loader/storage lifetime, zone visibility, source-version ownership, migration policy, provider integration, runtime consumption, and tooling/logging policy.

Manifest validation only supplies test artifacts for planning those future boundaries.
