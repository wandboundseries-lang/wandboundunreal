# Attack Declaration Report

## Scope

This pass adds deterministic attack declaration only. It validates whether a unit may declare an attack, spends one `AttacksLeft`, emits a replay trace, and exposes the action through legal action generation, selected-action execution, action codec IDs, replay traces, and runtime result serialization.

## Legality

- Action kind must be `Attack`.
- Game must not be over.
- Acting player must be a valid current/priority player in normal turn.
- Attacker and defender must exist.
- Attacker owner must match the acting player.
- Defender must not share the attacker's owner, except the later damage-resolution pass permits friendly Frozen targets for break-Frozen attacks.
- Attacker and defender tiles must be in bounds.
- Attacker must have `AttacksLeft > 0`.
- Stunned, Frozen, Cannot Attack, `cannot_attack`, and `no_attack` block declaration.
- Attacker and defender must be orthogonally aligned.
- Orthogonal distance must be within attacker AR.
- LOS must not be blocked by a wall or intervening unit.

## Runtime Behavior

- `WBEffectRunner::ApplyAttackDeclare` decrements the attacker's `AttacksLeft` by one.
- It emits one `attack_declared` trace with player, source unit, target unit, source tile, target tile, and attacks-left before/after fields.
- It does not change HP, MaxHP, MP, turn state, or response state. The later damage-resolution pass stages deterministic pending attack state for resolution.

## Action ID

Attack action IDs use:

```text
attack:p{player}:u{source}:t{target}
```

Existing Move, EndTurn, Pass, and PassResponse IDs are unchanged.

## Legal Action Ordering

For the current priority player during normal turn:

1. Existing Move actions remain first.
2. Legal Attack actions are appended after movement.
3. EndTurn remains last.

Attack ordering is deterministic by attacker Y, X, UnitId, then target Y, X, UnitId.

## Intentionally Not Implemented

- attack damage
- Frozen break-on-hit resolution
- pre-hit/post-hit response windows
- counterattacks
- attack passives
- wands
- terrain attack modifiers
- cards/effects
- VFX/UI/3D runtime
