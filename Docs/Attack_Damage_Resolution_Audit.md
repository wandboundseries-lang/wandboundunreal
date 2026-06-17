# Attack Damage Resolution Audit

## Godot Files Inspected

- `Reference/GodotProject/godotcanon/scripts/sim/combat_runner.gd`
  - `resolve_hit`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
  - `apply_damage`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
  - `can_unit_attack`
  - `get_attack_target_unit_ids`
  - `query_attack`
  - attack pre-hit simulation path
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
  - `request_attack_impl`
  - `_advance_pending_attack`
  - `_resolve_hit`
- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`
- `Reference/GodotCanon/GodotSourceIndex.md`

## Godot Damage Flow

- Godot declaration validates with `Rules.query_attack`, spends one `attacks_left`, and creates `pending_attack`.
- Godot stores base attack damage on the pending attack at declaration with `Rules.get_effective_atk(state, attacker_id)`.
- At hit resolution, Godot reads `pending_attack["damage"]`, adds later context bonuses/modifiers, then calls `CombatRunner.resolve_hit`.
- `CombatRunner.resolve_hit` handles Frozen before generic hit hooks and before `GameState.apply_damage`.
- For non-Frozen defenders, Godot calls `GameState.apply_damage(target_id, dmg, "battle", source)`.

## HP, Death, and Removal

- `GameState.apply_damage` returns immediately for non-positive damage without changing HP.
- Battle damage normally interacts with armor first in Godot; armor/prevention is out of scope for this Unreal pass.
- Godot clamps HP to zero before replacement/prevention/death handling.
- Godot can remove destroyed units through later destruction/prevention logic in `apply_damage`.
- This Unreal pass intentionally clamps HP at zero but does not remove dead units or resolve hero loss.

## Frozen

- The Rules Bible states that a direct targeted hit against a Frozen unit breaks Frozen and deals no damage unless a piercing rule says otherwise.
- The glossary repeats that direct targeted attacks normally break Frozen instead of dealing damage.
- Godot `CombatRunner.resolve_hit` checks `gs.has_status(target_id, "frozen")` before applying damage.
- If the attacker does not pierce Frozen, Godot removes the target's Frozen status, emits a frozen-break event, reports `frozen_blocked = true`, and returns without calling `apply_damage`.
- Godot `_resolve_hit` records this as an attack blocked by Frozen with damage and amount zero.
- Attacking while Frozen is illegal because `Rules.can_unit_attack` denies Frozen attackers.

## Friendly Frozen Targets

- Godot `get_attack_target_unit_ids` explicitly allows friendly targets only when the friendly unit is Frozen.
- Godot `query_attack` denies same-owner targets with `friendly_target_not_frozen` unless the target has Frozen.
- This pass ports that narrow exception: friendly non-Frozen targets stay illegal; friendly Frozen targets may be declared so damage resolution can break Frozen.

## LOS Timing

- Godot attack declaration validates LOS and target legality through `query_attack`.
- The damage-resolution path uses the stored pending attack and does not rerun full declaration LOS/range by default, because response/modifier windows may alter state between declaration and hit.
- This Unreal pass validates pending attack integrity and unit existence at damage resolution, but does not rerun LOS/range.

## Chosen Unreal Behavior This Pass

- Add rules-state-only pending attack data to `FWBGameStateData`.
- `ApplyAttackDeclare` spends `AttacksLeft`, emits `attack_declared`, and stages pending attack state.
- A second declaration while pending attack is active fails with `pending_attack_already_active`.
- `ApplyPendingAttackDamage` validates only pending attack integrity, attacker/defender existence, distinct ids, and valid attacking player id.
- Basic damage amount is `max(0, attacker.ATK)` at resolution time for now. Godot stores base damage at declaration, but the requested minimal Unreal pending state does not store damage; attack modifiers and response timing are future work.
- Non-Frozen damage sets defender HP to `max(0, previous HP - damage amount)`, clears pending attack, and emits `attack_damage_resolved`.
- Frozen defenders lose Frozen, take zero damage, clear pending attack, and emit `status_removed` followed by `attack_damage_resolved` with damage zero.
- Public board summaries reflect HP/status changes after damage; pending attack state is not serialized as public board data.

## Out of Scope

- pre-hit responses
- post-hit responses
- counterattacks
- passives
- wands
- cards/effects
- armor/prevention
- full death/removal
- terrain attack modifiers
- diagonal/ignore-wall/ignore-unit attack exceptions
- UI, VFX, audio, Blueprint, Actor, or `.uasset` work
