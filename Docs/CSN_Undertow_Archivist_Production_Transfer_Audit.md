# CSN Undertow Archivist Production Transfer Audit

## Baseline

- Repository baseline: `0e01cb19905cd1241c7e32611b152faa3dcdfa95` (`Add production CSN Crash-In and inheritance`).
- `HEAD` and `origin/main` matched before implementation and still match.
- Initial full automation baseline: 2,305 succeeded, 0 failed, 0 warnings, 0 not run.
- Replay schema: 1.
- No tracked or staged files were dirty at baseline.
- Unrelated untracked assets present at baseline and preserved untouched:
  - `Content/Maps/NewProjectTest.umap`
  - `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`

## Canonical Godot Reference

The read-only Godot CardDB defines `char_csn_undertow_archivist` as the CSN Character **CSN Undertow Archivist** with HP 11, ATK 2, AR 3, and RL 2. Its `csn_inheritance` passive has `draw_on_inheritance: 1`.

The audited Godot rules first apply CSN Inheritance, including the source Current RL snapshot and Wand transfer, then query the newly summoned unit's usable passives. A successful inheritance event draws the configured count through the ordinary draw path and records `csn_inheritance` as the source. The draw remains valid when the source contributes zero RL and zero Wands because the trigger is tied to the successful inheritance event, not a numeric change.

The Unreal port preserves behavior without copying the Godot dispatcher architecture. `Reference/GodotProject/` and `Reference/GodotCanon/` were read only and unchanged.

## Generic Trigger Architecture

`FWBCardDefinition` now supports deterministic `AfterCSNInheritanceTriggers`. Each definition carries a stable trigger ID, mandatory flag, and positive draw count. Fixture and production parsers reject unknown fields, optional triggers, invalid counts, empty IDs, and duplicate IDs. Trigger definitions participate in the production content digest.

`WBCSNInheritanceTrigger::ResolveAfterSuccessfulInheritance` receives a generic event context containing the inheriting unit/player, original source unit, snapshotted source Current RL, inherited Wand count, and stable replacement transaction ID. It:

1. validates the event and live inheriting unit;
2. suppresses personal passive execution for Stunned, Frozen, Negated, or terminal state;
3. resolves definitions in stable trigger-ID order;
4. draws through `WBCardLifecycle::DrawCards` on a working copy;
5. commits only after every mandatory trigger succeeds;
6. emits count-only `csn_inheritance_triggered`, `csn_inheritance_card_drawn`, and `csn_inheritance_trigger_resolved` traces.

No player action, activation, reaction window, replay schema field, or action codec entry was added.

## Replacement Transaction Integration

The existing `WBUnitReplacementEffect` remains the only production replacement and inheritance transaction. After the existing atomic replacement, Wand transfer, RL inheritance/reconciliation, overflow processing, pending-attack redirect, and terminal determination, it invokes the generic trigger resolver on the same outer working copy.

Consequences:

- a normal summon does not invoke the inheritance trigger;
- one successful inheritance event invokes the passive once, regardless of RL points or Wand count;
- failed replacement, stale continuation, failed draw, or empty deck rolls back the entire working-copy transaction;
- Undertow's Stunned/Frozen/Negated state suppresses only its personal draw, not inheritance already applied by the source effect;
- terminal Hero resolution suppresses post-game draw;
- the pending attack is redirected and resumed without another declaration.

## Production CardDB

The actual production definition is integrated into `Data/CardDB/Production/CSNCrashIn/cards.json`, which is the existing production bundle owning Crash-In's source/replacement definition set. Keeping the definition in that suite preserves the current manifest provenance boundary and avoids either broadening production loader trust or duplicating card definitions.

Definition:

- ID: `char_csn_undertow_archivist`
- name: `CSN Undertow Archivist`
- category/faction: Character / CSN
- stats: HP 11, ATK 2, AR 3, RL 2
- passive: mandatory `AfterCSNInheritance`, draw 1
- production suite digest: `e6592207e841eec56063020bf9d67b1c5f57e5bcd4f90c732921dc0449d24e4e`

The existing Crash-In fixture's expected bundle digest was updated to the new deterministic suite digest. A separate Undertow match fixture exercises the real production Crash-In and Undertow definitions. A definition-only fixture proves an alternate CardId with equivalent trigger data works and an Undertow-like CardId without trigger data does not draw.

## Draw, Privacy, and Replay

The draw uses exact Deck-to-Hand movement through `WBCardLifecycle::DrawCards`; direct Hand mutation was not added. The deterministic top card instance is preserved. Empty-deck behavior remains `deck_empty` and is transactionally atomic.

Opponent observation and serialized trace tests confirm that the drawn definition and instance IDs are not exposed. The packaged receipt remains exactly eight fields and contains no state digest, trace digest, continuation ID, or private drawn-card identity. Replay schema remains 1. Fresh coordinator replay matches state digest, trace digest, generation, revision, and accepted record count.

## Focused Automation

16 focused `Wandbound.CSNUndertowArchivist.*` tests succeeded with 0 failures and 0 warnings. Coverage includes:

- production definition, stats, faction, and mandatory trigger;
- alternate-ID behavior and name-only nonbehavior;
- normal summon without draw;
- zero-RL/zero-Wand inheritance;
- multiple Wands and RL with exactly one draw;
- exact Wand transfer and RL/RLUsed reconciliation;
- Stunned/Frozen/Negated passive suppression;
- missing replacement, stale continuation, empty deck, and malformed trigger failure;
- terminal Hero boundary;
- private observation and trace projection;
- deterministic state/trace output;
- real production Crash-In plus Undertow integration;
- real production Crash-In negation with no replacement, inheritance, or draw;
- packaged-smoke logic, fresh replay, schema, codec, and no-CardId-authority guards.

Existing `Wandbound.CSNCrashIn.*` regression: 8 succeeded, 0 failed.

Final full automation report: `Saved/AutomationReports/WandboundUndertowFullFinal2/index.json`.

- 2,321 succeeded
- 0 failed
- 0 warnings
- 0 not run

## Build Matrix

All required targets succeeded:

| Configuration | Result | Elapsed |
| --- | --- | ---: |
| Editor Development non-unity | Succeeded | 60.76 s finalized; 184.28 s initial cold validation |
| Game Development non-unity | Succeeded | 112.04 s finalized; 3,354.19 s initial cold validation |
| Editor Development default | Succeeded | 9.03 s |
| Game Development default | Succeeded | 39.99 s |
| Editor Development forced unity, adaptive unity disabled | Succeeded | 19.14 s |
| Game Development forced unity, adaptive unity disabled | Succeeded | 43.71 s |

Clean BuildCookRun also succeeded against the finalized source. A true clean rebuilt both Editor and Game in 371 actions, cooked 532 packages, staged, packaged, and archived successfully. AutomationTool exited 0 after 512.54 seconds (8m33s wall time reported by the wrapper). The cook reported no errors or warnings.

## Packaged Production Smoke

The freshly archived inner executable was run twice with the explicit local-play map, `WandboundUE` bootstrap argument, and package-relative paths:

- `Data/CardDB/Production/CSNCrashIn/root_manifest.json`
- `Data/Replay/CSNUndertowArchivistFixture/match_spec.json`
- `-WandboundProductionCSNUndertowArchivistSmoke`

Both runs self-terminated with process exit code 0 and logged `RequestExitWithStatus(0, 0, WandboundProductionCSNUndertowArchivistSmoke)`. They used production providers, coordinator legal actions, nested response resolution, real Crash-In replacement, inheritance, exact private draw, attack redirect/damage continuation, replay persistence, and fresh replay.

Both runs were byte-identical:

- archive SHA-256: `78fbe8258a8a066051542205d94be36948ef00efb4e0c294816e7e2eb666105d`
- receipt SHA-256: `eb807958e77d9f93c9244fc439cb8dd5adab1d0b05d111a14ac46d7e5fddb3f0`
- startup-result SHA-256: `0c82352c86f6b30b51ed882546e7017a9dac752242a6e03bd2ea6c0738afda5b`
- replay digest: `b5281cda833277c187c42e71021dd535127e6315cdec6e265aff7e7a44097eaf`
- final state digest: `83ea25c25e9d0b6379da3b7f057e7896a2a9f076d174e7768e6c120130b1262a`
- final trace digest: `d4c0ef61318d3b01eef6cf3a78c3c5552baeb24f727c34892cb04b2bbd921c83`
- final generation/revision: 1/11
- records verified: 10
- receipt fields: 8
- private drawn-card string occurrences in archive/receipt: 0/0

## WBActionCodec Guard

`WBActionCodec` is byte-identical to baseline and has no working-tree diff:

- `Source/WandboundCore/Public/WBActionCodec.h`: `44ef87156beb5799066c2a5ecbc98f04928d98c0`
- `Source/WandboundCore/Private/WBActionCodec.cpp`: `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`

## Changed Paths

- `Data/CardDB/Production/CSNCrashIn/cards.json`: real Undertow definition and smoke-only private draw card.
- `Data/CardDB/ProductionCardDB.schema.json`: typed inheritance-trigger schema.
- `Data/Replay/CSNCrashInFixture/match_spec.json`: updated production bundle digest.
- `Data/Replay/CSNUndertowArchivistFixture/definitions.json`: alternate-ID/name-only fixtures.
- `Data/Replay/CSNUndertowArchivistFixture/match_spec.json`: production Undertow packaged scenario.
- `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`: production trigger parsing, validation, sorting, and digesting.
- `Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp`: fixture trigger parsing and validation.
- `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp`: repository trigger validation and duplicate rejection.
- `Source/WandboundCore/Private/WBCSNInheritanceTrigger.cpp`: generic mandatory trigger resolver.
- `Source/WandboundCore/Private/WBUnitReplacementEffect.cpp`: invokes trigger after successful inheritance inside the existing transaction.
- `Source/WandboundCore/Public/WBCardDefinition.h`: generic trigger definition field.
- `Source/WandboundCore/Public/WBCSNInheritanceTrigger.h`: event context and resolver contract.
- `Source/WandboundRuntime/Private/WBProductionCSNCrashInSmoke.cpp`: shared production Crash-In scenario with isolated Undertow assertions.
- `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`: isolated packaged Undertow smoke flag.
- `Source/WandboundRuntime/Public/WBProductionCSNCrashInSmoke.h`: Undertow smoke entry point and result path.
- `Source/WandboundRuntime/WandboundRuntime.Build.cs`: stages the Undertow match fixture.
- `Source/WandboundTests/Private/WBCSNUndertowArchivistTests.cpp`: focused production, semantic, privacy, replay, and authority coverage.
- `Docs/CSN_Undertow_Archivist_Production_Transfer_Audit.md`: this audit.
- `Docs/CSN_Undertow_Archivist_Production_Transfer_Audit.json`: machine-readable audit.

No Config, map, Meshy, Blueprint, `.uasset`, `.umap`, Godot, action codec, or replay schema file was changed.

## Readiness

The production Undertow definition and generic inheritance trigger are integrated, deterministic, fail closed, private, replay-safe, fully tested, clean-built in every requested mode, and validated twice from the packaged runtime.
