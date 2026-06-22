# Damage/Heal Effect Request Scaffolding Report

## Scope

This pass adds deterministic generic `damage_effect` and `heal_effect` scaffolding for Unreal-owned fixture data.

Implemented:

- `FWBDamageEffectRequest`
- `FWBDamageEffectResult`
- `WBDamageEffect::ApplyDamageEffect`
- `FWBHealEffectRequest`
- `FWBHealEffectResult`
- `WBHealEffect::ApplyHealEffect`
- `WBEffectRunner::ApplyDamageEffect`
- `WBEffectRunner::ApplyHealEffect`
- `damage_effect` and `heal_effect` payloads in `FWBEffectRequest`
- standalone fixture operations for `apply_damage_effect` and `apply_heal_effect`
- fixture parsing for damage/heal effect request payloads

Not implemented:

- Godot CardDB import
- JSON card database loading
- real card activation timing
- target-selection UI
- player legal action generation for effects
- response windows
- effect negation
- passives
- wands
- card-specific effects
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work

## Damage Effect Behavior

`WBDamageEffect::ApplyDamageEffect` validates and resolves generic effect damage through `WBDamageResolution`.

Rules:

- missing target fails without mutation
- removed/defeated target fails without mutation
- negative amount fails without mutation
- zero amount succeeds as a no-op
- `DamageKind` is `Effect`
- `DamageCause` defaults to `Effect`
- `bBypassArmor` is passed through to the damage resolver
- zero-HP cleanup is not run inside `WBDamageEffect`

`WBEffectRunner::ApplyDamageEffect` wraps the helper, emits `damage_effect_resolved`, and then runs existing zero-HP cleanup when HP is at or below zero.

## Heal Effect Behavior

`WBHealEffect::ApplyHealEffect` restores HP directly on board units.

Rules:

- missing target fails without mutation
- removed/defeated target fails without mutation
- negative amount fails without mutation
- zero amount succeeds as a no-op
- healing clamps to `MaxHP`
- `EffectiveHealAmount = NewHP - PreviousHP`
- `MaxHP` is unchanged
- statuses are unchanged
- removed/defeated units are not revived
- responses, passives, wands, and card-specific hooks are not run

## Armor and Bypass Behavior

Generic effect damage uses armor by default, matching the inspected Godot `"effect"` damage path.

When `bBypassArmor` is true, armor remains unchanged and the full final damage amount is applied to HP. This is fixture-owned scaffolding for future special causes and does not import CardDB behavior.

## EffectRequest Integration

`EWBGenericEffectOp` now supports:

- `armor_effect`
- `status_effect`
- `damage_effect`
- `heal_effect`

`WBRules::CanApplyEffectRequest` validates damage/heal payload targets and non-negative amounts. `WBEffectRunner::ApplyEffectRequest` fills missing payload source/target ids from the request shell and dispatches payloads in order.

Multi-payload effect requests remain atomic through the existing working-state copy. If a later payload fails, earlier mutations are discarded and no success traces are returned.

## Trace Behavior

New child traces:

- `damage_effect_resolved`
- `heal_effect_resolved`

`damage_effect_resolved` includes public deterministic damage fields: source unit id, target unit id, player id, damage amount, HP before/after, armor before/after, absorbed armor, HP damage, bypass flag, damage cause, and zero-HP flag.

`heal_effect_resolved` includes source unit id, target unit id, player id, HP before/after, requested heal amount, and effective heal amount.

Replay trace JSON now serializes:

- `heal_amount`
- `effective_heal_amount`

Source card id and source effect id remain fixture/debug metadata only and are not emitted in traces or public summaries.

## Hidden Information

Damage/heal effect request traces and runtime utility serialization exclude:

- deck contents
- hand contents
- discard contents
- pending attack internals
- source card id
- source effect id

Visible board unit card ids remain public under the existing public board summary policy.

## Fixtures

Added standalone fixtures:

- `damage_effect_basic_reduces_hp.json`
- `damage_effect_armor_absorbs_damage.json`
- `damage_effect_bypass_armor.json`
- `damage_effect_lethal_runs_zero_hp_cleanup.json`
- `damage_effect_missing_target_fails_no_mutation.json`
- `heal_effect_basic_restores_hp.json`
- `heal_effect_clamps_to_max_hp.json`
- `heal_effect_removed_target_fails_no_mutation.json`
- `heal_effect_zero_amount_noop.json`

Added effect request fixtures:

- `effect_request_damage_effect_basic.json`
- `effect_request_heal_effect_basic.json`
- `effect_request_damage_then_heal_atomic_success.json`
- `effect_request_damage_then_heal_atomic_failure.json`
- `effect_request_mixed_armor_status_damage_heal_atomic_success.json`
- `runtime_result_serialization_after_damage_heal_effect_request.json`

## Remaining Work

- CardDB importer
- real card activation commands
- target selection
- card ownership/timing validation
- response windows
- effect negation
- passives
- wands
- card-specific damage/heal effects
- card-specific prevention/replacement
- death triggers
- discard movement
- equipped wand fallout

## Validation

Build:

```text
Result: Succeeded
Total execution time: 72.58 seconds
```

Wandbound automation:

```text
succeeded=508
succeededWithWarnings=0
failed=0
notRun=0
```

`git diff --check` reported no whitespace errors, only LF-to-CRLF working-copy notices.
