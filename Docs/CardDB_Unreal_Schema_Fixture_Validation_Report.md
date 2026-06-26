# CardDB Unreal Schema Fixture Validation Report

## Purpose

Added test-only CardDB schema fixture validation for fail-closed diagnostics.

The validator lives under `WandboundTests` and is not a production CardDB importer, production loader, production zone model, or runtime provider.

## Fixture Folder

Fixtures were added under:

```text
Reference/GodotCanon/CardDBSchemaFixtures/
```

These are Unreal-owned schema examples. They are not imported Godot CardDB data and are not placed under `Reference/GodotProject`.

## Valid Fixtures

Valid fixtures:

- `valid_damage_card.json`
- `valid_heal_card.json`
- `valid_status_burn_card.json`
- `valid_armor_card.json`
- `valid_cost_once_per_turn_card.json`

These cover damage, heal, status, armor, source gate, RR cost gate, and once-per-turn usage gate schema shape.

## Invalid Fixtures

Invalid fixtures:

- `invalid_missing_schema_version.json`
- `invalid_missing_card_id.json`
- `invalid_missing_public_name.json`
- `invalid_missing_effect_id.json`
- `invalid_duplicate_effect_id.json`
- `invalid_unsupported_payload_type.json`
- `invalid_unsupported_payload_operation.json`
- `invalid_unsupported_target_requirement.json`
- `invalid_unsupported_timing.json`
- `invalid_negative_rr_cost.json`
- `invalid_bad_status_id.json`
- `invalid_player_facing_label_internal_term.json`
- `invalid_hidden_info_policy_violation.json`

Each invalid fixture verifies one primary fail-closed diagnostic.

## Diagnostics

Supported diagnostic strings:

- `schema_version_missing`
- `card_id_missing`
- `public_name_missing`
- `effect_id_missing`
- `effect_id_duplicate`
- `unsupported_card_kind`
- `unsupported_target_requirement`
- `unsupported_source_zone`
- `unsupported_timing`
- `unsupported_payload_type`
- `unsupported_payload_operation`
- `invalid_rr_cost`
- `invalid_status_id`
- `hidden_info_policy_violation`
- `player_facing_label_contains_internal_term`
- `json_parse_failed`

## Fail-Closed Policy

Validation result `bOk` is false when any diagnostic is emitted.

Unsupported payload types, unsupported payload operations, unsupported target requirements, unsupported timings, invalid RR costs, invalid status ids, bad public labels, and hidden-information policy violations do not silently become no-op card definitions.

## Supported Payload Validation

The validator accepts and maps:

- `damage_effect` to `EWBGenericEffectOp::DamageEffect`
- `heal_effect` to `EWBGenericEffectOp::HealEffect`
- `status_effect` to `EWBGenericEffectOp::StatusEffect`
- `armor_effect` to `EWBGenericEffectOp::ArmorEffect`

Supported armor/status operations are limited to the current Unreal request structs.

## Player-Facing Label Validation

Public names, public text, and effect public labels are scanned for internal implementation terms.

The invalid label fixture fails with:

```text
player_facing_label_contains_internal_term
```

## Hidden-Info Policy Validation

Public names, public text, and effect public labels are scanned for hidden-info leak tokens.

The hidden-info fixture fails with:

```text
hidden_info_policy_violation
```

Diagnostic messages remain generic and do not repeat hidden fixture content.

## FWBCardDefinition Mapping

Valid fixtures produce an `FWBCardDefinition` for validation only.

The tests assert:

- card id mapping
- activated effect count
- target requirement mapping
- payload operation mapping
- status id and duration mapping
- source gate cost and once-per-turn mapping

## Guardrails

Confirmed by source guards:

- `Source/WandboundCore` does not include the test-only validator
- `Source/WandboundRuntime` does not include the test-only validator
- validator files do not call effect execution, normal legal action generation, activation candidate generation, or activation legal action generation paths

## Confirmed Out Of Scope

- no production importer
- no production loader
- no production zones
- no production runtime generation
- no effect execution
- no activation candidate/action generation by production code
- no Godot CardDB import
- no UI target picking
- no response windows
- no passives/wands
- no activation `FWBAction` integration
- no `WBActionCodec` changes
- no `WBRules::GenerateLegalActions` changes
- no Blueprints, `.uasset`, or `.umap` edits
