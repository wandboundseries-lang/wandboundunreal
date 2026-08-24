# CSN Vex Production Transfer Audit

## Baseline

- `HEAD`: `29df1879233444fc68fad07b069af8235adbed94`
- `origin/main`: `29df1879233444fc68fad07b069af8235adbed94`
- Baseline tests: 2,342 succeeded, 0 failed, 0 warnings, 0 not run.
- Replay schema: 1.
- Previous production bundle digest: `0d7ffbe33ae79ee590e0b0eb66d37a02482906f6b6c5b7218b067e293a14d2e5`.
- The worktree began with no tracked changes. The unrelated untracked map, Meshy material, and `h origin main` file were preserved.

## Canonical Definition

Production `char_csn_vex` is a CSN Character named CSN Vex with HP 13, ATK 3, AR 4, and RL 2. Movement remains orthogonal adjacent. Attack geometry remains orthogonal line with current range derived from AR.

Public rules text:

> CSN Inheritance: If this unit is summoned by a CSN unit's effect, transfer that unit's equipped Wands to this unit. Increase this unit's Base RL by that unit's Current RL.
>
> Enemy units within this unit's AR get -1 AR.

## Godot Reference Audit

Read-only source: `Reference/GodotProject/godotcanon/scripts/data/CardDB/characters.json` and the corresponding read-only simulation rules.

Godot defines `char_csn_vex` with HP 13, ATK 3, AR 4, RL 2 and passive payload `aura_enemy_stat_penalty_in_range`, `stat=ar`, `amount=-1`, `range_stat=ar`.

The transferred semantics are definition-driven: different-controller targets only; exact source-unit stacking; continuous derived AR; floor zero; source must be active on board and pass `WBCharacterPassiveEligibility`; Stunned, Frozen, and Negated suppress the aura; enemy-effect immunity rejects it; strict attack-line geometry; walls and intervening units block; diagonal geometry is available only through the generic diagonal-attack capability; source range uses a non-recursive pre-aura AR layer.

No Godot file was changed, imported, compiled, or used at runtime.

## Generic Continuous Aura Schema

`FWBCardDefinition::ContinuousStatAuras` owns typed `FWBContinuousStatAuraDefinition` entries. The production schema parses and validates:

- stable aura ID;
- target relation;
- target stat;
- operation and amount;
- range stat;
- geometry;
- wall/unit blocking flags;
- minimum result.

Vex data is `enemy`, `ar`, `add`, `-1`, source `ar`, `attack_line`, both blockers enabled, minimum zero. Unsupported enum values or semantic combinations fail closed in production CardDB validation. Canonical snapshots and bundle digests include the aura data.

## Raw AR and Effective AR

`FWBUnitState::AR` remains raw/stored current AR. It is initialized by summon, replacement, Hero setup, Hybrid setup, NPC creation, fixture loading, and replay state. It is serialized into authoritative state digests and is never mutated by the aura.

`WBUnitStatQuery::GetEffectiveAR` computes:

`max(0, stored AR + all qualifying continuous AR modifiers)`

The query recomputes from state and definitions. Sources are sorted by UnitId and each source's aura entries by AuraId, so unit-container iteration cannot change the result. Each source unit contributes independently; copies are not collapsed by CardId or aura ID.

`GetAuraRangeAR` is the explicit recursion boundary. It reads the source's clamped stored AR before the range-dependent enemy-aura layer. Opposing Vex sources therefore cannot recurse, oscillate, or redefine each other's radii.

## AR Consumer Inventory

| Consumer | Classification | Final authority |
|---|---|---|
| Production CardDB `stats.ar`, validation, canonical snapshot, bundle digest | Printed/data AR | Stored definition value |
| Initial Hero, summon, Deck summon, Hybrid summon, replacement, NPC spawn | Stored runtime initialization | Copy printed AR into `FWBUnitState::AR` |
| Replay state digest and raw game-state serialization | Stored runtime AR | Serialize raw `Unit.AR`; do not serialize derived aura deltas |
| `WBUnitStatQuery::GetAuraRangeAR` | Aura range layer | Clamped stored source AR, intentionally excluding enemy range auras |
| `WBUnitStatQuery::GetEffectiveAR` | Effective gameplay AR | Raw AR plus deterministic qualifying continuous modifiers, floor zero |
| Normal player attack declaration | Effective gameplay AR | Repository-aware `WBRules::CanDeclareAttack` |
| Player legal attack generation | Effective gameplay AR | Repository-aware generation delegates to the same declaration authority |
| Pending-attack redirect revalidation | Effective gameplay AR | Repository-aware `CanRedirectPendingAttack` |
| Counter range eligibility | Effective gameplay AR | Repository-aware `CanResolveCounterattack` |
| NPC attack declaration and target selection | Effective gameplay AR | Repository-aware NPC rules and NPC phase resolution |
| Coordinator action application and continuation | Effective gameplay AR | Repository forwarded through coordinator to rules/effects |
| Production public board summary | Public derived AR | Repository-aware summary exposes effective AR |
| Compatibility overloads without a repository | Legacy stored AR | Deliberately retain old raw behavior for isolated legacy callers/tests |
| Runtime presentation model | Public presentation AR | Receives the public summary value; does not recalculate rules |
| Test builders and fixture readers | Test setup/stored AR | Initialize explicit raw values; focused tests query effective AR separately |

No persistent `WBUnitStatDelta` is emitted for Vex. No synthetic apply/remove aura trace is produced by a query.

## Geometry and Eligibility

Aura reach reuses board bounds, orthogonal/diagonal line distance, wall edges, and strict intervening-unit checks. The target itself is not an intervening blocker. Either controller's unit can block the line. Production Vex is orthogonal because its definition has no diagonal capability; a semantic alternate source with `AttacksDiagonally` proves the generic diagonal path.

The target relation is controller inequality, not CardId, faction, or public name. Neutral/NPC units use the existing owner relationship and are enemies of player-controlled sources. `ImmuneToEnemyEffects` is a generic combat capability and prevents the penalty; removing it restores the derived penalty.

## Combat Integration

Repository-aware rule/effect overloads carry effective AR through normal declaration, generated attacks, redirect revalidation, counters, and NPC selection/declaration. The coordinator supplies its authoritative repository for production calls. Accepted pending attacks are not retroactively revalidated at a new Vex-specific checkpoint.

The public summary uses effective AR when a production repository is available. Raw AR remains in authoritative state/replay, avoiding redundant derived serialization.

## CSN Composition

- Rook -> Vex: an exact private Deck instance remains a mandatory player choice, is summoned to Rook's tile, receives the exact Wand and Current-RL inheritance, then becomes an active aura source.
- Crash-In -> Vex: an exact Hand instance replaces the defender, receives the exact Wand and Current-RL inheritance, becomes the redirected defender, and affects the attacker only after installation. Existing continuation authority remains unchanged.
- Existing Sable destruction-observer behavior remains generic and unchanged. CSN regressions all pass.

No Vex-specific inheritance branch exists.

## Definition-Driven Authority

Gameplay Core and CardDB semantic code contain no `char_csn_vex`, `CSN Vex`, or Vex-name branch. Those strings appear only in production data, tests, smoke diagnostics, and this audit. An alternate CardId/public name with the same aura definition behaves identically; a Vex-like identity without aura metadata provides no aura.

## Production Data and Digest

Vex was added to `Data/CardDB/Production/CSNCrashIn/cards.json`. The production loader computed the new bundle digest:

`152a869dd25a7d1d7e839ff9e2aec47ebbca31d164f42bc567bd19f6324a7c73`

The existing Crash-In, Rook, Sable, and Undertow Archivist replay fixtures were updated to pin that computed digest. The isolated Vex fixture is `Data/Replay/CSNVexFixture/match_spec.json`.

## Replay and Privacy

- Replay schema remains 1.
- The packaged smoke persisted and reloaded its archive before fresh replay verification.
- Final generation/revision: 1/14.
- Accepted records: 13.
- Final state digest: `f78b95e4e2567aaaa2e3fcd5bc55d131f38f838a637753756103dcb56a5d0690`.
- Authoritative final trace digest (`footer.final_trace_digest`): `69bb6305a2f70376d02ab34950760c531afa79f5ed564bb9ebfb39fdf5df0747`.
- Last accepted action record trace interval digest (`records[12].trace_digest`): `610713f1709e5e5b55bd4652bf4f0777ac6c43d8a731d534e1167dd6733a421a`.
- Receipt fields: exactly 8.
- Receipt excludes `char_csn_vex`, state digest, and trace digest.
- Effective AR is recomputed from replayed board/definition state; no redundant aura state is serialized.

`WBActionCodec` is unchanged:

- `Source/WandboundCore/Public/WBActionCodec.h`: `44ef87156beb5799066c2a5ecbc98f04928d98c0`
- `Source/WandboundCore/Private/WBActionCodec.cpp`: `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`

## Validation

Focused Vex: 14 succeeded, 0 failed, 0 warnings, 0 not run.

CSN regression: 72 succeeded, 0 failed, 0 warnings, 0 not run.

Full Wandbound automation: 2,356 succeeded, 0 failed, 0 warnings, 0 not run. Baseline was 2,342; net increase is 14.

Six-build matrix, exact final source:

1. Editor Development non-unity: succeeded.
2. Game Development non-unity: succeeded.
3. Default Editor Development: succeeded.
4. Default Game Development: succeeded.
5. Forced-unity Editor Development (`-ForceUnity -DisableAdaptiveUnity`): succeeded; Core/Runtime unity units compiled.
6. Forced-unity Game Development (`-ForceUnity -DisableAdaptiveUnity`): succeeded; Core/Runtime unity units compiled.

Clean BuildCookRun: succeeded in 629.06 seconds. Cook, stage, package, and archive completed.

Packaged Vex smoke, using inner executable and package-relative data paths:

- Run 1 exit: 0.
- Run 2 exit: 0.
- Archive SHA-256, both runs: `1a0c59f3956ae3b34174bca81e7dfd0f03a9dba864b396d30127bf0506580ee6`.
- Receipt SHA-256, both runs: `26e91cc7444350aad67639bd357d003e52ca68aa75eb05c0d5716abc199c113a`.
- Replay digest: `7632027d4da97cd5555e495287111989eaaf232dbd2bb3822acb3cdd2c7ce5d1`.
- Startup JSON SHA-256: `9baa3b7da1cb6b46a066d45186eb21e0b8b0058224db71196a2f59cd840bba68`.
- Archives and receipts are byte-identical.
- Both stderr logs are empty.
- No packaged `WandboundUE` process remained.

The smoke uses production providers and coordinator action submission to summon Vex, move it through legal actions, pass reaction windows, advance turns, prove the raw-range attack action is excluded by effective AR, persist replay, and verify a fresh replay. A copy-only diagnostic also verifies outside/inside aura, wall add/remove, passive suppression/restoration, destruction, and raw-AR immutability.

## Changed Path Ownership

Task paths are limited to production data/fixtures, generic Core/CardDB authority, production smoke/runtime staging, focused CSN tests, and these two audits. No asset, Config, Godot, Meshy, Blueprint, map, action-codec, or replay-schema path was changed.

Exact task paths (29):

1. `Data/CardDB/Production/CSNCrashIn/cards.json`
2. `Data/Replay/CSNCrashInFixture/match_spec.json`
3. `Data/Replay/CSNRookFixture/match_spec.json`
4. `Data/Replay/CSNSableFixture/match_spec.json`
5. `Data/Replay/CSNUndertowArchivistFixture/match_spec.json`
6. `Data/Replay/CSNVexFixture/match_spec.json`
7. `Docs/CSN_Vex_Production_Transfer_Audit.md`
8. `Docs/CSN_Vex_Production_Transfer_Audit.json`
9. `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`
10. `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp`
11. `Source/WandboundCore/Private/WBEffectRunner.cpp`
12. `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
13. `Source/WandboundCore/Private/WBNPCPhaseResolution.cpp`
14. `Source/WandboundCore/Private/WBPublicBoardSummary.cpp`
15. `Source/WandboundCore/Private/WBRules.cpp`
16. `Source/WandboundCore/Private/WBUnitStatQuery.cpp`
17. `Source/WandboundCore/Public/WBCardDefinition.h`
18. `Source/WandboundCore/Public/WBEffectRunner.h`
19. `Source/WandboundCore/Public/WBPublicBoardSummary.h`
20. `Source/WandboundCore/Public/WBRules.h`
21. `Source/WandboundCore/Public/WBTypes.h`
22. `Source/WandboundCore/Public/WBUnitStatQuery.h`
23. `Source/WandboundRuntime/Private/WBProductionCSNCrashInSmoke.cpp`
24. `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`
25. `Source/WandboundRuntime/Public/WBProductionCSNCrashInSmoke.h`
26. `Source/WandboundRuntime/WandboundRuntime.Build.cs`
27. `Source/WandboundTests/Private/WBCSNCrashInTests.cpp`
28. `Source/WandboundTests/Private/WBCSNRookTests.cpp`
29. `Source/WandboundTests/Private/WBCSNVexTests.cpp`

Known unrelated untracked paths preserved exactly:

- `Content/Maps/NewProjectTest.umap`
- `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`
- `h origin main`

Nothing is staged. No commit or push was performed.

## Limitations

Only AR is implemented in the effective-stat query in this pass. The typed aura schema is intentionally narrow and fails closed for unsupported stats, operations, range layers, or geometry. No other cards were transferred, and no caching was added.
