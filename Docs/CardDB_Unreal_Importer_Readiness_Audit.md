# CardDB Unreal Importer Readiness Audit

## Purpose

This audit records the current test-only CardDB bundle validation state before adding an importer-readiness helper.

Importer readiness in this pass means "safe for a future importer boundary to consider after validation." It is not production import, production loading, runtime activation data generation, zone creation, rules execution, or schema migration.

## Current Bundle Validation Behavior

`FWBCardDBSchemaFixtureValidator` lives under `Source/WandboundTests/Private`.

Bundle validation supports:

- `bundle_schema_version`
- required `carddb_version`
- optional `source_version`
- optional `migration_notes`
- optional bundle `metadata`
- non-empty `cards[]`
- duplicate card-id rejection
- strict unknown-field rejection when requested
- root-card validation for each bundled card
- cross-card reference validation
- dependency ordering
- self-reference and cycle diagnostics
- deterministic validation-result export snapshots

Valid cards map into `FWBCardDefinition` values for validation assertions only. Bundle validation does not load those definitions into runtime and does not expose a production importer API.

## Current Source-Version Compatibility Behavior

Source-version compatibility is opt-in through `FWBCardDBSchemaValidationOptions::SourceVersionCompatibility`.

When enabled, the matrix validates:

- a non-empty target source version
- directly supported source versions
- explicit source-to-target transitions
- optional or required bundle `source_version`

Unsupported source versions fail with `source_version_unsupported`. Unsupported transitions fail with `source_version_transition_unsupported`. Malformed matrix policy fails with `source_version_compatibility_matrix_malformed`.

The compatibility layer is validation policy only. It does not migrate, rewrite, or import source data.

## Current Dependency Order Behavior

Bundle validation derives `DependencyOrderCardIds` from card, effect, and payload references.

Policy:

- dependencies appear before dependents
- original `cards[]` order breaks ties
- invalid bundles leave dependency order empty
- missing references leave dependency order empty
- duplicate card ids leave dependency order empty
- self-references and cycles fail closed

Dependency order is currently test-only validation output and not a runtime load order.

## Current Export Snapshot Behavior

`BundleValidationResultToJsonStringForTest` exports deterministic condensed JSON containing:

- `ok`
- clean-file `source_path`
- `card_count`
- `dependency_order_card_ids`
- sanitized diagnostics

Exports omit diagnostic messages, full source JSON, public labels, public text, payload bodies, metadata values, and hidden referenced values.

## Current FWBCardDefinition Mapping

For each valid bundled card, root-card validation maps into `FWBCardDefinition`.

The mapping is validation-only. The resulting definitions are useful for tests to inspect card ids, effect ids, target requirements, source gates, and payload shapes after schema validation.

The mapping does not:

- create runtime zones
- generate activation candidates
- generate activation legal actions
- execute effects
- mutate game state
- merge activation into `FWBAction`

## Import-Ready Definition Before Production Importer Work

A bundle is import-ready only when:

- bundle validation succeeds
- source-version compatibility succeeds if enabled
- no diagnostics are present
- dependency order is available for the non-empty bundle
- every dependency-order card id resolves to a validated `FWBCardDefinition`
- ordered card definitions can be produced deterministically

Import readiness means a future importer boundary could consider the bundle after validation. It does not mean this pass imports or loads anything.

## Why Readiness Is Validation-Only And Test-Only

Readiness lives in `WandboundTests` so production code cannot depend on fixture parsing or importer-planning helpers.

This helper should call existing bundle validation, order validated card definitions, and export a sanitized readiness snapshot. It must not introduce runtime ownership, production parsing APIs, CardDB loading, zone state, rules mutation, or effect execution.

## Why Production Importer And Loader Remain Deferred

A production importer still needs decisions around:

- production storage and load lifetime
- version compatibility ownership
- migration policy
- zone and visibility models
- player-perspective observation
- activation `FWBAction` integration
- response windows, passives, wands, and card-specific behavior

Those decisions remain future milestones.

## Hidden-Information Policy

Readiness diagnostics and exports must not echo hidden values or full source snippets.

Readiness export should include clean filenames, stable reason strings, safe version identifiers, ordered public card ids, and sanitized diagnostic context only.

Hidden-token fixtures must fail closed and expected exports must not contain `SECRET` or hidden marker terms.
