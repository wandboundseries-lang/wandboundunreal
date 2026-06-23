# Card Activation Usage Marking Audit

## Scope

This audit records the Godot behavior reference and the chosen Unreal fixture behavior for once-per-turn activation usage marking. Godot files were inspected as read-only reference only.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot Behavior

`rules.gd` contains helper counters and markers for unit and wand activations:

- `_wand_activation_used_count`
- `_mark_wand_activation_used`
- `_unit_activation_used_count`
- `_mark_unit_activation_used`
- `_mark_red_ledger_unit_activation_used_if_resolved`

Legal action enumeration skips unit activations whose `once_per_turn` counter is already used and wand activations whose `once_per_turn_per_copy` counter has reached the equipped-copy count.

In the normal `activate` action path, Godot validates the source unit, source card/effect, and once-per-turn counters, then usually marks usage when the activation is accepted and `pending_effect_activation` is opened. Unit activations with the Red Ledger group-heal special case defer marking through `_mark_red_ledger_unit_activation_used_if_resolved`, which runs after pending effect resolution.

`autoload/Game.gd` mirrors the runtime/UI path: it records `pending_effect_activation`, marks most wand/unit activations when targeting/confirmation accepts the activation, and uses the Red Ledger deferred helper for that special case.

`effect_runner.gd` includes separate once-per-turn passive/hook usage keys. Those are related to passive triggers and response hooks, not the fixture-owned card activation command loop in this pass.

`game_state.gd` loads CardDB data and stores runtime unit/card state, including copied activations and resource fields. This pass does not import CardDB or reproduce the production zone/cost system.

## Declaration, Resolution, Failure, And Negation

Godot generally marks ordinary once-per-turn unit/wand activation usage at activation acceptance/opening, before the pending effect fully resolves. The Red Ledger group-heal helper is an observed exception that marks after resolution if the special activation resolves.

Negation and pending response behavior exist in Godot through `pending_effect_activation` and effect negation directives. This Unreal pass intentionally has no response window or negation layer, so it cannot yet match those edge cases exactly.

Chosen Unreal fixture behavior:

- mark usage only after `WBEffectRunner::ApplyEffectRequest` succeeds on the working state
- failed activation does not mark usage
- invalid usage commit fails atomically before original state mutation
- already-used keys fail with `once_per_turn_already_used`
- no usage key is serialized in trace/public outputs

## Usage Key Scope

Godot unit activation usage is effectively scoped to a unit plus source card/effect activation index. Wand activation usage is scoped to unit plus wand copy/card plus activation index. Some passive/hook usage keys use event/passive paths.

Current Unreal scaffold uses fixture-owned string usage keys stored per player in `FWBGameStateData::ActivationUsageKeysThisTurn`. Source gates and activation expansion may use explicit fixture keys or deterministic defaults derived from player, source unit, card id, and effect id.

## Reset Timing

Godot clears or advances turn-scoped usage through its turn flow and per-turn state. The existing Unreal scaffold clears `ActivationUsageKeysThisTurn` for a player during `ApplyTurnStartResourceSetupForPlayer`.

## Current Unreal Scaffold

Before this pass:

- `FWBCardActivationSourceGateDefinition` and `FWBCardActivationSourceGateContext` existed.
- Source gates could reject used once-per-turn keys.
- `FWBGameStateData` stored fixture activation usage keys.
- Turn-start resource setup cleared those keys.
- Candidate generation filtered through source gates.
- Activation command application did not yet mark usage.

## Deferred Items

- production CardDB import
- real hand/deck/discard/equipped zones
- real RL/RR costs and payment mutation
- response windows
- effect negation
- usage semantics for negated activations
- card-specific timing
- UI target selection
- passives and wands
- card-specific prevention/replacement
