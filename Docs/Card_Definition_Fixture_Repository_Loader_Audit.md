# Card Definition Fixture Repository Loader Audit

Date: 2026-06-28

## Current Repository API

`FWBCardDefinitionRepository` stores:

- `RepositoryId`
- `SourceVersion`
- `Definitions`

`WBCardDefinitionRepository` already provides deterministic validation, build/copy, exact lookup, duplicate-card-id checks, public-label internal-term guards, and deterministic card id / definition ordering.

The repository is separate from `FWBGameStateData`. Game state owns card instances and zones; the repository owns static definitions.

## Current FWBCardDefinition Shape

`FWBCardDefinition` contains `CardId`, `PublicName`, `Kind`, and `ActivatedEffects`.

`FWBCardEffectDefinition` contains `EffectId`, `PublicLabel`, `TargetRequirement`, `Payloads`, and `SourceGate`.

This is enough for a narrow Unreal-owned fixture loader to populate definitions without adding a parallel card schema.

## Current Generic Payload Families

The currently supported generic payload families are:

- `damage_effect` -> `FWBDamageEffectRequest`
- `heal_effect` -> `FWBHealEffectRequest`
- `status_effect` -> `FWBStatusEffectRequest`
- `armor_effect` -> `FWBArmorEffectRequest`

Those payloads are already used by activation expansion and effect-request execution paths. The loader should only map authored data into these structs and must not execute them.

## Current Test-Only Schema Validator Boundary

The existing CardDB schema and bundle validators live under `WandboundTests` and `Reference/GodotCanon/CardDBSchemaFixtures`.

They validate broad future schema behavior and may map valid fixture JSON into `FWBCardDefinition` for tests only. They are not production Core code, not a loader, and not a runtime provider.

This pass adds a narrow Core fixture loader without depending on those test-only validators.

## Chosen Minimal Fixture Format

The loader uses a small Unreal-owned repository JSON shape:

- top-level `repository_id`
- optional `source_version`
- top-level `cards[]`
- card `card_id`, `public_name`, optional `kind`, optional `activated_effects[]`
- effect `effect_id`, `public_label`, `target_requirement`, optional `source_gate`, `payloads[]`
- payload `type` with only the currently supported generic payload families

This is intentionally narrower than the future CardDB schema/importer plan.

## Why This Pass Does Not Import Godot CardDB

The Godot project remains read-only behavior reference material. Godot CardDB data may not be imported, runtime-loaded, or treated as production Unreal data in this pass.

The new fixtures live under `Reference/GodotCanon/CardDefinitionRepositoryFixtures` as Unreal-owned reference fixtures, not under `Reference/GodotProject`.

## Why This Pass Does Not Add Provider Or Runtime Behavior

Runtime continues to consume externally supplied decision data and public summaries. It should not load card repositories, inspect hidden zones, generate legal actions, or own rules truth.

Provider work is a future pass that will bridge card-zone observations, repository lookup, source-gate contexts, and activation candidate generation.

## Fail-Closed Validation Policy

The loader returns diagnostics instead of crashing and keeps the repository empty on failure.

It fails closed on malformed JSON, missing required fields, malformed arrays/objects, unknown fields, unsupported card kinds, unsupported target requirements, unsupported source zones, unsupported timing, unsupported payload types/operations, invalid numeric fields, invalid statuses, invalid RR costs, and repository validation failure.

Diagnostic order is deterministic:

1. top-level
2. cards in input order
3. effects in input order
4. payloads in input order

Diagnostics include safe card/effect id context and stable JSON paths. They must not echo hidden values.

## Hidden-Info And Public-Label Policy

Player-facing card/effect labels are checked through `WBCardDefinitionRepository` public-label guards.

Export snapshots must not include hidden tokens such as `SECRET`. Invalid hidden-token fixtures should fail with a safe diagnostic and sanitized export.

## Risks And Open Questions

- The fixture format is intentionally narrow and may not match the final production CardDB importer shape.
- Additional public text fields may need their own public-label/internal-term checks later.
- Source gates are parsed only for the currently represented fields.
- Fixture dependency ordering, manifests, and schema migration remain test-only/future production importer work.
