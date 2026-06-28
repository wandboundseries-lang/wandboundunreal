# CardDB Unreal Importer Batch Readiness Audit

## Purpose

This audit records the current test-only importer readiness and diagnostic summary behavior before adding a named batch-readiness helper.

Batch readiness remains test-only. It is not a production CardDB importer, production loader, production zone model, runtime activation path, gameplay rule, schema migration system, or EffectRunner path.

## Current Importer-Readiness Behavior

`FWBCardDBImporterReadinessForTests` lives under `Source/WandboundTests/Private`.

It evaluates one bundle fixture by calling the test-only bundle validator, copying dependency order, and producing ordered validated `FWBCardDefinition` values for test inspection.

A bundle is ready only when:

- bundle validation succeeds
- optional source-version compatibility succeeds when requested
- dependency order exists for the non-empty bundle
- each dependency-order card id resolves to a validated `FWBCardDefinition`

Not-ready bundles are expected results and do not imply a batch evaluation failure.

## Current Readiness Export Behavior

Readiness export emits deterministic condensed JSON containing:

- `ready`
- `reason`
- clean `source_path`
- sanitized `carddb_version`
- sanitized `source_version`
- `card_count`
- `dependency_order_card_ids`
- `ordered_card_ids`
- sanitized diagnostic context

It omits diagnostic messages, full source JSON, public labels, public text, payload bodies, and hidden referenced values.

## Current Diagnostic-Summary Behavior

`FWBCardDBImporterDiagnosticSummaryForTests` groups one or more readiness results.

It records:

- total bundle count
- ready bundle count
- not-ready bundle count
- readiness reason counts
- diagnostic code counts
- affected bundle counts per diagnostic
- affected card-context counts per diagnostic
- sorted sanitized ready/not-ready source filenames

Summary export is deterministic and aggregate-only. It does not include card ids, effect ids, diagnostic messages, or full source JSON.

## Why Batch Readiness Is Useful

Single-bundle readiness proves individual fixtures. Diagnostic summaries prove aggregate failure groupings. Batch readiness ties both together for named fixture sets:

- evaluate a curated bundle set in a stable order
- preserve per-bundle readiness outcome
- produce one grouped diagnostic summary
- compare one deterministic batch snapshot
- prove a future importer can report both per-bundle and aggregate readiness before any production importer exists

## Proposed Batch Fixture Set Behavior

A batch entry should provide:

- a stable display name
- a bundle fixture path
- validation/readiness options

Batch output should preserve input order for per-bundle entries. The grouped summary should use the existing deterministic summary helper.

Batch `bOk` means "the batch evaluation ran." It can be true even when some bundles are not ready. Missing bundle paths are batch errors because they prevent evaluation of the named set.

## Hidden-Information Policy

Batch output must use clean sanitized filenames and stable names only.

Batch output must not contain:

- hidden source values
- full source JSON
- diagnostic messages
- card ids beyond dependency-order card ids already treated as public authoring ids in readiness exports
- effect ids
- public labels/text
- payload bodies

Hidden-token fixtures must not leak `SECRET` or hidden marker terms into batch snapshots.

## Test-Only Boundary

The batch helper should live under `WandboundTests`, call existing readiness and summary helpers, and serialize fixtures for tests.

It must not be included by `Source/WandboundCore` or `Source/WandboundRuntime`.

It must not execute effects, mutate game state, generate activation candidates, generate activation legal actions, create production zones, or expose production parser/import APIs.

## Production Importer And Loader Deferred

Production importer work still needs loader/storage lifetime, zone visibility, source-version ownership, migration policy, provider integration, and runtime consumption decisions.

Batch readiness only supplies test artifacts for planning those future boundaries.
