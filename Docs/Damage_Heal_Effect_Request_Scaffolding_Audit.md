# Damage/Heal Effect Request Scaffolding Audit

Date: 2026-06-22

## Scope

This audit covers the Godot reference behavior for generic damage/heal-shaped effects and the current Unreal rules scaffolding that this pass builds on.

This pass does not import Godot CardDB data, add card activation timing, target selection UI, response windows, effect negation, passives, wands, or card-specific behavior.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`
- Existing GodotCanon golden scenarios under `Reference/GodotCanon/GoldenScenarios/`

## Godot Damage Shape

Godot `EffectRunner` recognizes card/effect operation names including `damage`, `heal`, `heal_unit`, and `heal_units_by_terrain`. The generic damage handlers inspected route damage through:

```gdscript
gs.apply_damage(target_id, amount, "effect", source)
```

The relevant `GameState.apply_damage` path:

- fails with `-1` for a missing unit id
- returns current HP for non-positive damage
- applies incoming-effect ignore checks for non-battle, non-burn, non-destroy, non-hp-payment, non-self-damage causes
- uses armor unless the cause bypasses armor
- clamps HP to zero
- runs Godot's card-specific replacement/prevention/removal logic when HP reaches zero

Generic `"effect"` damage is not one of the armor-bypass causes in the inspected Godot code.

## Godot Armor Bypass Causes

The inspected Godot bypass set is:

- `burn`
- `destroy`
- `hp_payment`
- `self_damage`

Generic `"effect"` damage normally uses armor. Unreal's generic `damage_effect` exposes an explicit fixture-owned `bBypassArmor` field so tests can model both normal effect damage and special future causes without importing CardDB or card-specific rules.

## Godot Healing Shape

Godot healing paths call:

```gdscript
gs.apply_heal(unit_id, amount, source)
```

The relevant `GameState.apply_heal` path:

- returns current HP for non-positive healing
- applies incoming-effect ignore checks
- reads current HP and effective max HP
- sets `hp = min(max_hp, hp + amount)`
- delegates to `set_hp(..., true)`, which also clamps to max HP and zero

Healing cannot exceed max HP in the inspected path.

## Defeated/Removed Healing

The inspected generic heal path operates on units found by id in `GameState.units`; Godot destruction may remove units from that array. There is no generic revive behavior in the inspected `heal`/`heal_unit` path. Any revive-like card behavior would need a card-specific implementation and is out of scope for this pass.

Unreal will therefore reject missing, defeated, or removed units for generic `heal_effect`. It will not revive removed units or clear defeat/removal flags.

## Zero-HP Cleanup

Godot `apply_damage` includes card-specific replacement/prevention and `remove_unit_by_id` when HP reaches zero. Unreal already has:

- `WBDamageResolution::ResolveDamageRequest`
- `WBEffectRunner::ApplyZeroHPDeathRemoval`
- default death prevention scaffolding through `WBDeathResolution`

For this pass, `WBDamageEffect` only resolves damage. `WBEffectRunner::ApplyDamageEffect` owns the zero-HP cleanup call after a successful damage effect, matching the existing Unreal attack-damage boundary.

## Current Unreal Support

Unreal already supports:

- deterministic `FWBGameStateData`
- `WBDamageResolution` for HP damage, armor absorption, prevention trace fields, and clamping HP at zero
- zero-HP defeat/removal scaffolding through `WBEffectRunner::ApplyZeroHPDeathRemoval`
- generic `FWBEffectRequest` dispatch for `armor_effect` and `status_effect`
- atomic multi-payload effect requests through a working-state copy
- parent `effect_request_resolved` traces followed by child payload traces
- public summaries and runtime serialization that exclude hidden hand/deck/discard data

## Chosen Unreal Behavior

### `damage_effect`

- Missing target fails without mutation.
- Defeated/removed target fails without mutation.
- Negative amount fails without mutation.
- Zero amount succeeds as a no-op.
- Damage uses `WBDamageResolution::ResolveDamageRequest`.
- `DamageKind` is `Effect`.
- `DamageCause` defaults to `Effect` when not supplied.
- Armor is used unless `bBypassArmor` is true.
- `WBDamageEffect` does not run zero-HP cleanup directly.
- `WBEffectRunner::ApplyDamageEffect` emits `damage_effect_resolved`, then runs zero-HP cleanup when HP is at or below zero.

### `heal_effect`

- Missing target fails without mutation.
- Defeated/removed target fails without mutation.
- Negative amount fails without mutation.
- Zero amount succeeds as a no-op.
- Healing clamps to `MaxHP`.
- `EffectiveHealAmount` is `NewHP - PreviousHP`.
- Max HP is unchanged.
- Statuses are unchanged.
- Removed/defeated units are not revived.
- No responses, passives, wands, or card-specific triggers run.

## Out of Scope

- Godot CardDB importer
- JSON card database loading
- card activation timing
- target-selection UI
- player legal action generation for effects
- response windows
- effect negation
- passives
- wands
- card-specific damage/heal/armor/status effects
- card-specific prevention/replacement, including Oathchain, CSN Backfill, CSN Juno, and Hybrid Hero replacement
- death triggers
- discard movement
- equipped wand fallout
- counters
- random dice
- draw
- NPC phase
- UI, Blueprint gameplay, 3D runtime, VFX, and sound
