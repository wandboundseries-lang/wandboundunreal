# Wandbound Deterministic Terminal Match and Replay Completion Report

Date: 2026-08-01

## Result

- Editor build: succeeded on final source; final incremental build 54.08 seconds.
- Game build: succeeded; cold validation 346.48 seconds and final incremental validation 47.76 seconds.
- Focused terminal/replay automation: 54 succeeded, 0 failed before the final trace-order case; all 55 cases are included in the final full run.
- Affected legacy groups: passed, including Death, Damage, Combat, MatchCoordinator, Replay, Turn, TurnStart, NPC, Marker, Runtime, Production, and Summon coverage.
- Full automation: increased from 1,901 to 1,955 succeeded, 0 failed, 0 warnings, 0 not run.
- Fresh BuildCookRun: succeeded in 88.86 seconds at `Saved/PackagedBuilds/TerminalReplayCompletionFinal_20260801`.
- Packaged development smoke: passed, exit 0.
- Packaged production startup: passed twice, byte-identical.
- Packaged partial replay regression: passed twice with prior archive, receipt, and protected replay digest preserved.
- Packaged terminal match and fresh replay verification: passed twice, exit 0.
- Repeated terminal archive and receipt: byte-identical.
- Winner/loser: player 0 / player 1 in both runs.
- Post-terminal submission: rejected with `game_over` and no mutation.
- Privacy scan: 0 protected-data hits; receipt remains exactly eight fields.
- Startup JSON: preserved byte-for-byte.
- `git diff --check`: passed after implementation; final rerun is recorded in the build report.
- Exact final errors: none.

## Baseline

- Commit: `8a33440aec9a611b6ae3a80b8303570c9122960a Add production match replay logging`.
- Synchronization: local `main`, `origin/main`, and `HEAD` all matched the baseline.
- Staged files: none. LFS-staged files: none.
- Initial automation: 1,901 succeeded, 0 failed.
- Startup SHA-256: `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- Prior partial archive SHA-256: `d30304a936fd3b5c2163209546b9063a64ed7a223a65b217092cb64ef6495463`.
- Prior partial receipt SHA-256: `881ffb586544d5ed78156635754b5eeddf555c7ef000c472f79ac607cc4d2dd9`.
- Prior protected replay digest: `391f0a6e836fc19439f110a5bd0a748367c00c826e73ad1d615fe53d9b492e7e`.
- Replay schema: v1, extended with conditional terminal fields; nonterminal bytes remain compatible.
- Production bundle digest: `87d2644aeb479e84a3e96967fd57901ac52aa7e283fd5cfee142d35e8659f00c`.
- Ordinary production match-spec SHA-256: `5fd3ba8d78af9681e3acdc4c4b58e7c2934aacbcfed83cd3504a8a37e23b03da`.
- Existing replay-smoke metadata match-spec digest: `bea42bb56a2f20f836229c6e395e6eb24a13b661aad66bedf270f05a630c51f1`.
- `WBActionCodec.h`: source SHA-256 `c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949`, Git blob `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- `WBActionCodec.cpp`: source SHA-256 `ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99`, Git blob `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.

Pre-existing tracked changes were preserved in `Config/DefaultEditor.ini`, the CardDB report, CardDB affordability/cost payment, game-state implementation, public board summary, resonance load, runtime result serialization, equip/public board headers, and the listed equip/resonance/runtime decision tests. Pre-existing untracked Content/Meshy, audit, sentinel, and malformed-name files were not modified. No baseline task file had staged content.

## Canonical Terminal Rule

A zero-HP Hero removed without replacement sets the typed Core outcome `hero_defeated_without_replacement`. The defeated Hero controller is the loser and the other player is the winner. The death transaction finishes unit/equipment cleanup before the coordinator adds terminal traces and completes the accepted action.

The repository has no production Hybrid Hero-replacement executor or canonical production Hybrid fixture. This pass therefore does not invent one. The enforced future boundary is atomic: a valid sacrifice and replacement must complete in one accepted action without an intermediate terminal state or footer.

## Existing Terminal Audit

Supported terminal sources are Attack, Burn status damage, revealed Trap damage, NPC damage, and supported activation effect damage. Each production path converges on `WBDeathResolution`, then the coordinator commits the typed outcome and replay record. Runtime, the replay recorder, the replay runner, and presentation are observers/finalizers, not gameplay authorities. Full path-level evidence is in `Terminal_Match_Replay_Completion_Audit.md` and `.json`.

Simultaneous dual-Hero death is not supported. The death transaction fails before commit with `simultaneous_hero_death_unsupported`, assigns no iteration-order winner, and emits no committed terminal traces.

## Terminal Authority

`WBMatchCoordinator::SubmitActionId` remains the only accepted-action authority. It commits cleanup, winner, loser, typed reason/source, turn, coordinator revision, and terminal trace index; clears pending decision state; exposes an empty legal-action set; and completes the accepted terminal record. Later submissions fail with the existing `game_over` code and leave generation, revision, state, trace, outcome, record count, replay digest, and archive bytes unchanged.

## Hero Replacement

Atomic Hybrid replacement is a documented canonical requirement but is not production reachable in this repository. No synthetic mechanic or test fixture was added. A future implementation must prove old Hero removal plus replacement installation in one transaction, no terminal traces/footer, and continued legal decisions.

## Terminal Trace

The packaged Attack termination preserves this deterministic order:

```text
attack damage event
hero_defeated
terminal_state_committed
game_over
```

The committed terminal event carries defeated player, winner, typed reason, public cause category, turn, and ordering index. No replay-only duplicate or private presentation event was added.

## Replay Final Action

The seventh record remains the normal accepted action `attack:p0:u0:t1`. It records acting player, family, before/after generation and revision, `completed=true`, `pending_decision=false`, `terminal=true`, winner 0, loser 1, canonical reason/source, final trace range/digest, state digest, and record hash. No synthetic terminal action exists, and the rejected later submission is not recorded.

## Replay Footer

The terminal footer contains `complete`, `terminal`, `winner`, `loser`, `terminal_reason`, `terminal_source`, `terminal_turn`, `terminal_generation`, `terminal_revision`, `terminal_trace_index`, `final_generation`, `final_revision`, `record_count`, `final_state_digest`, `final_trace_digest`, `final_record_hash`, and `replay_digest`. `MarkComplete` is idempotent: the second call succeeds without a second footer or byte/digest change. Duplicate footer input is rejected by the loader/validator.

## Replay Runner

A fresh coordinator reloads schema v1, verifies the hash chain, finds each stable action in the current legal set, and submits all seven actions through `SubmitActionId`. It reaches winner 0, loser 1, reason `hero_defeated_without_replacement`, source `attack`, turn 5, generation 1, revision 8, and trace index 129. It verifies no pending decision or legal actions and matches the final state, trace, record, and replay digests. Focused divergence cases reject missing/early terminal state, winner/loser/reason changes, post-terminal records, duplicate footer, and terminal-flag mismatch at the first deterministic divergence.

## Partial Replay Preservation

The prior partial archive remains valid and byte-identical. It remains `complete=false`, `terminal=false`, with winner and loser `-1`. Its archive SHA-256, eight-field receipt SHA-256, and protected replay digest are unchanged. Conditional v1 terminal fields are absent from nonterminal canonical bytes.

## Public Receipt

The receipt remains exactly:

```text
available
schema_version
opaque_match_id
entry_count
complete
terminal
final_replay_digest
failure_code
```

The terminal receipt reports available/complete/terminal true and an empty failure code. Winner, loser, seed, action IDs, protected digests, paths, hidden card/marker identity, and RNG data are absent.

## Packaged Terminal Smoke

- Fixture: `Data/Replay/TerminalFixture/`, outside the production bundle.
- Fixture match-spec file SHA-256: `34ff05a8aa6a160f7e5e0a9d93ab712181fc6cde97b9bad98a9119a1747ba7c0`.
- Loaded fixture bundle digest: `5cba7d72a3491289ecc4ed6c36305e53e58e00279f828e996063c232f732d6d3`.
- Sequence: discard, player 0 EndTurn, player 1 EndTurn, nonlethal player 0 Attack, player 0 EndTurn, player 1 EndTurn, lethal player 0 Attack.
- Terminal cause: visible Attack defeats player 1 Hero without replacement.
- Winner / loser / reason: `0` / `1` / `hero_defeated_without_replacement`.
- Accepted action count: 7.
- Archive: `Saved/PackagedBuilds/TerminalReplayCompletionFinal_20260801/Windows/WandboundUE/Saved/Wandbound/Replays/terminal_replay_smoke_match.wbpmr.json`.
- Archive SHA-256: `4e30424a56b613cbbda225295a0775473ed661cda390f172b609e529450235cc`.
- Receipt SHA-256: `5bcce2e1e9361e8848e4757a634cf82acdee30d2463a01f6f9f0023157e1ca76`.
- Both independent packaged runs produced identical outcome, records, archive bytes, receipt bytes, and digests.

## Determinism

- Final state digest: `72b87f890d2f862da1ea75e4f20857cf0679dc7480407f82a702038876bdaae3`.
- Final trace digest: `e26c1cf1c667ea9a0d563e46929d3025993070ebb99065d15b29b71651f009d1`.
- Final record hash: `a824a0ab7410c7da54d4df5653c38db791ac6d799ed87f852206424fd6345127`.
- Replay digest: `95a9bd178298085120774097e8beea17a266b4ec6c048aa49dc9351f9c50dc6b`.
- Archive, receipt, state, trace, final record, replay digest, winner, loser, and stable action IDs matched across both runs.

## Tests

`WBTerminalMatchReplayCompletionTests.cpp` adds 54 named cases from the requested `Wandbound.Terminal.*` and `Wandbound.Replay.Terminal.*` families plus `Wandbound.Terminal.Trace.DeterministicOrder`. They cover authority, Attack source, lock invariants, final record/footer, idempotence, fresh runner verification, terminal divergence, partial compatibility, receipt privacy, and packaged-contract behavior.

`WBMatchCoordinatorTests.cpp` was extended to assert typed terminal reason/loser/source for activation Effect, Burn status, Trap, and NPC paths. `WBRuntimeMatchHostTests.cpp` verifies the canonical public terminal reason. `WBProductionMatchReplayTests.cpp` stabilizes fixture lifetime while exercising the extended replay layout. The final full suite is the authoritative list and result: 1,955 succeeded.

## Changed Files

| Path | Purpose | Baseline / overlap | Impact |
|---|---|---|---|
| `Data/Replay/ProductionMatchReplay.schema.json` | Conditional terminal record/footer schema | Clean; task-only | Data/replay |
| `Data/Replay/TerminalFixture/*` | Dedicated legal terminal smoke bundle/spec | New | Test data only |
| `Source/WandboundCore/Public/WBTerminalOutcome.h` | Typed terminal outcome and canonical names | New | Core |
| `Source/WandboundCore/Private/WBTerminalOutcome.cpp` | Name mappings | New | Core |
| `Source/WandboundCore/Public/WBGameStateData.h` | Stores authoritative typed outcome | Clean; task-only | Core state |
| `Source/WandboundCore/Private/WBDeathResolution.cpp` | Preliminary transactional Hero-loss outcome | Clean; task-only | Core mutation helper |
| `Source/WandboundCore/Public/WBMatchCoordinator.h` | Terminal operation-result fields | Clean; task-only | Core authority API |
| `Source/WandboundCore/Private/WBMatchCoordinator.cpp` | Source inference, terminal commit, lock/result/record fields | Clean; task-only | Core authority/replay capture |
| `Source/WandboundCore/Public/WBProductionMatchReplay.h` | Terminal record/footer model | Clean; task-only | Replay |
| `Source/WandboundCore/Private/WBProductionMatchReplay.cpp` | Conditional canonical fields, JSON, duplicate-footer validation | Clean; task-only | Replay |
| `Source/WandboundCore/Public/WBPublicTurnSummary.h` | Public terminal loser/reason/source/turn | Clean; task-only | Public DTO |
| `Source/WandboundCore/Private/WBPublicTurnSummary.cpp` | Populate terminal-only public fields | Clean; task-only | Public DTO |
| `Source/WandboundRuntime/Public/WBProductionMatchReplayRuntime.h` | Idempotent finalizer and typed runner result | Clean; task-only | Runtime/replay |
| `Source/WandboundRuntime/Private/WBProductionMatchReplayRuntime.cpp` | Footer finalization and terminal runner verification | Clean; task-only | Runtime/replay |
| `Source/WandboundRuntime/Public/WBProductionTerminalReplaySmoke.h` | Terminal smoke interface | New | Test-only runtime hook |
| `Source/WandboundRuntime/Private/WBProductionTerminalReplaySmoke.cpp` | Coordinator-only packaged terminal sequence | New | Test-only runtime hook |
| `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp` | Adds terminal smoke flag dispatch | Clean; task-only | Runtime bootstrap |
| `Source/WandboundRuntime/Private/WBRuntimeMatchHostComponent.cpp` | Consumes typed public terminal observation | Clean; task-only | Presentation boundary |
| `Source/WandboundRuntime/WandboundRuntime.Build.cs` | Stages terminal fixture files | Clean; task-only | Package data |
| `Source/WandboundTests/Private/WBTerminalMatchReplayCompletionTests.cpp` | Focused terminal/replay coverage | New | Tests |
| `Source/WandboundTests/Private/WBMatchCoordinatorTests.cpp` | Status/Trap/NPC/Effect source assertions | Clean; task-only | Tests |
| `Source/WandboundTests/Private/WBProductionMatchReplayTests.cpp` | Stable extended-layout fixture observation | Clean; task-only | Tests |
| `Source/WandboundTests/Private/WBRuntimeMatchHostTests.cpp` | Canonical public terminal reason | Clean; task-only | Tests |
| `Docs/Terminal_Match_Replay_Completion_Audit.md` | Human authority audit | New | Documentation |
| `Docs/Terminal_Match_Replay_Completion_Audit.json` | Machine-readable path audit | New | Documentation |
| `Docs/Terminal_Match_Replay_Completion_Report.md` | Validation and implementation evidence | New | Documentation |
| `Docs/Build_Test_Report.md` | Project validation ledger | Clean; task append | Documentation |

No task hunk was added to a file known dirty at baseline. In particular, existing changes in `WBGameStateData.cpp`, public board/runtime serialization, CardDB/equip/resonance code, config, and unrelated tests remain outside this task.

## Git Status

- Task files: the paths in the Changed Files table only.
- Pre-existing tracked changes: preserved exactly as listed in Baseline; none staged.
- Pre-existing untracked files: Content/Maps, Content/MeshyImports, Plugins/meshy/Content, two unrelated audit documents, `MaxHP`, `RLTotal`, and five malformed-name/sentinel files; untouched.
- Staged files: none.
- LFS-staged files: none.
- Generated replay/package/test output: under ignored `Saved/`; not added to Git.
- No `WBActionCodec`, production bundle/spec, config default, Godot reference, Meshy, map, Blueprint, model, `.uasset`, or `.umap` task change was made.

## Remaining Risks

1. Hybrid Hero replacement is canonical but has no production implementation or fixture; its required atomic nonterminal behavior remains unproven until that system exists.
2. Simultaneous dual-Hero defeat has no canonical result. The engine intentionally rejects it with `simultaneous_hero_death_unsupported` rather than assigning a winner by iteration order.

## Recommended Next Task

Implement the canonical atomic Hybrid Hero-replacement production path, including coordinator-owned transaction, nonterminal trace/replay coverage, and a legal production fixture, without changing existing Hero-defeat semantics.
