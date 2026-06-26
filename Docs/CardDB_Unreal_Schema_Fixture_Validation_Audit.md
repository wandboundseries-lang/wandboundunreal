# CardDB Unreal Schema Fixture Validation Audit

## Scope

This audit supports a test-only CardDB schema fixture validation pass. The validator and fixtures are for `WandboundTests` coverage only. They do not implement a production importer, production loader, production zones, runtime generation, UI target picking, response windows, passives, wands, or activation `FWBAction` integration.

## Schema Docs Inspected

- `Docs/CardDB_Unreal_Schema_Audit.md`
- `Docs/CardDB_Unreal_Schema_Plan.md`
- `Docs/CardDB_Unreal_Schema_Examples.md`
- `Docs/Runtime_Activation_CardDB_Zone_Provider_Plan.md`
- `Docs/Card_Effect_Request_Scaffolding_Report.md`
- `Docs/Card_Definition_Activation_Expansion_Report.md`
- `Docs/Card_Activation_Source_Gates_Report.md`
- `Docs/Card_Activation_Cost_Gates_Report.md`
- `Docs/Card_Activation_Usage_Marking_Report.md`
- `Docs/Card_Activation_Candidate_Generation_Report.md`

## Core Headers Inspected

- `Source/WandboundCore/Public/WBCardDefinition.h`
- `Source/WandboundCore/Public/WBEffectRequest.h`
- `Source/WandboundCore/Public/WBArmorEffect.h`
- `Source/WandboundCore/Public/WBStatusEffect.h`
- `Source/WandboundCore/Public/WBDamageEffect.h`
- `Source/WandboundCore/Public/WBHealEffect.h`
- `Source/WandboundCore/Public/WBCardActivationSourceGate.h`

## Current Supported Payload Families

The current Unreal generic payload families are:

- `armor_effect`, mapping to `FWBArmorEffectRequest`
- `status_effect`, mapping to `FWBStatusEffectRequest`
- `damage_effect`, mapping to `FWBDamageEffectRequest`
- `heal_effect`, mapping to `FWBHealEffectRequest`

Unsupported payload types must fail closed with deterministic diagnostics.

## Current Required Card And Effect Fields

Fixture validation should require:

- `schema_version = 1`
- non-empty `card_id`
- non-empty `public_name`
- supported `kind`
- non-empty `effect_id` for every activated effect
- unique `effect_id` values within a card
- supported `target_requirement`
- supported payload `type`

Empty `activated_effects` remains allowed for vanilla/passive cards because passive and production CardDB behavior is not part of this pass.

## Current Source Gate, Cost, And Usage Fields

`FWBCardActivationSourceGateDefinition` currently supports:

- `required_zone`
- `timing`
- fixture zone ownership flag
- source unit flags
- Stunned/Frozen blockers
- external cost satisfied flag
- `cost_gate`
- `once_per_turn`
- `once_per_turn_key`
- explicit source gate marker

`FWBCardActivationCostGateDefinition` currently supports:

- external affordability requirement
- `RequiredRR`
- `CostKind`

Usage marking remains execution-path scaffolding. The validator may parse once-per-turn metadata, but it does not mark usage or mutate state.

## Current Player-Facing Label Restrictions

Player-facing fields must not expose internal terms such as:

- `EffectRunner`
- `Rules`
- raw payload type names
- schema field names
- hook names
- tile implementation field names
- player ids

Labels and public text must also avoid hidden-information tokens such as:

- `SECRET_`
- `hidden_hand`
- `opponent_hand`
- `deck_card_id`
- `marker_identity`

## Planned Fail-Closed Diagnostics

The test-only validator covers:

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

## Why This Pass Is Test-Only

The current need is to prove schema expectations and fail-closed diagnostics with small Unreal-owned fixtures. Keeping the validator under `WandboundTests` avoids accidentally creating production import APIs before production zones, visibility, response windows, and broader effect mapping are designed.

## Why Production Importer Remains Deferred

Production import is deferred because:

- production hand/deck/discard/equipped zones do not exist yet
- response-window activation is not modeled
- unsupported Godot/CardDB effect families are still out of scope
- hidden-information policy needs production observation boundaries
- activation still intentionally remains separate from `FWBAction`
- runtime providers consume external data and must not become import owners
