# Armor Effect Scaffolding Report

## Scope

This pass adds deterministic generic armor effect scaffolding. It exposes C++ operations that mutate armor values only; it does not implement card-specific armor cards, card database import, targeting UI, timing windows, passives, wands, responses, or runtime presentation.

## Godot Audit Summary

Godot armor effect behavior is built on:

- `restore_armor`
- `modify_stat` with `stat = "armor"`, `"current_armor"`, or `"max_armor"`
- `stat_adjustment` / `stat_adjust` for temporary stat effects

`restore_armor` is registered through the same modify-stat handler and maps to `current_armor`. The alias `armor` also maps to `current_armor`.

Godot clamps current armor through `set_current_armor`, using `Rules.get_effective_max_armor` as the cap. Reducing max armor clamps current armor down to the new max. Restoring armor cannot exceed max armor. Increasing max armor does not fill current armor unless the effect explicitly uses `restore_current_by_delta` / `also_restore_armor`.

## WBArmorEffect

Added pure C++ armor effect scaffolding:

- `EWBArmorEffectOp`
- `FWBArmorEffectRequest`
- `FWBArmorEffectResult`
- `WBArmorEffect::ApplyArmorEffect`

Supported operations:

- `AddCurrentArmor`
- `ReduceCurrentArmor`
- `SetCurrentArmor`
- `AddMaxArmor`
- `ReduceMaxArmor`
- `SetMaxArmor`
- `RestoreArmorToMax`

Failure cases do not mutate state:

- unknown operation
- missing target
- removed/defeated target
- negative amount

## Clamp Behavior

- Current armor is always clamped to `0..MaxArmor`.
- Max armor is always clamped to `>= 0`.
- Current armor is zero when max armor is zero.
- Reducing or setting max armor clamps current armor.
- Adding max armor does not auto-fill current armor.
- `RestoreArmorToMax` sets current armor to max armor and ignores amount.

## EffectRunner Wrapper

Added:

- `WBEffectRunner::ApplyArmorEffect`

The wrapper calls `WBArmorEffect`, returns failures directly, and emits `armor_modified` on success.

## Trace Behavior

`armor_modified` traces include:

- target unit id
- target owner as player id
- previous/new current armor
- previous/new max armor
- armor effect operation
- armor effect amount

Trace JSON fields:

- `armor_effect_operation`
- `armor_effect_amount`
- `previous_armor`
- `new_armor`
- `previous_max_armor`
- `new_max_armor`

Existing damage armor trace fields remain unchanged.

## Public Summary Behavior

Public board summaries and runtime result JSON already include:

- `current_armor`
- `max_armor`

After an armor effect, public/runtime summaries reflect the updated armor values. Hidden deck, hand, discard, marker, and pending-choice data remain excluded.

## Fixtures and Tests

Added GodotCanon fixtures:

- `armor_effect_add_current_clamps_to_max.json`
- `armor_effect_reduce_current_clamps_to_zero.json`
- `armor_effect_set_current_clamps_to_max.json`
- `armor_effect_add_max_does_not_auto_fill_current.json`
- `armor_effect_reduce_max_clamps_current.json`
- `armor_effect_set_max_clamps_current.json`
- `armor_effect_restore_to_max.json`
- `armor_effect_missing_target_fails_no_mutation.json`
- `armor_effect_removed_target_fails_no_mutation.json`
- `runtime_result_serialization_after_armor_effect.json`

Added `WBArmorEffectScaffoldingTests.cpp`, covering direct operations, failure cases, traces, public serialization, fixture scenarios, legal action stability, and action ID stability.

## Out of Scope

- card-specific armor cards
- Officer Repairer
- Armor Plate
- card database import
- effect activation timing
- target selection UI
- responses
- counters
- passives
- wands
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work
