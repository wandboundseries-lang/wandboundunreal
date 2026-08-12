# NPC Combat and Counterability Audit

## Baseline

- Repository baseline: `b4d36ba19f0c9086960d9a448e13947e64a8a389` (`Add suspended attack continuation`).
- `HEAD` and `origin/main` matched before work. Nothing was staged.
- Replay schema version: 1.
- `WBActionCodec` header/source blob hashes: `44ef87156beb5799066c2a5ecbc98f04928d98c0` and `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.
- The repository had unrelated dirty Config, RL, activation, public-summary, equip, runtime test, asset, Meshy, and scratch work. Those files were not overwritten or reverted.

## Canonical Sources

Tracked canon establishes PreHit and PostHit as reaction timings and counter as a later attack stage. The read-only Godot implementation supplies the previously missing neutral-NPC priority and continuation details:

- `Reference/GodotProject/godotcanon/autoload/Game.gd:26144` declares NPC attacks into the shared pending attack at `pre_hit`.
- `Game.gd:26177` opens the first response priority for the attacked unit's valid player owner. Therefore an NPC attacking Player 0 gives Player 0 first priority, and an NPC attacking Player 1 gives Player 1 first priority.
- `Game.gd:17334` accepts an explicit first-priority player when opening a response window.
- `Game.gd:17872` resets the pass streak after a successful React and alternates priority.
- `Game.gd:17952` auto-passes players with no legal response. The ordinary two-consecutive-pass close rule remains in force.
- `Game.gd:23658` and `Game.gd:23718` pause the NPC phase while response/attack work is pending, retain queue index, MP, and attacks, and resume the same NPC before later spawn-order entries.
- `Game.gd:22340` gives PostHit first priority to the defender owner. A neutral defender falls back to the non-current player.
- `Game.gd:22390` reads `cannot_be_countered` from the original attacker only at the counter stage.
- `Game.gd:22419` gives a counter's PreHit priority to the counter target's player owner. A neutral counter target receives no manual PreHit response; counter PostHit still follows the existing neutral-defender fallback.
- `Game.gd:15721` recomputes `cannot_be_countered` from currently equipped Wand rule effects.
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/wands_equip.json:74` stores the rule as `equipped_unit_cannot_be_counter_attacked=true`; line 80 provides the public wording `Equipped unit cannot be counter-attacked.`

The read-only Godot sources were inspected only. No Godot file was compiled, imported, or modified.

## Findings

### NPC priority and continuation

- Both players may React when their existing source gates make an action legal.
- The attacked player's owner receives first priority for NPC PreHit and PostHit.
- Priority alternates normally after a pass or React.
- A React resets consecutive passes.
- Players without a legal React auto-pass through coordinator authority.
- Two consecutive passes close the window.
- NPC automation suspends completely while a player decision, pending effect, or pending attack stage remains.
- The existing spawn-order queue, current NPC, path step, MP roll, attacks left, and selected declared target are retained. Later NPCs do not advance early.
- Player defenders counter neutral NPC attackers under the shared survival, status, Frozen, range, LOS, ATK, and no-chain rules.
- A neutral NPC may counter a player attack when otherwise legal. Counters do not consume attack resources.

### Counterability

- The semantic owner is the original attacker: the capability means the defender cannot perform the automatic counterattack against that attack.
- The capability does not suppress PreHit, PostHit, attack prevention, effect negation, or ordinary React actions.
- Godot derives the current value from equipped Wands and checks it dynamically at the counter stage.
- No tracked evidence establishes a current status, terrain, global, or temporary-effect source.
- The current Unreal `InitialCanonical` production bundle contains no counter-immunity assignment. Production data was therefore not changed. Isolated fixture definitions exercise the generic capability.

## Unreal Architecture

- `EWBAttackAuthorityKind` distinguishes player and neutral-NPC attacks without assigning a fake player ID.
- `FWBPendingAttackState` remains the only attack continuation.
- `FWBNPCPhaseContinuationState` stores only deterministic NPC phase resume data; it does not duplicate attack stages.
- `WBMatchCoordinator` opens, advances, closes, and resumes all player-facing combat decisions.
- `WBNPCPhaseResolution::BeginPhase` creates the stable queue; `AdvanceUntilAttackOrComplete` pauses after declaration and returns authority to the coordinator.
- `EWBCombatCapability::AttacksCannotBeCountered` is a typed rules capability.
- `WBRules::UnitHasCombatCapability` queries intrinsic unit capabilities and currently equipped typed card definitions.
- `WBRules::CanResolveCounterattack` applies the generic attacker-side gate with reason `attack_cannot_be_countered`.
- The production CardDB accepts only the typed equip token `attacks_cannot_be_countered`; unknown capability tokens fail closed.
- No combat module branches on a card ID or NPC definition ID.

## Replay and Privacy

- Automatic NPC declaration and continuation remain trace events, not fabricated player actions.
- Player React and `PassResponse` submissions remain accepted replay actions.
- Active neutral authority and active NPC resume state participate in private state digests only when present.
- States without the new capability or NPC suspension retain their previous canonical digest input.
- No public observation field exposes the private queue, continuation details, hidden hand, hidden candidates, or protected digests.
- Receipt schema remains eight fields and replay schema remains version 1.

## Explicit Boundaries

- No named card was added to the current production bundle.
- No Crash-In retargeting, Oddsman, Sealplate, Null Sigil, Claimshifter, Sever Thread, Shatter Parry, Oathchain, declaration-time ATK snapshot, after-damage collector, UI, Blueprint, asset, networking, AI, or save/load work was implemented.
- The canonical Godot behavior suppressing manual PreHit for a player counter whose target is neutral was preserved. No broader neutral-response rule was invented.
