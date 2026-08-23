# CSN Sable Production Transfer Audit

## Baseline

- Repository baseline: `ca5954b441c5e06ec6113420f3d172af86e7531d`
- Baseline commit: `Add production CSN Rook destruction trigger`
- `HEAD` and `origin/main` matched before implementation.
- Baseline automation: 2,332 succeeded, 0 failed, 0 warnings, 0 not run.
- Replay schema remains 1.
- No files were staged, committed, pushed, restored, reset, cleaned, or deleted.

## Canonical Card

Production CardDB now contains `char_csn_sable`, `CSN Sable`, as a CSN Character with HP 10, ATK 1, AR 3, and RL 2. Movement is orthogonal adjacent; attack is orthogonal line at range 3. Public rules text is:

> CSN Inheritance: If this unit is summoned by a CSN unit's effect, transfer that unit's equipped Wands to this unit. Increase this unit's Base RL by that unit's Current RL.
>
> Whenever a CSN unit you control is destroyed, this unit gets +1 ATK, +1 HP, and +1 Max HP.

The read-only Godot reference confirms these stats and behavior. Godot identifies CSN cards by CardId prefix; Unreal deliberately does not port that authority.

## Generic Destruction Observer

`FWBAfterUnitDestroyedTriggerDefinition` now supports:

- source scope `ControlledFactionUnitDestroyed`;
- operation `ApplyPersistentStatDeltaToTriggerSource`;
- target `TriggerSource`;
- reusable ATK, MaxHP, and current HP deltas.

At the committed destruction boundary, `WBDeathResolution` captures candidate on-board observer identities in stable controller, source-unit, source-card order. It excludes the destroyed unit and applies the shared automatic passive eligibility policy at capture time. The immutable card repository is consulted later to match those captured identities to definition-driven observer triggers and the destroyed unit's faction metadata.

This split prevents retroactive observers without putting CardDB access in death resolution. A unit summoned after destruction is absent from the frozen candidate snapshot and cannot observe that historical event. Source unit ID, CardId, controller, and source order are retained; hidden card choices are not.

At resolution, the exact source unit identity must still be on the board with the captured CardId and controller. A removed source produces a deterministic skipped result; it is not resurrected or redirected. Event-time passive eligibility is retained if status changes later, matching the queued-trigger policy. Negated, Stunned, or Frozen sources at the event boundary are not captured.

## Stat Mutation

`WBUnitStatDelta` applies one transactional working-copy mutation for all three changes. It validates source identity, board availability, integer overflow, and resulting stat bounds before committing anything. MaxHP is calculated before current HP is increased and clamped. The successful trace is `unit_stat_delta_applied`, including previous/new ATK and public HP/MaxHP values.

The current HP increase is a direct persistent stat increase, not a heal. It does not enter healing prevention or modification behavior. A damaged Sable gains exactly one current HP while its MaxHP rises by one.

## Ordering And Composition

For each destruction event, existing `DestroyedSelf` triggers retain first position. Observer triggers follow in stable controller, source unit ID, and trigger ID order. Multiple source units remain distinct by unit identity, and separate destruction events each produce independent observer applications.

When Rook and Sable observe the same Rook destruction, Rook's self trigger runs first. A non-empty Rook Deck candidate set opens the existing private `MandatoryDeckChoice`, including the one-candidate case. The already-captured Sable observer remains queued and resumes exactly once after the explicit choice. Rook summon and CSN Inheritance semantics are unchanged.

The real Crash-In replacement path proves both sides of the boundary:

- Sable summoned after Rook's destruction receives CSN Inheritance but no retroactive Sable growth.
- A pre-existing Sable is captured at Rook's destruction and grows once after the parent replacement transaction reaches its legal trigger boundary.
- Pending attack redirect/continuation is preserved with no second attack declaration or duplicate spend.

A destroyed Sable cannot observe itself, while another surviving eligible Sable can observe that CSN destruction. Terminal Hero loss clears queued post-destruction work before irrelevant observer mutation.

## Definition Authority

Destroyed faction qualification uses `FWBCardDefinition::PublicFactions`; it does not inspect CardId prefixes, public names, art, or models. Tests prove an alternate-ID CSN observer definition works and a Sable-like identity without the trigger definition does not gain behavior. Changed production gameplay and CardDB authority contain no `char_csn_sable`, `CSN Sable`, or Sable-specific branch.

## Replay And Privacy

Observer candidates and cursor state are included in the private deterministic state digest. Automatic observer resolution adds no player action and requires no replay schema change. Replay trace serialization includes deterministic previous/new ATK values. Fresh replay reproduces final state and trace digests.

Opponent observation hides the private Rook candidate set and exact Deck instance IDs. The Sable smoke accepts the mandatory choice only from the controller's private observation. Public receipt serialization remains exactly eight fields and excludes Sable identity, state digest, trace digest, paths, actions, hidden candidates, and private zones.

`WBActionCodec` is byte-identical to baseline and has no working-tree diff:

- `Source/WandboundCore/Public/WBActionCodec.h`: `44ef87156beb5799066c2a5ecbc98f04928d98c0`
- `Source/WandboundCore/Private/WBActionCodec.cpp`: `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`

## Tests

New focused coverage consists of nine `Wandbound.CSNSable.*` tests plus `Wandbound.CSNCrashIn.Regression.SableObserverUsesDestructionBoundary`. It covers production data, all five canonical destruction causes, faction/controller qualification, non-trigger identities, passive suppression, event-time capture, later status changes, missing sources, multiple Sables, multiple deaths, self-destruction, transactional direct stat growth, Rook pause/resume, terminal handling, deterministic trace/state serialization, schema/codec/receipt guards, real Crash-In boundaries, and production smoke/fresh replay.

Validation results:

- Focused Sable tests: passed.
- Rook, Undertow Archivist, Crash-In, and all CSN regressions: 58 succeeded, 0 failed.
- Full Wandbound automation: 2,342 succeeded, 0 failed, 0 warnings, 0 not run (baseline +10).

## Build Matrix

- Editor Development non-unity: succeeded.
- Game Development non-unity: succeeded (270 actions in the finalized cold run).
- Editor Development default: succeeded.
- Game Development default: succeeded.
- Editor Development forced unity with adaptive unity disabled: succeeded.
- Game Development forced unity with adaptive unity disabled: succeeded.

The final clean BuildCookRun rebuilt Editor and Game, cooked 459 packages, staged, packaged, and archived successfully. AutomationTool exited 0 after 558.93 seconds. The resulting package includes `Data/Replay/CSNSableFixture/match_spec.json` as a staged NonUFS dependency.

## Packaged Production Smoke

The inner packaged executable was run twice with the explicit local-play map, required `WandboundUE` bootstrap argument, exact argument-array transport, and package-relative paths:

- `Data/CardDB/Production/CSNCrashIn/root_manifest.json`
- `Data/Replay/CSNSableFixture/match_spec.json`
- `-WandboundProductionCSNSableSmoke`

Both runs self-terminated with process exit code 0. Each used production providers and coordinator legal actions to summon a real Sable before two friendly CSN destructions. The second destruction was Rook, including an explicit private mandatory Deck choice before the queued observer resumed. Final Sable stats were ATK 3, HP 12, MaxHP 12. Persistence and a fresh coordinator replay were verified in-process.

Repeated artifacts were byte-identical:

- archive SHA-256: `e4d84770703583a3a657aea10ad21d77f3a6a1164e837d3a245ee52cb8938eb1`
- receipt SHA-256: `05643ef48cdfc06300e6065978595c57040f543c7f6d21168bd09fdf724283eb`
- startup-result SHA-256: `5cf4785ef4896bc953e91d045591e2956e485a0b960ab09023f6865ec68a006f`
- replay digest: `ab0ecb95caf30effcc8e31471e8f071bd4aab986a4558cebe5c24dc712056e8f`
- final state digest: `64e55c1bce7e969938bc4f3e64561604b7d54a38f4cad55bf810b2a8ca4d0b39`
- final trace digest: `b7014259e39ca1e3b561f3a04c2c64ea78d12d6d9887caa9a624010eff0b3a6b`
- final generation/revision: 1/29
- records verified: 28
- receipt fields: 8
- replay schema: 1
- production bundle digest: `0d7ffbe33ae79ee590e0b0eb66d37a02482906f6b6c5b7218b067e293a14d2e5`

## Changed Paths

1. `Data/CardDB/Production/CSNCrashIn/cards.json` - production Sable definition.
2. `Data/CardDB/ProductionCardDB.schema.json` - generic observer/stat-delta schema.
3. `Data/Replay/CSNCrashInFixture/match_spec.json` - current production bundle digest.
4. `Data/Replay/CSNRookFixture/match_spec.json` - current production bundle digest.
5. `Data/Replay/CSNSableFixture/match_spec.json` - isolated Sable smoke fixture.
6. `Data/Replay/CSNUndertowArchivistFixture/match_spec.json` - current production bundle digest.
7. `Docs/CSN_Sable_Production_Transfer_Audit.json` - machine-readable audit.
8. `Docs/CSN_Sable_Production_Transfer_Audit.md` - this audit.
9. `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp` - production parsing/validation/digesting.
10. `Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp` - fixture parsing/validation.
11. `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp` - repository validation.
12. `Source/WandboundCore/Private/WBDeathResolution.cpp` - event-time observer capture.
13. `Source/WandboundCore/Private/WBPostDestructionTrigger.cpp` - generic observer collection/resolution.
14. `Source/WandboundCore/Private/WBProductionMatchReplay.cpp` - private snapshot replay state.
15. `Source/WandboundCore/Private/WBReplayTrace.cpp` - stat trace serialization.
16. `Source/WandboundCore/Private/WBUnitStatDelta.cpp` - transactional stat mutation.
17. `Source/WandboundCore/Public/WBCardDefinition.h` - generic trigger/payload types.
18. `Source/WandboundCore/Public/WBGameStateData.h` - observer source snapshot/cursor state.
19. `Source/WandboundCore/Public/WBReplayTrace.h` - previous/new ATK trace fields.
20. `Source/WandboundCore/Public/WBUnitStatDelta.h` - stat mutation API.
21. `Source/WandboundRuntime/Private/WBProductionCSNCrashInSmoke.cpp` - Sable production smoke.
22. `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp` - Sable smoke dispatch.
23. `Source/WandboundRuntime/Public/WBProductionCSNCrashInSmoke.h` - Sable smoke contract.
24. `Source/WandboundRuntime/WandboundRuntime.Build.cs` - package fixture staging dependency.
25. `Source/WandboundTests/Private/WBCSNCrashInTests.cpp` - real replacement boundary regression.
26. `Source/WandboundTests/Private/WBCSNSableTests.cpp` - focused Sable matrix.

## Preserved Files And Limitations

Pre-existing untracked `Content/Maps/NewProjectTest.umap`, `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`, and root file `h origin main` remain untouched. `Content/MeshyImports/**`, `.gitattributes`, `Config/DefaultEngine.ini`, `Config/DefaultEditor.ini`, Godot reference files, maps, assets, and Blueprints were not changed.

Destruction prevention remains an event-boundary contract; Juno is not implemented. No Sable-specific UI, presentation, networking, save/load, or additional CSN card transfer was added. Persistent deltas live for the current unit's board lifetime and intentionally skip a source removed before queued resolution.
