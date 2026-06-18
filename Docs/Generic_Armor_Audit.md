# Generic Armor Audit

## Scope

This audit inspected the Godot reference for generic armor behavior before adding Unreal C++ support. The pass is limited to generic `current_armor` / `max_armor` damage handling and does not implement card-specific prevention, replacement, armor-granting effects, death triggers, discard movement, or equipped-wand fallout.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/combat_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`

Search terms included `current_armor`, `max_armor`, `armor`, `bypass_armor`, `apply_damage`, `damage cause`, `burn`, `poison`, `attack damage`, `effect damage`, `shield`, `absorb`, and `HP damage`.

## Findings

Godot stores armor directly on unit dictionaries:

- `max_armor`
- `current_armor`

New units default both fields to zero. Applying card unit data reads `armor` / `max_armor` from stats/card data and calls `set_base_armor`, which sets both max and current armor to the same nonnegative value.

Armor is public. `autoload/Game.gd` includes `current_armor` and `max_armor` in the public unit snapshot path.

Godot clamps armor:

- `set_base_armor` clamps base armor to at least the provided minimum, usually zero.
- `set_current_armor` clamps current armor between zero and `Rules.get_effective_max_armor`.
- `Rules.get_effective_max_armor` clamps effective max armor to zero or higher.

Godot attack damage absorbs through armor. `combat_runner.gd:resolve_hit` calls `GameState.apply_damage` with cause `battle`, and `apply_damage` only bypasses armor for specific cause keys. Because `battle` is not a bypass cause, current armor absorbs battle damage before HP loss.

Godot Burn bypasses armor. `apply_damage` treats cause `burn` as armor-bypassing. Burn status ticks call `apply_damage(unit_id, 1, "burn")`, so Burn damages HP directly and leaves armor unchanged.

Godot Poison does not use armor. Poison status tick behavior reduces `max_hp` and clamps current HP through status tick logic; it does not call `apply_damage` and does not reduce armor.

Godot armor bypass causes in `apply_damage` are:

- `burn`
- `destroy`
- `hp_payment`
- `self_damage`

Generic effect damage does not bypass armor unless it uses one of those causes.

Godot HP damage is clamped after armor. `apply_damage` computes `hp_loss` after armor absorption, applies HP loss, and clamps HP to zero before card-specific prevention/replacement logic.

Godot trace/log behavior is uneven. Some effect runner paths record `armor_before` and `armor_after`, but generic `apply_damage` itself does not return a structured damage result that separates base damage, armor absorbed, final HP damage, and bypass state.

## Chosen Unreal Behavior

Unreal will add generic armor fields to `FWBUnitState`:

- `CurrentArmor`
- `MaxArmor`

`WBDamageResolution::ResolveDamageRequest` will treat `Prevention.FinalDamage` as pre-armor damage. Non-bypass damage consumes `CurrentArmor` first, then applies remaining HP damage. Bypass damage leaves armor unchanged and applies HP damage directly.

Attack damage will be non-bypass armor damage with stable cause `Attack`.

Burn status damage will route through the damage resolver as bypass damage with stable cause `Burn`.

Poison will remain max-HP status behavior and will not use armor or damage resolution.

Public board summaries and runtime result JSON will serialize armor because it is public combat state.

## Out of Scope

- card-specific prevention
- armor-granting cards
- Oathchain
- CSN Backfill
- CSN Juno
- Hybrid Hero replacement
- death triggers
- discard movement
- equipped-wand fallout
- responses
- counters
- passives
- wands
- UI, Blueprint, VFX, audio, or 3D runtime
