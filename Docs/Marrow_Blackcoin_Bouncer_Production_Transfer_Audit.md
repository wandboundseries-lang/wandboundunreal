# Marrow Blackcoin Bouncer Production Transfer Audit

## Baseline

- Repository baseline: `13259e6dc484ada002e2c0c510aa50006e1211ec` (`Add production CSN Patch sacrifice summon`).
- `HEAD` and `origin/main` matched before implementation.
- Baseline Wandbound automation: 2,369 succeeded, 0 failed, 0 warnings, 0 not run.
- Replay schema remains 1. Public replay receipts remain exactly eight fields.
- Known unrelated untracked files were preserved: `Content/Maps/NewProjectTest.umap`, `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`, and `h origin main`.

## Canon And Godot Audit

The read-only Godot source defines `char_marrow_blackcoin_bouncer` in `scripts/data/CardDB/characters.json` with HP 15, ATK 2, AR 2, RL 3, Marrow Syndicate faction, and the passive `when_attacked_coin_redirect_or_take_extra_damage`. The audited rules functions were `pending_attack_coin_redirect_passive` and `apply_pending_attack_damage_coin_redirect`; the orchestration functions were `_pending_attack_coin_redirect_passive`, `_resolve_pending_attack_coin_redirect_result`, `_maybe_apply_pending_attack_damage_coin_redirect_with_visual`, and `_advance_pending_attack`.

Godot resolves the coin after the ordinary PreHit response has closed and before hit damage resolves. Heads replaces the pending target with the attacker. Tails adds the configured bonus to the same pending battle hit. The owner supersession changes this passive to once per turn per exact source unit. Final production text is: "Once per turn, when this unit is attacked, flip a coin. If heads, the attacker takes the damage instead. If tails, this unit takes that damage +3."

## Production Design

- `FWBPreDamageAttackTriggerDefinition` is generic, typed definition data. It declares source role, timing, mandatory/once-per-turn policy, random branch kind, and branch modifier operations.
- `AutomaticPreDamageModifiers` is a resumable attack-continuation stage after PreHit and before CalculateDamage. A processed bit prevents duplicate resolution.
- The resolver re-reads the current defender at the checkpoint. A replaced, removed, defeated, prevented, off-board, Negated, Stunned, or Frozen source does not draw randomness or consume usage.
- Usage keys contain exact source unit ID, trigger ID, and turn number. Multiple copies do not share usage; the same source becomes eligible on the next turn.
- The coordinator owns the authoritative random state. `WBDeterministicRandom` extracts the established seeded LCG and advances it exactly once per resolved coin branch. No wall-clock, platform, UI, CardId, or player-selected outcome authority was added.
- Only the public Heads/Tails result and source unit are traced. Seed, raw random value, counter/state, and private digests are absent from public receipt data.

## Branch Semantics

Heads transforms the current pending battle hit recipient to the original attacker without declaring another attack, spending another attack, checking self-target geometry, opening another PreHit window, or recursively collecting an attacked trigger. `OriginalDefenderUnitId` remains the Blackcoin source; current defender, calculated hit unit, and final damage recipient become the original attacker. The original attacker remains the AfterDamage attacker role. Counter is suppressed for the reflected hit, and normal battle-damage armor, zero-HP destruction, Hero loss, and destruction observers remain authoritative.

Tails leaves Blackcoin as current defender and adds exactly 3 to raw attack damage before armor. It is one battle-damage event, not separate effect damage. Frozen handling, prevention, HP substitution, armor, AfterDamage, and death resolution continue through the shared pipeline.

Body Double revalidates against the calculated hit unit: a substitution protecting Blackcoin does not catch Heads damage to the attacker, while it remains applicable to Tails damage against Blackcoin. Crash-In or another PreHit transformation controls current-defender authority: replacing Blackcoin suppresses its trigger without spending usage; making an eligible Blackcoin current defender permits it. Player attacks, counters, and NPC attacks all use the same continuation stage.

## Definition And Data Authority

- Production data adds the real `char_marrow_blackcoin_bouncer` definition with canonical stats, faction, text, and generic trigger metadata.
- An alternate identity carrying the same metadata has identical behavior; a Blackcoin-like identity without metadata has none.
- No Blackcoin/CardId/name branch exists in changed WandboundCore or production CardDB behavior source.
- Production bundle digest: `8ee7e358c940121a115629b59c4a185216b9a3595ff8ac4510599640c9f73558`.
- Pinned CSN replay fixture digests were updated from the recomputed production database snapshot.

## Replay And Packaged Validation

The isolated production fixture uses seed 42046 and the real Blackcoin card. Its accepted actions cover a Heads attack, a same-turn second attack with no second coin, turn cycling, and a Tails attack. Fresh replay is performed by the production smoke and must match state and trace digests before the receipt is written.

Two packaged runs exited 0 and were byte-identical:

- Archive SHA-256: `a82348c03ec96839dd3464cdaaf8dfafd650022f1c60dcb21fb7818299454873`.
- Receipt SHA-256: `c116bc2f8fa3fcd26b4d983d2cf0458bc1ae3f07023f1be8d1903737ed93d514`.
- Startup JSON SHA-256: `6d59f8b2e7c3c15b6d911c1df557b59a78f5f7597872354082b66a9ce507d030`.
- Replay digest: `81545ec7b4a890e4f9bd5d99ef74db5b2ae8c0a9ceed923d3dbb902a513006b2`.
- Final state digest: `bcb2212de6df7a04910163441bc72e87d21cbe4cd476645d0c8941eed7a9d1bb`.
- Final trace digest: `c12a4fe0dadb3c505ac766f165852ec770168e049529827c2d9c1e73380a48b7`.
- Final generation/revision: 1/11; accepted records: 10.
- Receipt fields: 8. Privacy scan found no random seed/state/counter or protected state/trace digest.
- No packaged `WandboundUE.exe` process remained after validation.

## Tests And Builds

- Focused Blackcoin automation: 13 succeeded, 0 failed.
- CSN regression group: 85 succeeded, 0 failed.
- Full Wandbound automation: 2,382 succeeded, 0 failed, 0 warnings, 0 not run. Net increase: 13.
- Editor non-unity: succeeded, 121 actions, 157.98 seconds.
- Game non-unity: succeeded, 279 actions, 498.58 seconds.
- Default Editor: succeeded, 5 actions, 33.42 seconds.
- Default Game: succeeded, 9 actions, 169.42 seconds.
- Forced-unity Editor final-source rebuild: succeeded, 211 actions, 627.58 seconds.
- Forced-unity Game final-source rebuild: succeeded, 186 actions, 466.09 seconds.
- A prior four-action parallel Editor attempt hit compiler virtual-memory errors C3859/C1076; the required serial non-unity build passed with no source error.
- Clean BuildCookRun: succeeded. Build 685.43 seconds; cook completed with 0 errors and 0 warnings; total 825.56 seconds.
- `git diff --check`: no whitespace errors; only working-copy line-ending notices.

## Compatibility Boundaries

- Replay schema remains 1.
- Receipt remains eight fields.
- `WBActionCodec.h` Git blob SHA-1 remains `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- `WBActionCodec.cpp` Git blob SHA-1 remains `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.
- No Godot, Blueprint, map, `.uasset`, Meshy, networking, AI, or unrelated card transfer was performed.
- The legacy Godot Backfill branch remains deferred and untouched.

## Changed Paths

Production data and fixtures:

- `Data/CardDB/Production/CSNCrashIn/cards.json`
- `Data/CardDB/ProductionCardDB.schema.json`
- `Data/Replay/CSNCrashInFixture/match_spec.json`
- `Data/Replay/CSNPatchFixture/match_spec.json`
- `Data/Replay/CSNRookFixture/match_spec.json`
- `Data/Replay/CSNSableFixture/match_spec.json`
- `Data/Replay/CSNUndertowArchivistFixture/match_spec.json`
- `Data/Replay/CSNVexFixture/match_spec.json`
- `Data/Replay/MarrowBlackcoinBouncerFixture/match_spec.json`

Core/CardDB production source:

- `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`
- `Source/WandboundCore/Public/WBCardDefinition.h`
- `Source/WandboundCore/Public/WBDeterministicRandom.h`
- `Source/WandboundCore/Public/WBGameStateData.h`
- `Source/WandboundCore/Public/WBMatchCoordinator.h`
- `Source/WandboundCore/Public/WBPreDamageAttackTrigger.h`
- `Source/WandboundCore/Public/WBReplayTrace.h`
- `Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp`
- `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp`
- `Source/WandboundCore/Private/WBDeterministicRandom.cpp`
- `Source/WandboundCore/Private/WBEffectRunner.cpp`
- `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
- `Source/WandboundCore/Private/WBPreDamageAttackTrigger.cpp`
- `Source/WandboundCore/Private/WBProductionMatchReplay.cpp`
- `Source/WandboundCore/Private/WBReplayTrace.cpp`
- `Source/WandboundCore/Private/WBRules.cpp`

Runtime/tests/docs:

- `Source/WandboundRuntime/Public/WBProductionMarrowBlackcoinBouncerSmoke.h`
- `Source/WandboundRuntime/Private/WBProductionMarrowBlackcoinBouncerSmoke.cpp`
- `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`
- `Source/WandboundRuntime/WandboundRuntime.Build.cs`
- `Source/WandboundTests/Private/WBMarrowBlackcoinBouncerTests.cpp`
- `Docs/Marrow_Blackcoin_Bouncer_Production_Transfer_Audit.md`
- `Docs/Marrow_Blackcoin_Bouncer_Production_Transfer_Audit.json`

## Result

The production Blackcoin transfer, generic pre-damage trigger infrastructure, deterministic replay, shared combat composition, and packaged validation are complete. No known correctness or privacy blocker remains for this scoped transfer.
