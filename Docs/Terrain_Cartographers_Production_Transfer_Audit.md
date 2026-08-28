# Terrain Cartographers Production Transfer Audit

## Baseline

- Repository baseline: `0bbc8d25ed64b3586b1d47ca10c597a161d0a5d1` (`Add production Marrow Blackcoin Bouncer`).
- `HEAD` and `origin/main` matched before implementation.
- Baseline automation: 2,382 succeeded, 0 failed, 0 warnings, 0 not run.
- Replay schema: 1.
- Known unrelated untracked paths were preserved: `Content/Maps/NewProjectTest.umap`, `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`, and `h origin main`.

## Godot Family Definition

The read-only source `Reference/GodotProject/godotcanon/scripts/data/CardDB/characters.json` defines Mire, Emberfault, Tidecall, and Rimecall Cartographer as HP 15, ATK 2, AR 3, RL 1 Characters with a chosen-tile `set_terrain` activation using `range_stat = ar`. Their terrain IDs are `mud`, `lava`, `water`, and `ice`, respectively. No Godot file was modified.

Owner canon supersedes the old ATK value. Every production Cartographer is HP 15, ATK 1, AR 3, RL 1.

## Production Definitions

| Card ID | Public name | Activation | Terrain |
| --- | --- | --- | --- |
| `char_mire_cartographer` | Mire Cartographer | Mud Survey | `mud` |
| `char_emberfault_cartographer` | Emberfault Cartographer | Lava Survey | `lava` |
| `char_tidecall_cartographer` | Tidecall Cartographer | Water Survey | `water` |
| `char_rimecall_cartographer` | Rimecall Cartographer | Ice Survey | `ice` |

All four use the same definition-driven board-source activation shape: normal-turn priority, exact owned source unit, once per turn, public tile target, Manhattan metric, current effective AR, occupied tiles allowed, and no line-of-sight requirement. The definition parser defaults an omitted once-per-turn key to the existing exact-source usage key.

## Activation and Targeting

- The tile is selected and fixed in the activation command before the normal pending-effect response window opens.
- Runtime tile options are generated deterministically in Y/X order from public board coordinates.
- Bounds use the existing board dimensions.
- Range is `ManhattanDistance <= WBUnitStatQuery::GetEffectiveAR`, including distance zero.
- Empty, friendly-occupied, enemy-occupied, NPC-occupied, and source tiles are legal.
- Walls and intervening units are not queried for this payload. Existing attack line-of-sight logic is not reused.
- Vex composition is dynamic: AR 3 reduced to effective AR 2 excludes distance 3 while preserving distance 2; removing the aura restores distance 3.

## Reaction, Negation, and Usage

Cartographer activations use the existing generic reactable effect continuation. Opponent-first response authority, nested reaction handling, pass closure, and negate behavior are unchanged. The chosen tile remains fixed while responses resolve. A negated activation makes no terrain change and emits no `terrain_changed` event. Existing accepted-activation policy consumes exact-source once-per-turn usage even when the effect is later negated. Two exact unit instances maintain independent usage.

## Terrain Authority

`FWBGameStateData::TerrainByTileIndex` remains the only non-default terrain store. `SetTerrainAt` validates the tile, canonicalizes the terrain ID, replaces the single value at that tile, and removes an override when setting the default terrain. No ownership or duration was added.

The generic `SetTerrain` effect captures previous terrain and emits one public `terrain_changed` trace only when the authoritative value changes. A legal same-terrain activation is accepted and consumes usage but emits no fabricated terrain transition. Terrain persists through turn advancement until another effect changes it.

Changing terrain under an occupant does not move or remove it, alter HP or Armor, or apply an invented status or hazard. Existing and future continuous queries read the authoritative terrain map immediately; no Cartographer cache or refresh branch exists.

## Public State and Privacy

The existing public board terrain summary reports changed non-default tiles deterministically and retains one entry per tile. Target tiles and terrain transitions are public. Private deck ordering, hidden candidates, private continuation data, and protected state/trace digests remain absent from the public receipt.

The packaged receipt has exactly eight fields. Privacy scans found none of: `state_digest`, `trace_digest`, `ordered_deck`, `deck_instance`, `hand_instance`, `private`, `continuation_id`, or `target_options`.

## Definition-Driven Boundary

Automation proves an alternate CardId/public name with equivalent activation metadata receives the same behavior, while a Cartographer-like identity without `set_terrain` metadata receives none. Production CardId/name occurrences are limited to card data, tests, fixture/smoke diagnostics, and these audit artifacts. No semantic branch in WandboundCore or WandboundCardDB recognizes a Cartographer identity.

## Production Bundle and Replay

- Previous production bundle digest: `8ee7e358c940121a115629b59c4a185216b9a3595ff8ac4510599640c9f73558`.
- Final production bundle digest: `39afb9fb4089e2c95565ad9d57fffc234443de72a0098228c91625550b4d4534`.
- Existing production replay fixtures that pin this bundle were updated to the computed final digest.
- Replay schema remains 1.
- Packaged replay records: 22.
- Final generation/revision: 1/23.
- Final state digest: `dad14fc8bc0a0cf9c6fe3f9c63bdba52497a7e296502bf12ce3136e4eded03d9`.
- Final trace digest: `d50b600c3386108af47a40ff5bea71ca67cee2cabcb2c066f26580745525f8ae`.
- Final replay digest: `676009f0de6eb71dbc389c44a713c37c5c25d1c73e3390199c97f48cf185c366`.
- Archive SHA-256: `d59cfcb8d61e5eab5047ca4904bca75b51486839cfc8ec24da9392301e73041b`.
- Receipt SHA-256: `d63c73ce2b7a708ce05f011044b678c37a6578144cc661dc2a12a20cdff0424a`.
- Startup JSON SHA-256: `c1589b0b9f442008164c71854ffc1ded9dbf77f74160daef9a4ff550f194d99c`.

Fresh replay reconstructs the same final terrain map, state digest, and trace digest. The two packaged runs produced byte-identical replay, receipt, and startup JSON artifacts.

## WBActionCodec

No codec source changed.

- `Source/WandboundCore/Public/WBActionCodec.h`: `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- `Source/WandboundCore/Private/WBActionCodec.cpp`: `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.

## Tests

Nine focused automation tests cover the four-card table, owner ATK override, Manhattan geometry, all occupancy classes, no LOS through an actual wall and intervening unit, dynamic Vex effective AR, overwrite/no-op/persistence, public summaries, public tile selection, exact-source usage, response/negation, definition-driven behavior, and fresh replay.

Targeted validation:

- Terrain Cartographers: 9 succeeded.
- Public Board Summary: 16 succeeded.
- Runtime Result Board Summary: 8 succeeded.
- CSN aggregate: 85 succeeded.
- CSN Vex: 14 succeeded.
- CSN Patch: 13 succeeded.
- Core Movement: 17 succeeded.
- Marrow Blackcoin Bouncer: 13 succeeded.
- Final full Wandbound automation: 2,391 succeeded, 0 failed, 0 warnings, 0 not run, a net increase of 9.

## Build Matrix

All six final-source build modes now have successful compilation evidence after the packaged wall probe was routed through the production runtime activation bridge. The validation-only remediation cleaned only target-generated outputs and used `MaxParallelActions=2`; no source, CardDB, runtime, test, or build-script file changed.

1. Editor non-unity: 6 actions including the changed smoke, 7.34 seconds.
2. Game non-unity: 3 actions including the changed smoke, 13.46 seconds.
3. Editor default fresh compile: 195 compiler actions out of 214 total actions, succeeded in 309.44 seconds.
4. Game default fresh compile: 186 compiler actions out of 189 total actions, succeeded in 350.88 seconds.
5. Editor forced-unity fresh compile: 195 compiler actions out of 214 total actions, succeeded in 242.48 seconds.
6. Game forced-unity fresh compile: 186 compiler actions out of 189 total actions, succeeded in 303.45 seconds.

The forced-unity commands compiled `Module.WandboundCore.*`, `Module.WandboundRuntime.*`, and other module unity translation units. UBT's Git-based adaptive-unity policy separately compiled dirty final-source files, including `WBProductionTerrainCartographerSmoke.cpp` and `WBTerrainCartographerTests.cpp`; therefore unity compilation occurred and every final task-relevant source was included. The preceding non-unity validation also completed 121 Editor actions and 116 Game actions after the first wall-probe revision. The final clean BuildCookRun rebuilt the complete 403-action graph.

Fresh build commands used the UE 5.7 bundled `dotnet.exe` and `UnrealBuildTool.dll` with the following target arguments. Each build was preceded by the same target arguments plus `-Clean`; forced-unity clean commands also included `-ForceUnity`.

- Editor default: `WandboundUEEditor Win64 Development -Project=<project> -WaitMutex -NoHotReload -MaxParallelActions=2`
- Game default: `WandboundUE Win64 Development -Project=<project> -WaitMutex -NoHotReload -MaxParallelActions=2`
- Editor forced unity: `WandboundUEEditor Win64 Development -Project=<project> -WaitMutex -ForceUnity -NoHotReload -MaxParallelActions=2`
- Game forced unity: `WandboundUE Win64 Development -Project=<project> -WaitMutex -ForceUnity -NoHotReload -MaxParallelActions=2`

The first Editor-default clean attempt inside the restricted validation sandbox exited 6 before compilation because UBT could not rotate its user-profile log (`UnauthorizedAccessException`). The identical target clean/build was rerun with access to UBT's normal user-profile log location and succeeded. This was an execution-environment access failure, not a compiler or source failure.

## BuildCookRun and Packaged Smoke

The earlier first final-source clean BuildCookRun attempt reached a host resource failure after 442.54 seconds: compiler errors C3859/C1076 with Windows code 1455 reported that the paging file was too small for a PCH allocation. No source diagnostic was emitted. The same clean BuildCookRun was rerun with Unreal's supported `MaxParallelActions=2` option and no project/config change. All four remediation builds also used the two-action cap and needed no source workaround.

The capped final clean BuildCookRun succeeded with the complete 403-action graph, a full non-incremental cook, 532 discovered cook packages reaching zero remaining, 459 package-store packages, and exit code 0. Build execution took 29,761.16 seconds; BuildCookRun took 29,911.17 seconds; AutomationTool reported 8 hours 18 minutes 31 seconds.

The isolated packaged smoke uses the real production bundle and all four real Cartographers. It verifies occupied/source tile mutation, Mire mud, Emberfault overwrite to lava, Tidecall water, Rimecall ice, dynamic effective-AR legality, exact-source once-per-turn denial, public terrain, public transition traces, persisted replay, and fresh replay parity. Both validated launches exited 0 with empty stderr. Source and packaged hashes match for both the fixture and production cards data.

The package match-spec schema has no initial-wall authority. Following the existing packaged Vex smoke precedent, the Cartographer smoke copies the coordinator state into a non-authoritative diagnostic probe, places both an intervening unit and wall between the real Mire source and its real generated target action, and executes the resolved handoff through `WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff` with the production repository. That bridge delegates to `WBEffectRunner::ApplyCardActivationCommand`, which validates through the same `WBRules::CanApplyCardActivationCommand` authority used by production. The probe contains no duplicate Cartographer targeting implementation. The authoritative coordinator state and replay are not mutated by the probe. Focused automation directly adds a real wall and calls the same rules authority, while the normal packaged scenario loads the real production Cartographer definitions. This is an acceptable package-fixture coverage limitation, not a commit blocker.

One discarded launcher attempt used separate native arguments, which Windows transformed into `root_manifest .json` and `match_spec .json`; it did not enter the production smoke. Final validation used the established waited inner executable launch with one argument string, the local-play map, production-data mode, and package-relative paths.

## Validation Freshness

All timestamps are UTC and refer to the exact evidence retained for this source tree.

- Last core source edit: `2026-08-27T16:17:38.0908385Z` (`Source/WandboundCore/Private/WBRules.cpp`).
- Last CardDB edit: `2026-08-27T16:07:11.3287950Z` (`Data/CardDB/Production/CSNCrashIn/cards.json`).
- Last runtime edit: `2026-08-28T03:09:06.8202888Z` (`Source/WandboundRuntime/Private/WBProductionTerrainCartographerSmoke.cpp`).
- Last test edit: `2026-08-27T16:45:36.6628970Z` (`Source/WandboundTests/Private/WBProductionActivationExecutionHandoffTests.cpp`).
- Last build-script edit: `2026-08-27T17:39:03.5787017Z` (`Source/WandboundRuntime/WandboundRuntime.Build.cs`).
- Focused Cartographer report: `2026-08-28T03:11:42.5222484Z`.
- Full Wandbound automation report: `2026-08-28T03:13:19.0471512Z`.
- Fresh Editor default compile completed: `2026-08-28T12:19:10.6271880Z`.
- Fresh Game default compile completed: `2026-08-28T12:25:25.6144423Z`.
- Fresh Editor forced-unity compile completed: `2026-08-28T12:29:49.1311940Z`.
- Fresh Game forced-unity compile completed: `2026-08-28T12:35:29.7101495Z`.
- Existing clean BuildCookRun log completed: `2026-08-28T11:40:50.4366422Z`.
- Existing second packaged smoke log completed: `2026-08-28T11:41:54.0158265Z`.
- Build-evidence audit update recorded: `2026-08-28T12:38:26.5804190Z`.

The package and smoke artifacts were generated before the later validation-only target-output rebuilds. SHA-256 comparison of all 36 non-document task files before and after the rebuilds found zero mismatches, and all 36 source timestamps were unchanged, so the package and the four fresh builds validate the same byte-identical source tree. Automation and packaging were not rerun because no implementation, data, test, runtime, or build-script source changed.

## Changed Files

Production data and fixtures:

- `Data/CardDB/Production/CSNCrashIn/cards.json`
- `Data/Replay/CSNCrashInFixture/match_spec.json`
- `Data/Replay/CSNPatchFixture/match_spec.json`
- `Data/Replay/CSNRookFixture/match_spec.json`
- `Data/Replay/CSNSableFixture/match_spec.json`
- `Data/Replay/CSNUndertowArchivistFixture/match_spec.json`
- `Data/Replay/CSNVexFixture/match_spec.json`
- `Data/Replay/MarrowBlackcoinBouncerFixture/match_spec.json`
- `Data/Replay/TerrainCartographerFixture/match_spec.json`

CardDB and core authority:

- `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`
- `Source/WandboundCore/Private/WBCardActivationExpansion.cpp`
- `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp`
- `Source/WandboundCore/Private/WBEffectRunner.cpp`
- `Source/WandboundCore/Private/WBGameStateData.cpp`
- `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
- `Source/WandboundCore/Private/WBReplayTrace.cpp`
- `Source/WandboundCore/Private/WBRules.cpp`
- `Source/WandboundCore/Public/WBCardActivationLegalAction.h`
- `Source/WandboundCore/Public/WBEffectRequest.h`
- `Source/WandboundCore/Public/WBGameStateData.h`
- `Source/WandboundCore/Public/WBReplayTrace.h`
- `Source/WandboundCore/Public/WBRules.h`

Runtime and package smoke:

- `Source/WandboundRuntime/Private/WBProductionActivationDataProvider.cpp`
- `Source/WandboundRuntime/Private/WBProductionActivationExecutionHandoff.cpp`
- `Source/WandboundRuntime/Private/WBProductionActivationTargetSelectionBridge.cpp`
- `Source/WandboundRuntime/Private/WBProductionTerrainCartographerSmoke.cpp`
- `Source/WandboundRuntime/Private/WBRuntimeActivationExecutionBridge.cpp`
- `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`
- `Source/WandboundRuntime/Public/WBProductionActivationTargetSelectionBridge.h`
- `Source/WandboundRuntime/Public/WBProductionTerrainCartographerSmoke.h`
- `Source/WandboundRuntime/Public/WBRuntimeActivationExecutionBridge.h`
- `Source/WandboundRuntime/WandboundRuntime.Build.cs`

Tests and audit:

- `Source/WandboundTests/Private/WBCardDefinitionRepositoryTests.cpp`
- `Source/WandboundTests/Private/WBProductionActivationExecutionHandoffTests.cpp`
- `Source/WandboundTests/Private/WBProductionActivationTargetSelectionBridgeTests.cpp`
- `Source/WandboundTests/Private/WBTerrainCartographerTests.cpp`
- `Docs/Terrain_Cartographers_Production_Transfer_Audit.md`
- `Docs/Terrain_Cartographers_Production_Transfer_Audit.json`

## Scope and Limitations

- No immediate mud, lava, water, or ice hazard behavior was invented.
- Terrain has no owner or duration.
- No other card was transferred.
- No Blueprint, map, asset, Godot, Meshy, config, codec, or replay-schema change was made.
- The current package match-spec cannot author an initial wall; the packaged smoke therefore uses a non-authoritative copied-state rules probe, matching existing production smoke precedent.
