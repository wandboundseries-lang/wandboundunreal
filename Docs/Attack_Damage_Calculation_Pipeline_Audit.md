# Attack Damage Calculation Pipeline Audit

## Previous Path

The committed path was `PreHit -> Damage -> PostHit -> Counter`. Its Damage step
selected a replacement recipient before calculation, checked that recipient's
Frozen status, consumed that recipient's Armor, changed HP, and ran death cleanup.
`WBDamageResolution::ResolveDamageRequest` both calculated and mutated Armor/HP.

## Current Path

Production suspended attacks now use:

`PreHit -> CalculateDamage -> SubstituteDamage -> ApplyDamage -> PostHit -> CounterEligibility -> Counter -> Complete`

Player, neutral-NPC, and counter attacks share this path. The legacy `Damage` enum
and `ApplyPendingAttackDamage` entry point remain compatibility-only; the wrapper
executes the same three typed internal stages.

## Calculation Boundary

`WBDamageResolution::CalculateDamageRequest` is pure. It previews prevention,
consumptive Armor, and HP damage without changing state. `ApplyCalculatedDamage`
fails closed if the calculation subject's HP or Armor changed before apply.

Attack calculation always uses current attacker ATK and the final current defender.
Frozen produces zero HP damage and a pending Frozen-break result. Prevention
produces zero transferable HP damage. Only ApplyDamage changes Armor, HP, Frozen,
or board/death state.

## Identity and Substitution

The state separates defender/hit unit, calculation subject, and final HP recipient.
Substitution can change only the final HP recipient. Hit-unit Armor is still applied;
transferred HP damage bypasses substitute Armor and substitute Frozen because that
unit was not hit. Invalid substitutes fall back to the hit unit without cancelling
the attack.

## Traces and Replay

Generic traces include `attack_damage_calculated`, `attack_damage_substituted` when
active, `attack_damage_applied`, and the compatibility `attack_damage_resolved`.
Automatic stages emit stage traces, including `counter_eligibility`. Conditional
private digest fields cover active substitution and calculation state. Replay schema
remains 1 and `WBActionCodec` is unchanged.

## Future Boundary

The point immediately after ApplyDamage and death resolution, before PostHit, is the
future collection boundary for canonical "after damage is dealt" effects. No such
collector, reduction, amplification, reflection, or new reaction window is added.
