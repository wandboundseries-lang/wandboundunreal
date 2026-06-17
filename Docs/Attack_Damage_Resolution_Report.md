# Attack Damage Resolution Report

## Scope

This pass adds deterministic attack damage resolution after attack declaration. It introduces rules-state-only pending attack data, resolves HP damage from pending attacks, handles Frozen break-on-hit, and adds fixture-driven coverage.

## Pending Attack State

- Added `FWBPendingAttackState` to `FWBGameStateData`.
- A successful `ApplyAttackDeclare` now:
  - decrements `AttacksLeft`
  - emits `attack_declared`
  - stores attacker, defender, attacking player, tiles, and declaration action id in `PendingAttack`
- A second declaration while one attack is pending fails with `pending_attack_already_active`.
- Pending attack is not a legal action and is not exposed in public board summaries.

## Damage Formula

- `WBEffectRunner::ApplyPendingAttackDamage` validates with `WBRules::CanResolvePendingAttackDamage`.
- Baseline damage is `max(0, attacker.ATK)`.
- Defender HP becomes `max(0, previous HP - damage amount)`.
- Pending attack clears only after successful damage resolution.
- Failed damage resolution does not mutate HP/statuses and does not clear pending attack.

Godot stores base attack damage at declaration, then modifies it during hit resolution. This Unreal pass computes from current ATK at resolution because the requested minimal pending attack state does not store damage. Response windows and modifiers remain future work.

## HP Clamp and Death

- HP clamps to zero.
- Units are not removed at zero HP in this pass.
- Hero loss, death replacement, prevention, discard, and wand fallout are deferred.

## Frozen Policy

- Godot audit confirmed direct targeted hits break Frozen and deal no damage unless a piercing rule applies.
- Frozen defenders now:
  - lose Frozen during `ApplyPendingAttackDamage`
  - take zero damage
  - emit `status_removed` for `Frozen`
  - emit `attack_damage_resolved` with `damage_amount = 0`
- Frozen attackers still cannot declare attacks.
- Friendly Frozen targets are legal only as a narrow break-Frozen exception.
- Friendly non-Frozen targets remain illegal.

## Trace Fields

- Added `damage_amount` to replay trace JSON.
- `attack_damage_resolved` includes:
  - player id
  - source unit id
  - target unit id
  - from/to tiles
  - declaration action id
  - damage amount
  - previous HP
  - new HP
  - `at_or_below_zero_hp` when HP is zero

## Fixtures and Tests

Added GodotCanon fixtures for:

- basic ATK damage
- zero ATK no-op damage
- HP clamp to zero
- missing pending attack failure
- missing attacker failure
- missing defender failure
- declaration stages pending attack
- declaration fails while pending exists
- Frozen target break
- friendly Frozen attack-break declaration

Added `WBAttackDamageResolutionTests.cpp` with direct and fixture-driven coverage.

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
