# Zero-HP Death Removal Audit

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
  - `apply_damage`
  - `remove_unit_by_id`
  - `destroy_unit`
  - `drain_recent_deaths`
- `Reference/GodotProject/godotcanon/scripts/sim/combat_runner.gd`
  - `resolve_hit`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
  - `_process_recent_deaths_sim`
  - `_apply_ok_with_death_triggers`
  - Hybrid summon/sacrifice hero replacement paths
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
  - `unit_destroyed` directive emission
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
  - `_process_recent_deaths`
  - `_check_game_over_if_chain_finished`
  - match log `unit_destroyed` / `hero_replaced` reporting
- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`

## Godot Non-Hero Removal

- `GameState.apply_damage` reduces HP and records HP 0 before prevention/replacement checks.
- If no prevention/replacement applies, `apply_damage` calls `remove_unit_by_id(unit_id, cause)`.
- `remove_unit_by_id`:
  - clears statuses tied to the unit
  - appends a snapshot to `recent_deaths`
  - discards equipped wands
  - clears the unit from `board_units`
  - removes the unit dictionary from the `units` array
  - resyncs dynamic unit state
- Death trigger processing is separate: `drain_recent_deaths` feeds rules/game death-trigger controllers after removal.

## Godot Discard and Side Effects

- Godot immediately discards equipped wands during `remove_unit_by_id`.
- Godot does not show a simple always-discard-visible-unit-card rule in the inspected core death path.
- Death triggers, on-destroy buffs, hand choices, and special cards are interleaved after `recent_deaths` is drained.
- This Unreal pass does not model discard movement, equipped wands, graveyard/discard ownership side effects, or death triggers.

## Godot Hero Loss

- Godot tracks per-player `hero_unit_id`.
- `Game._check_game_over_if_chain_finished` waits until there is no response and no pending attack, then checks whether each hero id still resolves to a unit.
- If a hero unit is missing, Godot sets `game_over = true` and `game_over_loser = pid`.
- Hybrid hero replacement and death-prevention systems can change the hero id or prevent removal before simple loss is observed.

## Chosen Unreal Behavior This Pass

- Preserve unit records in `FWBGameStateData::Units` for deterministic replay/debug.
- Add `bDefeated` and `bRemovedFromBoard` to `FWBUnitState`.
- `bDefeated` means HP reached zero and the unit is defeated.
- `bRemovedFromBoard` means the unit no longer occupies a tile, blocks movement/LOS, acts, targets, or appears in public board summaries.
- Removing from board sets `X = -1` and `Y = -1`.
- `ApplyZeroHPDeathRemoval` processes all active board units with `HP <= 0`, sorted by `Y`, then `X`, then `UnitId`.
- Non-Hero cleanup emits `unit_defeated` then `unit_removed_from_board`.
- Hero cleanup uses the simple no-replacement baseline:
  - sets `bGameOver = true`
  - sets `WinnerPlayerId` to the opposing player for owners 0/1
  - emits `hero_defeated` after removal
- Lethal `ApplyPendingAttackDamage` appends death/removal traces after `attack_damage_resolved`.

## Deferred

- prevention effects
- replacement effects
- Hybrid Hero replacement
- death triggers
- on-destroy buffs
- equipped wand fallout
- discard movement for visible unit cards
- counters and responses
- passives
- cards/effects
- UI, VFX, sound, Blueprint, Actor, or `.uasset` work
