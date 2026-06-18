# Attack Declaration Audit

## Godot Files Inspected

- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
  - `can_unit_attack`
  - `get_attack_target_unit_ids`
  - `query_attack`
  - `query_los`
  - `_segment_blocked_for_attack`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
  - `request_attack_impl`
- `Reference/GodotProject/godotcanon/scripts/sim/combat_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotCanon/GodotSourceIndex.md`

## Canon References Inspected

- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`

## Findings

- Attack range uses AR as distance only. The Rules Bible says AR does not grant line of sight by itself.
- Default line of sight is orthogonal. Diagonal attacks require an explicit card/passive and are out of scope for this pass.
- Walls block LOS. Godot checks every crossed orthogonal edge with attack wall logic.
- Units block LOS unless a card/passive says attacks ignore units. This pass includes baseline intervening-unit blocking because both Godot and the canon are clear.
- `can_unit_attack` denies attack declaration while stunned, frozen, or `no_attack`.
- The canon adds Cannot Attack as a status-like restriction that prevents declaring attacks.
- `query_attack` requires `attacks_left > 0`, a valid target, target in line, target in range, and LOS.
- Friendly targets are normally illegal in Godot unless the friendly target is frozen. The later attack damage pass ports that narrow break-Frozen exception.
- Neutral NPC targets can be treated as enemy targets in this baseline when the attacker is the active player.
- Godot `request_attack_impl` decrements `attacks_left` immediately after declaration legality succeeds and before pending pre-hit response/damage resolution.

## Chosen Unreal Baseline

- Add attack declaration as a deterministic C++ rules/effects path only.
- Legal attacks require active normal-turn priority, attacker ownership, enemy/neutral target or friendly Frozen target, `AttacksLeft > 0`, attack-capable status, orthogonal alignment, AR range, and unblocked LOS.
- LOS walks each adjacent edge from attacker to target and denies `blocked_by_wall` or `blocked_by_unit`.
- A legal declaration decrements `AttacksLeft` by one and emits one `attack_declared` trace.
- Damage, Frozen breaking, and pending attack staging were added in the later attack damage pass. Response windows, counters, passives, cards, terrain modifiers, wands, UI, VFX, and 3D runtime remain out of scope.
