# Damage Prevention Scaffolding Audit

## Scope

This audit inspected the Godot reference before adding Unreal scaffolding for damage prevention, armor, death prevention, replacement, Hero replacement, and death triggers. The implementation pass intentionally adds seams only and does not activate card-specific prevention or replacement behavior.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/combat_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/handlers/damage_handler.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

Search terms included `prevent`, `prevention`, `replacement`, `Oathchain`, `Backfill`, `Juno`, `shield`, `armor`, `reduce damage`, `ignore damage`, `redirect`, `save from death`, `apply_damage`, `recent_deaths`, `death trigger`, `equipped wand`, and `destroy_unit`.

## Findings

Godot has generic armor state. `game_state.gd` stores `max_armor` and `current_armor`, exposes helpers such as `set_current_armor`, `restore_armor`, `bump_max_armor`, `increase_max_armor`, and `decrease_max_armor`, and `rules.gd` exposes `get_effective_max_armor`. `apply_damage` uses `current_armor` to absorb damage unless the cause bypasses armor.

Godot's armor bypass list currently includes `burn`, `destroy`, `hp_payment`, and `self_damage`. Battle damage does not bypass armor in the Godot reference.

Godot does not have a clean standalone attack-damage prevention class. Attack damage flows through `combat_runner.gd:resolve_hit`, then `game_state.gd:apply_damage`. Response and directive systems can prevent or redirect pending attacks before damage through kinds such as `prevent_attack`, `negate_attack`, `redirect_pending_attack`, and pending attack damage coin redirect paths.

Death prevention and replacement are interleaved with `apply_damage` and controller/card-specific logic. `apply_damage` clamps HP to zero before checking several replacement/prevention cases, then either restores HP or removes the unit.

Known prevention/replacement systems found:

- `CSN Oathchain`: equipped wand can prevent destruction by destroying the wand and setting HP to 1.
- `CSN Backfill`: CSN unit destruction prevention, once per unit while the player-level effect is active.
- `CSN Juno`: first destruction prevention per CSN unit when Juno is present.
- `Sever Thread` / `Shatter Parry`-style response effects: pending attack prevention through response directives.
- `Traprunner Familiar`: trap marker damage/effect prevention.
- Hybrid Hero replacement: sacrificing a Hero can replace that Hero with the Hybrid.
- Marrow death triggers and Next-of-Kin-style replacement/summon flows.

Godot death triggers are not part of simple cleanup. `game_state.gd` records `recent_deaths`, `rules.gd` has simulation death-trigger processing, and `autoload/Game.gd` drains and processes recent deaths through the game/death-trigger controller layer.

Hero replacement is interleaved with non-cleanup systems. The Rules Bible says Hero loss without replacement loses the game, while Hybrid summon notes and smoke tests cover Hero replacement as a separate effect path.

## Unreal Scaffold Choice

This pass adds two deterministic C++ seams:

- `WBDamageResolution`, which evaluates default no-prevention damage and applies HP loss without removing units.
- `WBDeathResolution`, which evaluates default no-prevention death cleanup candidates.

Attack damage now routes through `WBDamageResolution::ResolveDamageRequest`. Zero-HP cleanup asks `WBDeathResolution::EvaluateDeathPrevention` before marking a unit defeated/removed.

Although Godot has generic armor, Unreal does not add live armor fields to `FWBUnitState` in this pass. Adding armor would change current damage behavior and needs dedicated armor fixtures. Armor remains documented future work.

## Intentionally Unimplemented

- card-specific prevention
- card-specific replacement
- armor gameplay
- Oathchain
- Backfill
- Juno
- Hybrid Hero replacement
- death triggers
- on-destroy buffs
- discard movement
- equipped-wand fallout
- response windows
- counters
- passives
- wands
- trap prevention
- random dice
- draw
- UI, Blueprint, VFX, audio, or 3D runtime
