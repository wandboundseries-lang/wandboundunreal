# CardDB Unreal Bundle Source Version Compatibility Audit

## Purpose

This audit records the current test-only CardDB bundle source version behavior before adding an opt-in compatibility matrix for future importer planning.

This is not a production CardDB importer, loader, migration system, runtime activation path, zone model, gameplay rule, or EffectRunner path.

## Current Source Version Format Behavior

Bundle `source_version` is optional.

When present, the current test-only bundle validator requires it to be a string, max 64 characters, using only letters, numbers, underscore, hyphen, dot, and slash.

Malformed values fail with:

```text
invalid_source_version
```

The current validator does not decide whether a validly formatted `source_version` is supported by Unreal. Any valid format passes unless another bundle/card diagnostic fails.

## Current Bundle Metadata And Version Behavior

Bundle `carddb_version` is required and must be a non-empty string, max 64 characters, using letters, numbers, underscore, hyphen, and dot.

Bundle `migration_notes` is optional, must be a string when present, has a max length of 512 characters, and fails hidden-information policy if it contains hidden tokens.

Bundle `metadata` is optional. When present, it must be an object with allowed authoring fields and fixed value types:

- `author`: string
- `notes`: string
- `source`: string
- `version`: string
- `test_only`: boolean

Strict mode rejects unknown bundle metadata fields separately from malformed metadata values.

## Current Export Snapshot Behavior

Bundle validation exports deterministic test-only JSON with:

- `ok`
- sanitized clean-file `source_path`
- `card_count`
- `dependency_order_card_ids`
- sanitized diagnostics containing code, card index, card id, effect id, and JSON path

Export snapshots omit diagnostic messages, full source JSON, public labels, payload bodies, metadata values, migration note values, hidden referenced values, and matrix internals.

Invalid bundles currently leave `dependency_order_card_ids` empty because dependency order is skipped whenever bundle diagnostics exist.

## Why Compatibility Diagnostics Are Useful

Format validation only proves that `source_version` is shaped safely. It does not prove the future importer understands the source data family or knows whether that source can be accepted at the target Unreal schema version.

An opt-in compatibility matrix gives fixture coverage for fail-closed importer planning before any production loader exists. It can prove:

- current target source versions pass directly
- explicitly supported older source versions pass directly
- explicitly supported source-to-target transitions pass
- unsupported source versions fail closed
- unsupported transitions fail closed
- missing source versions can be required by a future importer boundary

## Proposed Target Source Version Model

Compatibility validation uses a test-only target source version supplied through validation options.

When compatibility validation is enabled, `TargetSourceVersion` must be non-empty and must pass the same safe `source_version` format policy as bundle source versions.

The target version is validation policy only. It is not exported, not loaded into runtime, and not used to mutate fixture data.

## Proposed Direct Support Policy

A bundle passes compatibility if its `source_version` equals the target source version.

A bundle also passes if its `source_version` appears in `DirectlySupportedSourceVersions`.

Direct support means "the future importer may accept this source shape as-is after validation." It does not mean any production importer exists in this pass.

## Proposed Transition Support Policy

A bundle passes compatibility if an explicit transition exists whose `FromSourceVersion` equals the bundle `source_version` and whose `ToSourceVersion` equals the target source version.

Transition support means "a future importer plan recognizes this source-to-target relationship." It does not perform migration, rewrite fields, alter parsed cards, or change dependency order beyond normal invalid-bundle diagnostics.

## No Migration Logic

This pass must not implement schema migration. The matrix only validates declarations. It does not:

- rewrite fixture JSON
- transform cards
- map old payloads to new payloads
- change dependency references
- change parsed `FWBCardDefinition` data
- expose production parser/export APIs

Unsupported source versions and unsupported transitions fail closed with stable diagnostics instead of attempting fallback behavior.

## Test-Only Boundary

The compatibility matrix belongs only to `Source/WandboundTests/Private/WBCardDBSchemaFixtureValidator.*`.

It must not be included by `Source/WandboundCore` or `Source/WandboundRuntime`.

The validator may parse bundle fixture JSON, validate bundle fields, emit deterministic test diagnostics, and produce `FWBCardDefinition` values for validation assertions only.

## Production Importer And Loader Deferred

Production CardDB import, loading, zones, runtime activation generation, target picking, response windows, passives, wands, and activation `FWBAction` integration remain future milestones.

No runtime behavior should depend on these fixtures or on the compatibility matrix.

## Hidden-Information Policy

Diagnostics and expected exports must not echo hidden values or full source snippets.

`source_version` values may be safe authoring identifiers, but compatibility diagnostics should avoid echoing them in messages. Export snapshots should contain only stable diagnostic code and JSON path context.

Hidden tokens in source version fields should continue to fail before compatibility diagnostics through existing invalid-format or hidden-info policy where applicable, and snapshots must not contain the hidden token.
