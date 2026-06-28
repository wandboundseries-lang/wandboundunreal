# Card Definition Fixture Loader Report

Date: 2026-06-28

## Purpose

This pass adds a minimal production-safe WandboundCore fixture loader for Unreal-owned card definition repositories.

The loader ingests a narrow JSON fixture shape, populates `FWBCardDefinitionRepository`, reuses `WBCardDefinitionRepository` validation, and returns deterministic diagnostics instead of crashing.

This is not a Godot CardDB importer and not the full future CardDB loader.

## Fixture Format

Fixture files live under:

```text
Reference/GodotCanon/CardDefinitionRepositoryFixtures/
```

Top-level fields:

- `repository_id` required
- `source_version` optional
- `cards` required non-empty array

Card fields:

- `card_id` required
- `public_name` required
- `kind` optional
- `activated_effects` optional

Effect fields:

- `effect_id` required
- `public_label` required
- `target_requirement` required
- `source_gate` optional
- `payloads` required non-empty array

Supported target requirements are `none`, `unit`, `tile`, and `wall_edge`.

## Supported Payloads

The loader maps only the currently supported generic payload families:

- `damage_effect` -> `FWBDamageEffectRequest`
- `heal_effect` -> `FWBHealEffectRequest`
- `status_effect` -> `FWBStatusEffectRequest`
- `armor_effect` -> `FWBArmorEffectRequest`

Unsupported payload types and unsupported operations fail closed.

## Source Gate Support

Supported source zones:

- `fixture`
- `board`
- `hand`
- `discard`
- `equipped`

`deck` and unknown zones fail with `unsupported_source_zone`.

Supported timing:

- `any`
- `normal_turn_priority`

`response_window` fails with `unsupported_timing` until response integration exists.

Cost gate parsing supports external affordability flags, `required_rr`, and `cost_kind`. Negative or malformed RR values fail with `invalid_rr_cost`.

## Fail-Closed Diagnostics

The loader returns `FWBCardDefinitionFixtureLoadResult`.

On any diagnostic:

- `bOk=false`
- `Reason=fixture_load_failed`
- `Repository` is emptied

Diagnostics preserve deterministic input order and include safe context:

- `Code`
- `CardId`
- `EffectId`
- `Path`

The loader fails closed on malformed JSON, missing required fields, malformed arrays or objects, unknown fields, unsupported card kind, unsupported target requirement, unsupported source zone, unsupported timing, unsupported payload type or operation, invalid numeric fields, invalid statuses, invalid RR costs, and repository validation failure.

## Repository Validation Reuse

After parsing succeeds without loader diagnostics, the loader calls `WBCardDefinitionRepository::ValidateRepository`.

Repository validation still owns duplicate-card checks, duplicate-effect checks, missing id checks, missing public names, supported target requirement checks, and player-facing public-label internal-term guards.

Repository validation failures are surfaced with the original repository reason plus `repository_validation_failed`.

## Expected Export Snapshots

Expected deterministic exports live under:

```text
Reference/GodotCanon/CardDefinitionRepositoryFixtures/ExpectedExports/
```

They cover valid damage and mixed repositories plus duplicate card id, unsupported payload, bad public label, and hidden-token-safe failures.

The export shape records:

- `ok`
- `reason`
- clean `source_path`
- `repository_id`
- `source_version`
- `card_count`
- sorted `card_ids`
- sanitized diagnostics

## Hidden-Token Safety

The hidden-token fixture includes a secret payload type value. Loader diagnostics do not echo unsupported payload values, and the expected export contains no `SECRET` token.

Player-facing public names and labels still pass through repository internal-term guards.

## Boundaries

This pass did not add:

- Godot CardDB import
- full production CardDB loading
- runtime/provider integration
- activation legal action generation
- activation candidates
- effect execution
- zone movement
- response windows
- target picking
- `FWBAction` activation integration
- `WBActionCodec` changes
- `WBRules::GenerateLegalActions` changes
- UI, Blueprint, `.uasset`, or `.umap` changes

The loader is data ingestion only.

## Production Provider Test Use

Fixture-loaded repositories can be supplied to `FWBProductionActivationDataProvider` tests.

The provider still consumes only Unreal-owned `FWBCardDefinitionRepository` data after loading; it does not import Godot CardDB or inspect `Reference/GodotProject`.

## Next Planned Pass

Add read-only target option enumeration for provider-emitted activation source/effect choices.
