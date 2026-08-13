# CSN Body Double Damage-Substitution Transfer Audit

## Authority

This pass uses the user-directed Body Double text as the card's current authority.
The read-only Godot implementation remains historical evidence only.

OLD: `redirect_attack_to_adjacent_friendly` changed the pending attack target and
required the replacement to satisfy attacker range, alignment, walls, and line of
sight.

NEW: the attack remains targeted at the current Hero defender. A selected,
controlled, orthogonally adjacent CSN unit receives only the attack's Damage-stage
result. PostHit and counter context remain attached to the Hero defender.

The global Redirect definition and `RedirectPendingAttack` implementation are not
repurposed or removed.

## Production Representation

The isolated production-schema fixture defines `effect_react_csn_body_double` as a
Hand React with RR 2, response-window timing, an own-current-Hero-defender source
condition, a controlled CSN orthogonal-Hero-adjacency target condition, and the
generic `SubstitutePendingAttackDamageRecipient` payload.

No generic authority branches on the Body Double card id. Faction membership is
read from `FWBCardDefinition::PublicFactions`.

## Continuation Semantics

`FWBPendingAttackState::DamageRecipientUnitId` is continuation-bound private state.
It does not change defender, original defender, defender tile, declaration id,
continuation id, or reaction target.

Generic application requires a live pending PreHit attack, exact continuation id,
and a live on-board recipient. The latest successfully resolved substitution wins.
Damage uses the recipient's current Armor and Frozen state without any attack
geometry recheck. The substitution clears after Damage resolution.

If the selected unit disappears before the effect resolves, immutable-target
validation fails closed. If it disappears after successful substitution but before
Damage, resolution emits `pending_attack_damage_recipient_fallback` and applies the
ordinary attack to the current defender. Defender removal continues to use existing
attack cancellation rules.

## Interactions

- Negate: a negated Body Double frame does not resolve, does not pay RR, and does
  not set the recipient. A negated Negate permits normal Body Double resolution.
- Prevent: successful prevention still prevents the attack after substitution.
- Multiple substitutions: latest successful resolution replaces the earlier one.
- Move/status/Armor changes: target eligibility is checked when selected; after
  resolution, current recipient state is used at Damage and adjacency is not
  rechecked.
- NPC and counter attacks: the generic operation is authority-neutral. Typed Body
  Double legality depends only on a current own Hero defender during PreHit.

## Redirect Cross-Interaction

Tracked canon does not establish whether a true Redirect after a successful damage
substitution clears or preserves that substitution. This pass does not modify
Redirect to guess. The unresolved ordering remains fail-closed at the card-design
boundary: no production fixture combines the two effects, and no Body Double
special case was added to Redirect authority.

## Privacy and Replay

The active recipient is included in the private canonical state digest only while
present. Replay schema remains 1 and `WBActionCodec` is unchanged. Public receipts
remain eight fields and exclude card instance alternatives, continuations, state
digests, and trace digests. Core traces use generic substitution terminology.

## Protected Baselines

Body Double is isolated under `Data/Replay/CSNBodyDoubleFixture/`; the canonical
startup bundle, Active Format, decks, and existing Redirect fixture are unchanged.
