# CSN Body Double Damage-Substitution Transfer Audit

## Authority

This pass uses the user-directed Body Double text as the card's current authority.
The read-only Godot implementation remains historical evidence only.

OLD: `redirect_attack_to_adjacent_friendly` changed the pending attack target and
required the replacement to satisfy attacker range, alignment, walls, and line of
sight.

NEW: choose any other controlled, live, on-board CSN unit. The attack calculates
against its final current defender after PreHit Redirects. If that hit unit is the
protected Hero and the calculation produces HP damage, the selected CSN loses that
already-calculated HP amount directly. PostHit remains attached to the hit unit.

The global Redirect definition and `RedirectPendingAttack` implementation are not
repurposed or removed.

## Production Representation

The isolated production-schema fixture defines `effect_react_csn_body_double` as a
Hand React with RR 2, response-window timing, an own-current-Hero-defender source
condition, a controlled CSN `other_than_own_hero` target condition, and the generic
`RegisterPendingAttackHPDamageSubstitution` payload.

No generic authority branches on the Body Double card id. Faction membership is
read from `FWBCardDefinition::PublicFactions`.

## Continuation Semantics

`FWBPendingAttackState::DamageSubstitution` stores protected and substitute unit
ids. `DamageCalculation` separately stores hit identity, raw attack damage, Armor
outcome, calculated HP damage, Frozen break, and prevention. Registration does not
change defender, original defender, declaration, continuation, or reaction target.

Generic application requires a live pending PreHit attack, exact continuation id,
and a live on-board recipient. The latest successfully resolved substitution wins.
CalculateDamage is pure for HP, Armor, Frozen, and removal. ApplyDamage consumes the
hit unit's Armor or removes its Frozen status. A transferred HP amount bypasses the
substitute's Armor and does not remove substitute Frozen.

If the selected unit disappears before the effect resolves, immutable-target
validation fails closed. If it disappears after successful substitution but before
Damage, resolution emits `pending_attack_damage_substitution_fallback` and applies the
ordinary attack to the current defender. Defender removal continues to use existing
attack cancellation rules.

## Interactions

- Negate: a negated Body Double frame does not resolve, does not pay RR, and does
  not set the recipient. A negated Negate permits normal Body Double resolution.
- Prevent: successful prevention still prevents the attack after substitution.
- Multiple substitutions: latest successful resolution replaces the earlier one.
- Move/status/Armor changes: target eligibility is checked when selected. No
  adjacency, range, line-of-sight, alignment, or wall relationship is required.
- NPC and counter attacks: the generic operation is authority-neutral. Typed Body
  Double legality depends only on a current own Hero defender during PreHit.

## Redirect Cross-Interaction

True Redirect changes the defender before calculation. Redirect away from the Hero
makes Body Double inert; the redirected unit is hit and damaged normally. If later
Redirects leave the Hero as final defender, Body Double may substitute. Intermediate
targets do not matter and generic Redirect legality remains unchanged.

## Privacy and Replay

Active substitution and calculation state are conditionally included in the
private canonical state digest. Replay schema remains 1 and `WBActionCodec` is unchanged. Public receipts
remain eight fields and exclude card instance alternatives, continuations, state
digests, and trace digests. Core traces use generic substitution terminology.

## Protected Baselines

Body Double is isolated under `Data/Replay/CSNBodyDoubleFixture/`; the canonical
startup bundle, Active Format, decks, and existing Redirect fixture are unchanged.
