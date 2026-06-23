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

## Fixture Support

Added parser support for:

- `source_gate`
- `source_gate_context`
- `initial_state.activation_usage_keys_this_turn`
- `operation.kind = evaluate_card_activation_source_gate`

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
- real RL/RR cost checks and payment
- response-window activation legality
- effect negation
- passives and wands
- card-specific behavior
- UI target selection
