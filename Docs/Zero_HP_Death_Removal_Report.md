# Zero-HP Death Removal Report

## Scope

This pass adds deterministic zero-HP defeat/removal scaffolding after attack damage. It keeps the implementation in WandboundCore C++ and does not add prevention, replacement, triggers, discard movement, counters, responses, passives, cards, UI, VFX, Blueprints, or assets.

## State Model

- Added `FWBUnitState::bDefeated`.
- Added `FWBUnitState::bRemovedFromBoard`.
- Added helpers:
  - `IsUnitOnBoard`
  - `MarkUnitDefeated`
  - `RemoveUnitFromBoard`
- Added `FWBGameStateData::WinnerPlayerId`.
- Removed units keep their `UnitId`, `OwnerId`, and `CardId` in the unit record for replay/debug, but their board position is cleared to `X = -1`, `Y = -1`.

## Cleanup Behavior

- Added `WBRules::ShouldUnitBeDefeatedAtZeroHP`.
- Added `WBRules::CanApplyZeroHPDeathRemoval`.
- Added `WBEffectRunner::ApplyZeroHPDeathRemoval`.
- Cleanup finds active board units with `HP <= 0` and processes them in deterministic order:
  - `Y` ascending
  - `X` ascending
  - `UnitId` ascending
- Each non-Hero defeated unit emits:
  - `unit_defeated`
  - `unit_removed_from_board`
- If no units qualify, cleanup fails with `no_zero_hp_units` and does not mutate state.

## Lethal Attack Integration

- `ApplyPendingAttackDamage` now calls zero-HP cleanup after successful damage when defender HP is zero.
- Trace order for a lethal non-Hero attack is:
  - `attack_damage_resolved`
  - `unit_defeated`
  - `unit_removed_from_board`
- Pending attack still clears on successful damage resolution.
- If attacker or defender is already defeated/removed before damage resolution, validation fails and pending attack remains unchanged.
- Frozen break still deals 0 damage; healthy Frozen defenders are not removed.

## Board and Action Behavior

- Removed units do not occupy tiles.
- Removed units do not block movement or LOS.
- Removed units cannot move.
- Removed units cannot declare attacks.
- Removed units cannot be attack targets.
- Removed units are excluded from generated legal actions.
- Game-over states generate no legal actions for either player.

## Public Summary and Serialization

- Removed units are omitted from `WBPublicBoardSummary`.
- Runtime result public board JSON omits removed units.
- `FWBPublicTurnSummary` and runtime result JSON now include `winner_player_id`.
- Trace events identify defeated/removed unit ids without exposing hidden deck, hand, discard, or trigger data.

## Hero Loss

- Simple no-replacement Hero loss is implemented.
- If the removed unit matches that owner's `HeroUnitId`, Unreal sets:
  - `bGameOver = true`
  - `WinnerPlayerId = opposing player`
- Hero cleanup emits `hero_defeated` after `unit_removed_from_board`.
- Hybrid Hero replacement, death prevention, and replacement choices remain deferred.

## Fixtures and Tests

Added GodotCanon fixtures for:

- `zero_hp_nonhero_removed_from_board.json`
- `zero_hp_cleanup_no_units_fails.json`
- `attack_damage_lethal_removes_defender.json`
- `attack_damage_nonlethal_keeps_defender.json`
- `removed_unit_no_longer_blocks_movement.json`
- `removed_unit_not_attack_target.json`
- `removed_unit_not_in_public_board_summary.json`
- `zero_hp_hero_sets_game_over.json`
- `attack_damage_lethal_hero_sets_game_over.json`

Updated `attack_damage_hp_clamps_to_zero.json` to expect cleanup traces and removed board state.

Added `WBZeroHPDeathRemovalTests.cpp` with direct and fixture-driven coverage.

## Out of Scope

- prevention
- replacement
- Hybrid Hero replacement
- death triggers
- on-destroy buffs
- counters
- responses
- passives
- wands
- cards/effects
- discard movement
- UI/VFX/audio
- Blueprint/Actor/`.uasset` work
