# CardDB Unreal Importer Manifest Suite Audit

> **Historical / Superseded**
>
> This audit records the pre-implementation state and is retained for engineering history. It is not current implementation authority. See `Docs/CardDB_Unreal_Importer_Manifest_Suite_Report.md` and commit `7625719c2c02a57e998a872d9f369d4078e94f8f`.

## Purpose

This audit records the current test-only CardDB importer manifest stack before adding suite-level aggregation across multiple manifest fixtures.

Manifest suite aggregation remains test-only. It is not a production CardDB importer, production loader, production zone model, runtime activation path, gameplay rule, schema migration system, or EffectRunner path.

## Current Manifest Validator Behavior

`FWBCardDBImporterManifestForTests` lives under `Source/WandboundTests/Private`.

It validates one manifest fixture, rejects malformed manifest fields, duplicate batch names, duplicate bundle aliases, unsafe or missing bundle paths, malformed metadata, malformed compatibility, and hidden-information tokens.

If manifest validation succeeds, it evaluates each named batch through `FWBCardDBImporterBatchReadinessForTests`.

If manifest validation fails, no batch readiness evaluation runs.

## Current Manifest Export Behavior

Manifest exports are deterministic condensed JSON.

Valid manifest exports include:

- manifest ok state
- manifest id
- batch count
- batch-level readiness counts
- batch bundle names
- batch grouped summaries

Invalid manifest exports include manifest diagnostics only.

Manifest exports do not include full per-bundle readiness exports, full source JSON, diagnostic messages, payload bodies, public text, or hidden values.

## Current Batch-Readiness Behavior

`FWBCardDBImporterBatchReadinessForTests` evaluates named bundle entries in input order.

It calls the readiness helper for each bundle and builds one grouped diagnostic summary from those readiness results.

Batch `bOk` means the batch evaluation ran. A batch may be `bOk=true` while some bundles are not ready. Missing bundle paths are batch-level failures, but manifest validation already rejects not-found paths before batch evaluation.

## Current Diagnostic-Summary Behavior

`FWBCardDBImporterDiagnosticSummaryForTests` groups readiness results by stable reason and schema diagnostic code.

It also counts affected bundles and affected card contexts. It omits diagnostic messages, card ids, effect ids, full JSON snippets, payload bodies, and hidden values.

## Proposed Suite Shape

A suite should describe ordered named manifest entries:

- `suite_schema_version`
- `suite_id`
- optional `description`
- optional `metadata`
- `manifests`

Each manifest entry should provide:

- stable `name`
- relative manifest `path`
- optional `metadata`

Duplicate manifest names fail closed. Duplicate paths are allowed when aliases differ because a suite may intentionally compare the same manifest fixture under different names.

## Why Suite Aggregation Is Useful

Single manifests prove one named set of batches. Suites provide a planning layer for larger importer readiness passes:

- multiple manifests can be evaluated from one fixture
- manifest aliases can be tracked in order
- suite-level counts summarize manifests, batches, bundles, ready bundles, and not-ready bundles
- one aggregate diagnostic summary can represent readiness failure groupings across all valid manifests
- suite validation can reject bad manifest references before any production loader exists

## Hidden-Information Policy

Suite validation must reject hidden tokens in suite ids, manifest aliases, descriptions, metadata strings, and paths.

Diagnostics and exports must not echo hidden values. Suite output should include stable diagnostic codes, safe context fields, manifest aliases, aggregate counts, and grouped summary arrays only.

Suite exports must not include full source JSON, hidden values, diagnostic messages, manifest exports, batch exports, bundle readiness exports, payload bodies, card text, or internal runtime labels.

## Test-Only Boundary

The suite helper should live under `WandboundTests`, parse suite fixture JSON, validate suite fields, resolve safe manifest paths, and call the manifest helper.

It must not be included by `Source/WandboundCore` or `Source/WandboundRuntime`.

It must not execute effects, mutate game state, generate activation candidates, generate activation legal actions, create production zones, or expose production parser/import APIs.

## Production Importer And Loader Deferred

Production importer work still needs loader/storage lifetime, zone visibility, source-version ownership, migration policy, provider integration, runtime consumption, and production diagnostics routing.

Manifest suite aggregation only supplies test artifacts for planning those future boundaries.
