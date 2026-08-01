# Wandbound Production Match Replay Log Report

Date: 2026-08-01

## Result

- Editor build: succeeded on final source in 57.2 seconds.
- Game build: succeeded on final source in 64.1 seconds.
- Focused replay automation: 85 succeeded, 0 failed, 0 warnings, 0 not run.
- Requested affected areas within the final full run: 1,234 unique tests succeeded, 0 failed.
- Initial full automation: 1,816 succeeded. Final full automation: 1,901 succeeded, 0 failed, 0 warnings, 0 not run.
- Fresh BuildCookRun: succeeded in 58.8 seconds; package `Saved/PackagedBuilds/ProductionReplayReceiptFinal_20260801`.
- Packaged development smoke: passed, exit 0, generation 1, revision 4.
- Packaged canonical production startup: passed twice, exit 0 both runs.
- Packaged replay creation and fresh replay verification: passed twice, exit 0 both runs, 3 records (`discard`, `end_turn`, `discard`).
- Repeated archive and receipt comparisons: byte-identical and SHA-256-identical.
- Startup JSON comparison: byte-identical and preserved established SHA-256.
- Privacy allowlist scan: 8 required fields, no missing/extra fields, 0 protected-value hits.
- `git diff --check`: passed with line-ending notices only.
- Exact final errors: none.

An initial direct invocation of the packaging PowerShell script was blocked by the machine execution policy before Unreal ran. The approved process-scoped `-ExecutionPolicy Bypass` invocation succeeded. A public receipt key was aligned from `record_count` to the requested `entry_count`; all final validation above was rerun afterward.

## Architecture

`WBMatchCoordinator` is the only committed-decision authority. It computes the before checkpoint from its protected state, current legal action set, phase, generation, and revision; submits through the existing production path; then appends one immutable record only when submission succeeds. Rejected submissions never enter replay identity.

`WBProductionMatchReplay` serializes a server-private typed archive. `WBProductionMatchReplayPersistence` writes it below `Saved/Wandbound/Replays` using a sibling temporary file and replacement. `FWBProductionMatchReplayRunner` loads a fresh production snapshot and submits each recorded stable action ID through `WBMatchCoordinator::SubmitActionId`. `FWBProductionMatchReplayReceipt` is a separate allowlisted public view.

## Replay Schema

Header fields:

- `schema_version`, `replay_format_id`, and `rules_compatibility_version` establish compatibility.
- `opaque_match_id` gives a caller-controlled stable non-personal identifier.
- production bundle, match-spec, Active Format, and addendum digests pin immutable sources.
- seed, initial generation/revision, and initial state/trace digests pin deterministic setup.
- previous/header hashes start the integrity chain.

Accepted-action fields:

- index, acting seat, action family, chosen stable ID, and expected decision ID identify the accepted decision.
- before/after generation and revision detect stale or repeated execution.
- before/after protected state digests and legal-action-set digest verify hidden deterministic context without storing snapshots or legal lists.
- completed, pending decision/player, and terminal fields preserve coordinator boundaries.
- trace range and digest verify automatic work caused by the action without serializing protected traces.
- previous/record hashes continue the chain.

Footer fields:

- complete/terminal/winner/loser preserve completion semantics.
- final generation/revision, record count, state/trace digests, final record hash, and replay digest close and authenticate the archive.

Partial nonterminal archives remain valid and explicitly carry `complete=false`.

## Capture And Replay

- Family classification happens before mutation while the selected legal action is available.
- State, legal-set, and decision checkpoints are captured before execution.
- Revision, pending state, terminal state, trace range/digest, and state digest are captured after the coordinator finishes or pauses.
- Persistence runs only after accepted coordinator commits. A write failure disables recording but does not retry, roll back, or mutate gameplay.
- Fresh replay validates schema, chain, compatibility/source digests, normal production setup, initial checkpoints, current legal membership, decision/player context, and every post-submit checkpoint. It stops at the first divergence.
- Durable turn-start ordering is replayed as a normal coordinator-submitted ID. Tests verify resume does not duplicate draw, MP roll, attack/wall reset, or status tick work.

## Privacy

Protected archive data includes the deterministic seed, stable action IDs, acting seat, source digests, coordinator checkpoints, and protected state/trace/legal-set digests. The public receipt is limited to availability, schema version, opaque match ID, entry count, completion/terminal flags, final replay digest, and a typed failure code.

The receipt excludes hands, decks, hidden markers, action IDs, legal lists, seed, protected state/trace contents and digests, RNG state, and paths. Replay metadata is not added to production startup JSON, HUD data, public observations, or presentation events.

## Persistence And Smoke Fixture

Default archives are written to `Saved/Wandbound/Replays/{opaque_match_id}.wbpmr.json`; `Saved/` is already ignored. Each write first closes a sibling `.tmp`, then replaces the destination. A failed replacement preserves the previous valid file.

The canonical production match specification intentionally remains unchanged. Its opening setup consumes the available non-Hero deck cards, so it cannot exercise the next-player turn-start draw. `Data/Replay/production_replay_smoke_match_spec.json` is therefore a replay-smoke-only staged specification using existing production card definitions with one additional unique Character per deck. It changes no production definitions or rules.

## WBActionCodec

- Header baseline SHA-256: `c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949`.
- Source baseline SHA-256: `ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99`.
- The replay implementation calls no codec-specific replay branch and creates no alternate action-ID algorithm.

## Automation Coverage

The 85 tests in `WBProductionMatchReplayTests.cpp` use the exact `Wandbound.Replay.Production.*` names requested. They cover:

- schema: valid, unsupported, missing header/record/footer fields;
- serialization: byte identity, field order, absence of wall clock and paths;
- hash chain: header/action edits, deletion, insertion, reorder, footer edit, final digest;
- capture: accepted/rejected, family, before/after revisions, pending/terminal, trace range, stable ID;
- families: all classifier values, round trip, unknown guard, automatic-event exclusion, durable choices;
- turn start: order record/replay, no duplicate draw/roll/reset/tick, dead source, stale/reordered choice, partial pending archive;
- runner: coordinator use, initial state, legal membership, wrong player/decision/action, state/trace/revision/pending divergence, terminal and partial replay;
- privacy: public observation separation and absence of seed, actions, hands, decks, markers, traces, paths, and startup changes;
- persistence: intermediate/final archive, preserved previous file, no repeat/mutation on failure, truncation, and Saved location;
- guards: codec, ID algorithm, state mutation, turn/effect bypass, RNG, presentation, Godot, Meshy, maps/models/assets;
- package/runtime: archive, replay, safe receipt, repeated bytes, startup bytes, and provider refresh.

Exact test names and category purposes:

- Schema acceptance/rejection: `Wandbound.Replay.Production.Schema.ValidArchiveAccepted`, `UnsupportedVersionRejected`, `MissingHeaderRejected`, `MissingRecordFieldRejected`, `MissingFooterRejected`.
- Canonical serialization: `Wandbound.Replay.Production.Serialization.ByteIdentical`, `StableFieldOrder`, `NoWallClockData`, `NoAbsolutePaths`.
- Hash-chain integrity: `Wandbound.Replay.Production.Hash.HeaderMutationDetected`, `ActionMutationDetected`, `RecordDeletionDetected`, `RecordInsertionDetected`, `RecordReorderDetected`, `FooterMutationDetected`, `FinalDigestVerified`.
- Coordinator capture: `Wandbound.Replay.Production.Capture.AcceptedActionRecorded`, `RejectedActionNotRecorded`, `ActionFamilyCaptured`, `BeforeRevisionCaptured`, `AfterRevisionCaptured`, `PendingDecisionCaptured`, `TerminalCaptured`, `TraceRangeCaptured`, `StableActionIdPreserved`.
- Family guards: `Wandbound.Replay.Production.Coverage.AllProductionFamiliesClassified`, `AllProductionFamiliesRoundTrip`, `UnknownFamilyFailsGuard`, `AutomaticEventsNotFakeActions`, `AllDurableChoicesStable`.
- Turn-start durability: `Wandbound.Replay.Production.TurnStart.OrderChoiceRecorded`, `OrderChoiceReplayed`, `ResumeDoesNotRedraw`, `ResumeDoesNotReroll`, `ResumeDoesNotResetTwice`, `ResumeDoesNotRetick`, `DeadSourceCreatesNoChoice`, `StaleChoiceRejected`, `PartialPendingArchiveValid`, `ReorderedChoiceMismatch`.
- Fresh runner divergence: `Wandbound.Replay.Production.Runner.UsesCoordinator`, `InitialStateVerified`, `ActionFoundInLegalSet`, `WrongPlayerDetected`, `WrongDecisionDetected`, `IllegalActionDetected`, `StateDivergenceDetected`, `TraceDivergenceDetected`, `RevisionDivergenceDetected`, `PendingStateDivergenceDetected`, `TerminalResultVerified`, `PartialReplayVerified`.
- Public privacy: `Wandbound.Replay.Production.Privacy.ArchiveNotInPublicObservation`, `SeedNotInReceipt`, `ActionIdsNotInReceipt`, `HandsNotInReceipt`, `DecksNotInReceipt`, `ConcealedMarkersNotInReceipt`, `PrivateTraceNotInReceipt`, `PathsNotInReceipt`, `StartupJsonUnchanged`.
- Persistence behavior: `Wandbound.Replay.Production.Persistence.IntermediateArchiveWritten`, `FinalArchiveWritten`, `PreviousValidFilePreserved`, `WriteFailureDoesNotRepeatAction`, `WriteFailureDoesNotAlterMatch`, `TruncatedArchiveRejected`, `GeneratedFilesRemainUnderSaved`.
- Source/authority guards: `Wandbound.Replay.Production.Guard.ActionCodecUnchanged`, `NoSecondActionIdAlgorithm`, `NoDirectGameStateMutation`, `NoTurnControllerBypass`, `NoEffectRunnerTransitionBypass`, `NoGameplayRNGOwnership`, `NoPresentationDerivedRecords`, `NoGodotChanges`, `NoMeshyChanges`, `NoModelMapOrAssetChanges`.
- Package/runtime behavior: `Wandbound.Replay.Production.Package.ArchiveCreated`, `ReplayVerified`, `ReceiptPublicSafe`, `RepeatedArchiveByteIdentical`, `RepeatedReceiptByteIdentical`, `StartupResultByteIdentical`, and `Wandbound.Replay.Production.Runtime.ProviderRefreshAfterRecord`.

## Changed Files

Core:

- `Source/WandboundCore/Public/WBProductionMatchReplay.h` and `Private/WBProductionMatchReplay.cpp`: typed replay model, canonical protected/public serialization, digests, and validation.
- `Source/WandboundCore/Public/WBMatchCoordinator.h` and `Private/WBMatchCoordinator.cpp`: generation/revision checkpoints and accepted-action stream. These files were clean at baseline.
- `Source/WandboundCore/WandboundCore.Build.cs`: OpenSSL SHA-256 dependency. Clean at baseline.

Runtime:

- `Source/WandboundRuntime/Public/WBProductionMatchReplayRuntime.h` and `Private/WBProductionMatchReplayRuntime.cpp`: metadata, persistence, recorder, and fresh runner.
- `Source/WandboundRuntime/Public/WBProductionMatchReplaySmoke.h` and `Private/WBProductionMatchReplaySmoke.cpp`: packaged record/reload/replay smoke.
- production bootstrap, runtime host, bootstrap actor, and Runtime Build.cs: minimal metadata, lifecycle, and smoke integration. Clean at baseline.

Data/tests/docs:

- `Data/Replay/ProductionMatchReplay.schema.json`: tracked versioned schema.
- `Data/Replay/production_replay_smoke_match_spec.json`: packaged smoke-only production-data fixture.
- `Source/WandboundTests/Private/WBProductionMatchReplayTests.cpp`: 85 automation tests.
- this report and `Production_Match_Replay_Log_Audit.md/.json`: audit and evidence.

No task file was dirty at baseline. The unrelated tracked and untracked baseline files were preserved.

## Validation

Final packaged SHA-256 evidence:

- Startup JSON: `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004` both runs; bytes equal.
- Private archive file: `d30304a936fd3b5c2163209546b9063a64ed7a223a65b217092cb64ef6495463` both runs; bytes equal.
- Public receipt file: `881ffb586544d5ed78156635754b5eeddf555c7ef000c472f79ac607cc4d2dd9` both runs; bytes equal.
- Protected replay digest inside archive/receipt: `391f0a6e836fc19439f110a5bd0a748367c00c826e73ad1d615fe53d9b492e7e`.

The receipt fields were exactly `available`, `schema_version`, `opaque_match_id`, `entry_count`, `complete`, `terminal`, `final_replay_digest`, and `failure_code`. A scan using the actual packaged seed, chosen action ID, legal-set digest, before/after state digests, trace digest, path, and hidden-zone/marker tokens found zero matches.

Final `WBActionCodec` hashes equal baseline:

- `WBActionCodec.h`: SHA-256 `c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949`, Git blob `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- `WBActionCodec.cpp`: SHA-256 `ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99`, Git blob `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.

No codec diff exists.

## Git Status

Task files are the Core/runtime/schema/smoke/test/docs files listed above plus the minimal coordinator, bootstrap, host, actor, and Build.cs integrations. Generated replay, receipt, report, staging, and package files remain below ignored `Saved/`.

Pre-existing tracked changes were preserved in `Config/DefaultEditor.ini`, `Docs/CardDB_Unreal_Bundle_Schema_Validation_Report.md`, CardDB affordability/payment, game state, public summary, resonance, runtime result, equip header, and nine existing equip/resonance/runtime test files.

Pre-existing untracked content was preserved under `Content/Maps`, `Content/MeshyImports`, `Plugins/meshy/Content`, the two unrelated audit files, `MaxHP`, `RLTotal`, and the five stray terminal-named files. Staged files and LFS-staged files remain none.

## Remaining Risks

- Current production behavior does not expose response-window Pass React, standalone Pass, wall-edit choices, marker choices, or resonance allocation through the coordinator, so they are guarded but cannot be live-recorded until those production decisions exist.
- Archive storage is local server-private plaintext under `Saved`; encryption, remote durability, recovery UI, and background writes are intentionally out of scope.
- The packaged replay fixture is a smoke-specific specification, not the canonical production match specification.

## Recommended Next Task

Add deterministic production match completion and terminal replay coverage through the existing coordinator, including winner/loser footer verification, without adding UI, networking, save/load, or response windows.
