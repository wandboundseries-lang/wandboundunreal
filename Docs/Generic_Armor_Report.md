# Generic Armor Report

## Scope

This pass implements deterministic generic armor from the Godot `current_armor` / `max_armor` model.

Implemented:

- `FWBUnitState::CurrentArmor`
- `FWBUnitState::MaxArmor`
- armor normalization helpers
- non-bypass damage absorption before HP loss
- bypass damage that leaves armor unchanged
- attack damage as non-bypass armor damage
- Burn as armor-bypass damage
- Poison unchanged as MaxHP status behavior
- armor trace fields
- public board summary armor fields
- runtime result JSON armor fields
- GodotCanon armor fixtures
- `Wandbound.Core.GenericArmor.*` automation tests

Not implemented:

- armor-granting cards
- card-specific prevention
- card-specific replacement
- Oathchain
- CSN Backfill
- CSN Juno
- Hybrid Hero replacement
- death triggers
- discard movement
- equipped wand fallout
- responses
- counters
- passives
- wands
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work

## Damage Behavior

`WBDamageResolution::ResolveDamageRequest` now treats `Prevention.FinalDamage` as damage after prevention and before armor.

For non-bypass damage:

- `ArmorAbsorbedAmount = min(CurrentArmor, FinalDamage)`
- `CurrentArmor -= ArmorAbsorbedAmount`
- `HPDamageAmount = FinalDamage - ArmorAbsorbedAmount`
- `HP = max(0, HP - HPDamageAmount)`

For bypass damage:

- armor is unchanged
- `ArmorAbsorbedAmount = 0`
- `HPDamageAmount = FinalDamage`
- `HP = max(0, HP - HPDamageAmount)`
- `bBypassedArmor = true`

Damage resolution still does not remove units. Zero-HP cleanup remains a separate deterministic EffectRunner step.

## Attack, Burn, and Poison

Attack damage uses:

- `DamageKind = Attack`
- `DamageCause = Attack`
- `bBypassArmor = false`

Burn status damage uses:

- `DamageKind = Burn`
- `DamageCause = Burn`
- `bBypassArmor = true`

Poison does not use damage resolution and does not affect armor. It still reduces MaxHP at start turn and clamps HP to MaxHP.

## Trace Fields

Damage traces now expose:

- `previous_armor`
- `new_armor`
- `armor_absorbed_amount`
- `bypassed_armor`
- `hp_damage_amount`
- `damage_cause`

`damage_amount` remains the requested/base damage. `final_damage_amount` remains the post-prevention, pre-armor amount. `hp_damage_amount` is the actual HP loss after armor.

## Public Serialization

Armor is public combat state. Public board summaries and runtime result JSON now serialize:

- `current_armor`
- `max_armor`

Removed units remain excluded from public board summaries. Hidden deck, hand, discard, pending-choice, and private marker data remain excluded.

## Invariants

- Units with no armor keep previous damage behavior.
- Armor cannot go below zero.
- Current armor clamps to max armor.
- HP cannot go below zero.
- Action IDs are unchanged.
- Legal action generation is unchanged by armor.

## Fixtures and Tests

Added GodotCanon fixtures:

- `armor_attack_absorbs_all_damage.json`
- `armor_attack_absorbs_partial_damage.json`
- `armor_attack_no_armor_same_as_before.json`
- `armor_burn_bypasses_armor.json`
- `armor_poison_does_not_use_armor.json`
- `armor_lethal_after_partial_absorb_removes_unit.json`
- `runtime_result_serialization_armor_fields.json`
- `damage_trace_armor_absorption_fields.json`

Added `WBGenericArmorTests.cpp`, covering direct damage behavior, attack traces, Burn bypass, Poison non-damage behavior, public/runtime serialization, action ID stability, legal generation stability, and fixture-driven canon coverage.
