# Terminal Match Replay Completion Audit

Date: 2026-08-01

Baseline: `8a33440aec9a611b6ae3a80b8303570c9122960a`

## Authority Finding

`WBMatchCoordinator::SubmitActionId` is the sole production authority that accepts an action, commits its terminal consequence, clears the pending decision, records the accepted action, and exposes the completed result. `WBDeathResolution` is a transactional mutation helper: it removes zero-HP units and attached equipment and identifies a Hero loss, but it does not own accepted-action authority or replay finalization. Runtime code observes the committed outcome and cannot create or change it.

The committed order is:

```text
accepted action begins
damage or removal resolves
zero-HP cleanup resolves transactionally
hero_defeated
terminal_state_committed
game_over
accepted-action replay record completes
completion footer finalizes once
```

## Terminal-Capable Paths

| Classification | Path / symbol | Damage or removal source | Hero defeat | Replacement | Terminal check / owner | Assignment and reason | Trace and replay | Production / tests | Required follow-up |
|---|---|---|---|---|---|---|---|---|---|
| CoordinatorOwned | `WBMatchCoordinator::SubmitActionId` | Accepted Attack, activation, movement/Trap, EndTurn status, or NPC consequence | Yes | No replacement path invoked by current actions | Post-action committed path / coordinator | Opponent wins; defeated Hero owner loses; `hero_defeated_without_replacement` | Adds `terminal_state_committed`, `game_over`; completes normal accepted-action record | Production; terminal authority, lock, replay, runner tests | None for supported rule |
| FocusedMutationHelper | `WBDeathResolution::ResolveZeroHPUnits` | Zero-HP unit cleanup after any supported damage path | Yes | No production Hybrid replacement input exists | Transactional working copy / coordinator finalizes | Preliminary typed outcome; source is finalized by coordinator | Unit/equipment cleanup and `hero_defeated` | Production; zero-HP and source integration tests | Keep helper non-authoritative |
| CoordinatorOwned | Attack execution in `WBMatchCoordinator` | Visible attacker combat damage | Yes | No | Coordinator after death cleanup | Source `attack` | Damage, `hero_defeated`, terminal traces, accepted record | Production; `Wandbound.Terminal.Source.AttackDamage` | None |
| CoordinatorOwned | End-turn status processing | Burn status damage | Yes | No | Coordinator after status/death cleanup | Source `status` | Status/damage then terminal traces; accepted EndTurn record | Production; updated coordinator status test | None |
| CoordinatorOwned | Marker resolution after movement/summon | Revealed Trap damage | Yes | No | Coordinator after marker/death cleanup | Source `trap` | Reveal/damage then terminal traces; accepted action record | Production; updated coordinator Trap test | None |
| CoordinatorOwned | NPC phase resolution | NPC attack damage | Yes | No | Coordinator after NPC/death cleanup | Source `npc` | NPC damage then terminal traces; accepted EndTurn record | Production; updated coordinator NPC test | None |
| CoordinatorOwned | Activation generic effect resolution | Supported damage effect | Yes | No | Coordinator after effect/death cleanup | Source `effect` | Effect/damage then terminal traces; accepted activation record | Production; updated coordinator activation test | None |
| Unsupported | Hybrid Hero replacement | Hero sacrifice plus atomic replacement | Potentially, but must not be terminal when replacement succeeds | Canon requires atomic replacement | No current production data/executor path to audit | Not implemented | No supported trace/replay fixture | Not production reachable | Add only when a canonical Hybrid production path exists |
| Unsupported | Simultaneous dual-Hero death | One cleanup batch defeats both Heroes | Both | None | `WBDeathResolution` rejects before commit | No winner assigned; `simultaneous_hero_death_unsupported` | No terminal traces committed | Explicit zero-HP negative coverage | Canon decision required; do not infer winner from iteration order |
| ReplayFinalizer | `FWBProductionMatchReplayRecorder::MarkComplete` | None | Observes committed terminal | N/A | Reads coordinator outcome | Copies typed outcome into footer | Idempotent: repeated completion does not append or alter bytes | Production replay smoke and automation | None |
| TerminalObserver | `FWBProductionMatchReplayRunner::Run` | Replays accepted records | Verifies expected terminal point | N/A | Fresh coordinator remains authority | Verifies winner, loser, reason, source, turn, revision, generation | Verifies chain, record/footer, state/trace/final/replay digests | Production runner and divergence tests | None |
| PublicPresentation | `FWBPublicTurnSummary::FromState`, `UWBRuntimeMatchHostComponent` | None | Observes terminal | N/A | No terminal mutation | Exposes public winner, loser, reason, source, terminal turn | Preserves committed ordering | Runtime host automation | None |
| TestOnly | `FWBProductionTerminalReplaySmoke::Run` | Deterministic Attack sequence | Exactly one Hero | None | Submits only through coordinator | Verifies player 0 winner, player 1 loser | Finalizes, persists, reloads, replays, and rejects later input | Packaged terminal smoke | Fixture must remain outside production bundle |

## Replacement Finding

No production Hybrid replacement implementation or production Hybrid data exists in the audited repository. Creating a synthetic replacement operation would expand unsupported gameplay, so this pass does not fabricate the requested replacement regression. The canonical constraint is recorded: when implemented, removal of the old Hero and installation of the new Hero must be one accepted-action transaction with no intermediate `hero_defeated`, `terminal_state_committed`, `game_over`, or replay footer.

## Terminal Lock

After `TerminalOutcome.bTerminal` is committed, the coordinator returns no legal actions. A later submission fails with the existing typed-equivalent code `game_over`. It does not change state, trace, winner, loser, revision, generation, accepted replay record count, replay digest, or persisted bytes.

## Privacy

The terminal outcome carries only public cause categories (`attack`, `status`, `trap`, `npc`, or `effect`). It does not expose hidden card identity, concealed marker identity, deck order, private candidates, state digests, trace digests, RNG internals, or paths. The public replay receipt remains the existing exact eight-field allowlist.
