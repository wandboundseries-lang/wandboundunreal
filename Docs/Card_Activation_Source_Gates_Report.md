# Card Activation Source Gates Report

## Purpose

This pass adds fixture-driven source legality gates for card activation candidates. It does not add production CardDB loading, real card zones, real cost payment, response windows, or activation entries to `FWBAction`.

## Core Additions

- `EWBCardActivationSourceZone`
- `EWBCardActivationTimingRequirement`
- `FWBCardActivationSourceGateDefinition`
- `FWBCardActivationSourceGateContext`
- `FWBCardActivationSourceGateResult`
- `WBCardActivationSourceGate`
- `FWBGameStateData::ActivationUsageKeysThisTurn`

`FWBCardEffectDefinition` now owns a `SourceGate`, and `FWBCardActivationCandidateSource` now owns `SourceGateContext`.

## Gate Behavior

`WBCardActivationSourceGate::Evaluate` is read-only. It checks player, game-over state, fixture zone metadata, timing, source unit requirement/ownership, Stunned/Frozen policy, external cost-satisfied flag, and once-per-turn usage keys.

Reason strings are stable and non-secret:

- `invalid_player`
- `game_over`
- `source_zone_mismatch`
- `source_unit_required`
- `source_unit_missing`
- `source_unit_removed`
- `source_unit_owner_mismatch`
- `source_unit_stunned`
- `source_unit_frozen`
- `timing_not_normal_turn_priority`
- `response_window_not_supported`
- `costs_not_satisfied`
- `once_per_turn_already_used`
- `usage_key_missing`

## Candidate Filtering

`WBCardActivationCandidateGenerator` evaluates each effect's source gate before target expansion. Failed gates skip that effect/source and preserve source/effect/target ordering for remaining candidates.

Legacy default source filtering remains in place for existing fixtures: default candidate generation still excludes removed, defeated, Stunned, and Frozen sources. Explicit `source_gate` or `source_gate_context` fixtures can exercise the configurable Frozen policy.

## Usage Keys

Fixture usage keys are stored on `FWBGameStateData` and are not exposed in public summaries. `Evaluate` never marks usage. `MarkUsageIfAllowedForTest` exists only for fixture/test scaffolding.

Usage keys clear for the player when `ApplyTurnStartResourceSetupForPlayer` succeeds.

## Follow-Up - Usage Marking

Source gates now pair with activation command usage marking. When expansion builds a command for an effect whose source gate is `bOncePerTurn`, the command carries a fixture-owned usage commit. `WBEffectRunner::ApplyCardActivationCommand` marks that key only after successful effect resolution.

The key remains internal and resets through the existing turn-start resource setup path.

## Follow-Up - Detailed Cost Gates

Source gates now support `FWBCardActivationCostGateDefinition` and `FWBCardActivationCostGateContext` for fixture-owned externally supplied RR/RL affordability.

The detailed gate is read-only and filters candidate generation only. It does not inspect production zones, pay costs, mutate `RLUsed`, trigger overflow, or destroy wands. If the legacy external cost-satisfied flag and the detailed gate are both present, both must pass.

## Follow-Up - Affordability Query Consumer

Source gates remain the consumer boundary for affordability context. `WBCardActivationAffordability` can compute and project read-only RL/RR affordability into source-gate contexts, but source gates still only validate supplied context and never pay costs or mutate state.

## Follow-Up - Fixture Source-Zone Ownership

Source gates now support fixture-only zone ownership checks through `FWBCardActivationFixtureZoneEntry` and `FWBCardActivationFixtureZoneContext`.

When `bRequiresFixtureZoneOwnership` is set, the gate requires exactly one fixture entry matching `SourceCardId`, player id, and required zone. Hand and Discard entries validate fixture ownership only. Equipped entries also require `EquippedToUnitId` to match the source unit id, while existing source-unit checks validate ownership, removal, and status.

Deck activation is explicitly unsupported in this scaffold with `source_zone_deck_not_supported`. Legacy `Fixture` source behavior remains compatible without fixture zone metadata.

## Follow-Up - Board Source Parity

Source gates now support Board-source unit/card-id parity. When `RequiredZone = Board` and fixture ownership is enabled, `WBCardActivationSourceGate::EvaluateBoardSourceParity` validates the source unit, ownership policy, on-board state, and visible unit `CardId` against the source activation card id.

Board-source parity uses `FWBUnitState::CardId` as the source of truth and does not require `FixtureZoneContext` entries. Hand, Equipped, Discard, Deck unsupported, and legacy Fixture behavior remain unchanged.

## Follow-Up - Unreal CardDB Schema Planning

The future CardDB schema defines the authoring shape for `source_gate`.

Schema fields should map into `FWBCardActivationSourceGateDefinition`: required zone, timing, source-unit ownership, status blockers, cost gate, fixture ownership, once-per-turn policy, and usage key. Unsupported source zones or timings must fail closed during schema validation.

## Fixture Support

Added parser support for:

- `source_gate`
- `source_gate_context`
- `effect_source_gate_contexts`
- `initial_state.activation_usage_keys_this_turn`
- `operation.kind = evaluate_card_activation_source_gate`
- `operation = query_card_activation_affordability`

Added GodotCanon source-gate fixtures covering ownership, Stunned/Frozen policy, zones, timing, external costs, once-per-turn usage, turn-start reset, and ordered candidate filtering.

## Guardrails

Confirmed unchanged:

- activation candidates remain separate from `FWBAction`
- activation legal actions remain separate from `FWBAction`
- `WBRules::GenerateLegalActions` output remains unchanged
- `WBActionCodec` output remains unchanged
- no Godot CardDB import
- no UI, response windows, negation, passives, wands, card-specific behavior, Blueprints, `.uasset`, or `.umap` work

## Remaining Work

- production card database import
- real hand/deck/discard/equipped zones
- production RL/RR affordability ownership and payment
- response-window activation legality
- effect negation
- passives and wands
- card-specific behavior
- UI target selection
