# Damage Prevention Scaffolding Report

## Scope

This pass adds deterministic C++ scaffolding for future damage prevention, armor, death prevention, replacement, Hero replacement, and death triggers. It does not activate any card-specific behavior and keeps current attack damage and zero-HP cleanup behavior unchanged.

## Damage Resolution

Added `WBDamageResolution` with:

- `EWBDamageKind`
- `FWBDamageRequest`
- `FWBDamagePreventionResult`
- `FWBDamageResolutionResult`
- `EvaluateDamagePrevention`
- `ResolveDamageRequest`

Default prevention behavior is:

- `bPrevented = false`
- `PreventedAmount = 0`
- `FinalDamage = max(0, BaseDamage)`
- `PreventionReason = None`

`ResolveDamageRequest` validates the target, applies `FinalDamage`, clamps HP at zero, reports previous/new HP, and does not remove the unit.

## Death Resolution

Added `WBDeathResolution` with:

- `FWBDeathPreventionResult`
- `FWBDeathResolutionCandidate`
- `EvaluateDeathPrevention`

Default death prevention behavior is:

- `bPrevented = false`
- `PreventionReason = None`

`WBEffectRunner::ApplyZeroHPDeathRemoval` now builds a death candidate and checks this seam before marking a unit defeated/removed. With the default result, cleanup behavior is unchanged.

## Attack Damage Pipeline

`WBEffectRunner::ApplyPendingAttackDamage` now builds a `FWBDamageRequest` for non-Frozen attacks:

- `DamageKind = Attack`
- source unit = pending attacker
- target unit = pending defender
- source player = pending attacking player
- base damage = non-negative attacker `ATK`

The resolver applies HP loss, then existing pending-attack clearing and zero-HP cleanup continue as before.

## Frozen Behavior

Frozen break behavior is unchanged:

- Frozen is removed before damage.
- HP damage remains zero.
- Pending attack clears.
- Trace order remains `status_removed`, then `attack_damage_resolved`.
- Frozen break is not treated as damage prevention.

## Trace Fields

Added optional trace fields:

- `damage_prevented`
- `prevented_damage_amount`
- `final_damage_amount`
- `prevention_reason`

Normal attack damage emits:

- `damage_amount = attacker ATK`
- `prevented_damage_amount = 0`
- `final_damage_amount = damage_amount`
- no prevention reason
- no `damage_prevented` JSON field because the value is false

Frozen break emits final damage zero and no prevention.

## Fixtures and Tests

Added GodotCanon fixtures:

- `damage_resolution_default_no_prevention.json`
- `attack_damage_uses_damage_resolution_pipeline.json`
- `zero_hp_cleanup_uses_death_resolution_pipeline.json`
- `damage_prevention_trace_defaults_no_prevention.json`

Added `WBDamagePreventionScaffoldingTests.cpp` covering:

- default damage prevention result
- default death prevention result
- direct damage resolution HP reduction
- direct damage resolution clamp without removal
- attack damage resolver trace defaults
- Frozen break preservation
- zero-HP cleanup default death resolution
- Hero death default no-replacement loss
- fixture-driven coverage for the four new scenarios

## Out of Scope

- card-specific prevention
- armor gameplay
- Oathchain
- Backfill
- Juno
- Hero replacement
- death triggers
- discard movement
- equipped wand fallout
- responses
- counters
- passives
- wands
- trap prevention
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work
