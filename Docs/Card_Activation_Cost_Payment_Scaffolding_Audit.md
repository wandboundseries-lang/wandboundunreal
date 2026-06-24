# Card Activation Cost Payment Scaffolding Audit

## Scope

This audit was completed before adding Unreal cost-payment scaffolding. Godot reference files were inspected as read-only behavior references; no Godot files were modified, compiled, imported, or runtime-loaded by Unreal.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot RL/RR Model

Godot represents unit RL capacity and reservation separately:

- `rl_total` is the unit's effective RL capacity.
- `rl_used` is reserved RL.
- available RL is computed as `max(0, rl_total - rl_used)`.
- card RR reads `rr`, with legacy fallback to `rl_cost`.

In `scripts/sim/game_state.gd`, applying card unit data initializes `rl_total` from card stats and resets `rl_used` to zero. The default unit dictionary also carries `rl_used: 0`.

In `scripts/sim/rules.gd`, `available_rl(state, unit_id)` reads effective `rl_total` and `rl_used`; `default_caster_unit_id_for_rr` requires active units with available RL at least the requested RR.

## Godot RR Payment And Activation Timing

Godot checks RR affordability before hand effects, traps, reactions, and equip options become legal. For hand `cast`/`equip` actions, `rules.gd` stages `pending_effect_activation` with `rr` and `rl_cost`, then opens the response window. That means the action is declared and can be responded to before final effect resolution.

The explicit `rl_used` mutation found in the inspected canonical path is the equip reservation path:

- `GameState.equip_wand_to_unit(...)` checks `Rules.available_rl(...) < rr` unless told to ignore availability.
- on successful equip, it appends the wand and increments `rl_used` by RR.
- `remove_equipped_wand_at(...)` reverses applied wand effects and decrements `rl_used` by the removed wand RR.

Normal effect activation resolution in `autoload/Game.gd` carries `rr`/`rl_cost` metadata through the pending activation payload, but the inspected `_resolve_effect_activation_impl` path delegates payload mutation to `EffectRunner.resolve_card_effects(...)` and does not directly reserve `rl_used` for non-equip effects. Negated pending effects skip resolution and do not run the equip reservation path. Failed equips are discarded without reserving RL.

## Overflow And Wand Destruction

Godot has separate RL overflow presentation/resolution code. `_check_rl_overflow(unit_id)` compares `rl_used` to effective RL total and opens/queues overflow handling when used exceeds total. Destroying an equipped wand calls `remove_equipped_wand_at(...)`, which also refunds the wand RR. This is separate from the basic act of reserving RL for an equip.

Unreal does not implement overflow, wand destruction, equipped-wand fallout, or CardDB-driven wand reservation in this pass.

## Current Unreal Baseline

Before this pass:

- `WBCardActivationAffordability::QueryFromUnitRL` reads `RLTotal`, `RLUsed`, and available RL without mutating state.
- `FWBCardActivationCostGateDefinition` and `FWBCardActivationCostGateContext` allow candidate generation to filter unaffordable activation effects using prepared affordability metadata.
- `FWBCardActivationCommand` carries source, effect request, optional usage commit, and debug id.
- `WBRules::CanApplyCardActivationCommand` validates source, usage commit, and effect request without mutation.
- `WBEffectRunner::ApplyCardActivationCommand` uses a working copy, applies the effect request, marks usage on success, and commits only after success.

## Chosen Unreal Fixture Behavior

This pass adds fixture-owned RR payment metadata to activation commands. When a command requests payment:

- rules validation re-checks affordability before execution.
- EffectRunner pays by increasing `RLUsed` on a working copy before applying the effect request.
- if payment fails, the effect request is not applied.
- if the effect request fails, payment rolls back with the working copy.
- if the effect request succeeds, payment commits with the rest of the activation.
- usage marking still happens only after successful effect resolution.

Payment is atomic with successful activation because the existing Unreal command executor already treats effect payload mutation and usage marking as one working-copy transaction. Adding payment inside that transaction prevents partial `RLUsed` mutation when activation validation or effect resolution fails.

## Deferred Work

- production card zones
- real CardDB costs/import
- production payment ownership
- RL overflow cleanup
- equipped wand fallout
- response windows
- effect negation
- passives/wands
- card-specific behavior
- UI target selection
