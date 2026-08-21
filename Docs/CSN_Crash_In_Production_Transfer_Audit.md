# CSN Crash-In Production Transfer Audit

## Baseline

- `HEAD` and `origin/main`: `2206dee407d0031c2a2fb3e6d9691bd858defaad` (`Document final repository hygiene state`).
- Initial Wandbound automation: 2,297 succeeded.
- Replay schema: version 1.
- `WBActionCodec.h` blob: `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- `WBActionCodec.cpp` blob: `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.
- Pre-existing unrelated untracked assets: `Content/Maps/NewProjectTest.umap` and `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`.

## Authoritative Behavior Audit

Tracked Unreal canon and the production coordinator remain authoritative. The read-only Godot implementation was consulted for card-specific behavior. It establishes that CSN Crash-In is an RR 2 PreHit reaction when the activating player controls the attacked CSN unit and has an eligible CSN Character in Hand. The player selects an exact replacement, the source's Current RL and equipped Wands are snapshotted, the source is destroyed, the selected Character is summoned to the vacated tile, inheritance is applied, and the same pending attack is redirected.

The bespoke Godot picker, CardId branching, and Crash-In nested-response exemption were not copied. Unreal preserves its newer invariant that every activation is pending and reactable, including nested A -> B -> C resolution. The Godot `csn_effect` suppression of a second ordinary summon response is represented by resolving the replacement inside the existing suspended effect/attack continuation; no extra summon action or response checkpoint is created.

Godot does not expressly exclude a Hero defender. Current Unreal canon makes Hero loss terminal without explicit Hero replacement authority. Crash-In therefore does not promote the summoned Character to Hero: a CSN Hero may satisfy the definition-driven condition, but canonical destruction commits the terminal result. This behavior is explicit in automation.

## Generic Architecture

The implementation adds the generic operation `ReplacePendingAttackDefenderFromHand`, the generic condition `OwnCurrentDefender`, an auxiliary exact-zone-card selection, a Character replacement-kind constraint, and the inheritance policy `TransferEquippedWandsAndAddSourceCurrentRL`. Repository validation rejects malformed combinations. Candidate generation creates deterministic, instance-distinct Hand choices; the coordinator supplies only qualifying owned Character instances using definition faction metadata.

No gameplay authority branches on `effect_react_csn_crash_in`. That identifier appears only in production data, fixture/smoke code, tests, and documentation. A differently named semantic-equivalent definition works, while a Crash-In-like identifier without the generic operation does not.

## Production Card Data

The isolated production bundle at `Data/CardDB/Production/CSNCrashIn/` contains the actual `effect_react_csn_crash_in` definition with public name `CSN Crash-In`, RR 2, PreHit timing, CSN defender/source requirements, exact CSN Character Hand selection, generic replacement operation, inheritance policy, and attack redirect. Its production bundle digest is `69e7036ac6814fb7bcec6c44056d44c980050ab6728f47abe5c2625471b35906`.

The existing production schema was sufficient; `Data/CardDB/ProductionCardDB.schema.json` was not changed. The production parser and repository fail closed on unsupported conditions, choices, replacement kinds, or inheritance policies.

## Transactional Resolution

`WBUnitReplacementEffect` validates the exact PreHit continuation, current defender, ownership and faction, selected Hand instance and definition, tile, unit cap replacement, zones, unit allocation, equipment definitions, and death-prevention result before committing. It operates on a working copy, validates the final zones, and commits only after every dependent mutation succeeds.

Successful resolution:

1. Snapshots the source Current RL and exact equipped Wand instances.
2. Applies canonical destruction/death handling to the source.
3. Removes the exact selected Character instance from Hand.
4. Creates the replacement from printed HP, ATK, AR, and Base RL on the vacated tile.
5. Transfers the original Wand instances without changing instance ID, owner, slot, or deterministic order.
6. Sets replacement Base RL to printed Base RL plus snapshotted source Current RL.
7. Recalculates Current RL and RLUsed through the canonical resonance pipeline and resolves deterministic overflow if required.
8. Redirects the exact pending attack continuation through the generic EffectRunner redirect API.
9. Emits deterministic destruction, replacement summon, Wand transfer, inheritance, redirect, and attack-continuation traces.

Failure leaves the original state unchanged. Replacement at the normal unit cap is legal because the transaction removes one unit before adding one. The attack is not redeclared, the attacker pays no second attack budget, and no movement trace is fabricated.

## Hidden Information and Replay

The authoritative private activation carries the exact selected Hand instance. Acting-player candidate IDs remain deterministic and instance-distinct. Opponent public observation does not expose that selection before resolution. Once successfully summoned, the replacement becomes public normally.

The public receipt remains exactly eight fields and excludes replacement identity, continuation data, state digest, and trace digest. The server replay archive retains the accepted private action needed for deterministic replay; public receipt/startup output does not. Rejected and negated effects preserve generic pending-effect semantics.

Replay schema remains version 1 and `WBActionCodec` is byte-identical. The packaged smoke freshly replays all 10 accepted actions and matches generation, revision, final state digest, and final trace digest.

## Test Results

- Focused `Wandbound.CSN.CrashIn`: 8 succeeded, 0 failed, 0 not run, 0.273 seconds. The tests contain the requested candidate, pending-stack, successful-resolution, atomic-failure, unit-cap, Hero, privacy, and definition-driven assertions.
- Full Wandbound automation: 2,305 succeeded, 0 failed, 0 warnings, 0 not run, 27.564 seconds.
- The exact default commandlet intermittently stopped during unrelated Unreal editor-plugin startup before test discovery. The completed full run used command-line plugin isolation for `TakeRecorder,ChaosCaching`; all 2,305 Wandbound tests executed. A prior exact-command run reached completion with 2,304 passing and one runtime-source guard failure, which was fixed before the final successful run.

## Build Matrix

- Editor Development non-unity: succeeded, 12.53 seconds.
- Game Development non-unity: succeeded, 408.30 seconds.
- Default Editor Development final rebuild: succeeded, 43.23 seconds.
- Default Game Development: succeeded, 67.05 seconds.
- Forced-unity Editor with adaptive unity disabled: succeeded, 130.02 seconds.
- Forced-unity Game with adaptive unity disabled: succeeded, 116.68 seconds.
- Clean BuildCookRun: succeeded; build/cook/stage/package/archive completed in 1,042.91 seconds (17m27s).

An interim forced-unity compile found a translation-unit-local comparator name collision with the existing Hybrid implementation. Renaming it to `CrashInEquippedEntryLess` fixed the collision; both definitive forced-unity targets then passed.

## Packaged Production Smoke

The packaged inner executable used the explicit local-play map, `WandboundUE` bootstrap argument, and package-relative paths:

- `Data/CardDB/Production/CSNCrashIn/root_manifest.json`
- `Data/Replay/CSNCrashInFixture/match_spec.json`
- `-WandboundProductionCSNCrashInSmoke`

The validated PowerShell stop-parsing invocation was required because ordinary Windows native argument marshalling inserted a space before `.json`. Both valid runs loaded the production bundle digest and requested exit status 0. Run 1 exhibited the known post-request packaged wait and was stopped only after the success status and complete artifacts were recorded; run 2 self-terminated.

Both runs were byte-identical:

- Archive SHA-256: `8a5c3ca21d775ab058fd6bf1cad32db9648aff189ea3c3f1337110b97394817e`.
- Receipt SHA-256: `2d0cb65d591ff9f31bfd8e5f0486b83e64ea7f276f7e78a580078589407779b1`.
- Startup-result SHA-256: `504b635089afcf66482ad4f75ad06dddfa8db46461ef758c8e2167d544ea0a01`.
- Replay digest: `ead4e9ddea6f07c14c3052a7c62896a36a8158ecdbdf9c2508561ad3289fdb0d`.
- Final state digest: `4dffbf11e80b9bae3f90b4aabcb4def46a321aafb779dd47a705eaa74c15997b`.
- Final trace digest: `c1c4e16164e4b0582169a6343da046b5f923ffdf1f8a64de31d94ecb7dd824a3`.
- Final generation/revision: 1/11.
- Receipt fields: 8.

The smoke covers normal production providers, coordinator legal actions, exact replacement selection, nested A -> B -> C reactions, destruction, effect summon, Wand inheritance, RL recalculation, exact attack redirect, damage continuation, replay persistence, and fresh replay.

## Changed Files

All implementation files were clean at baseline; all new files were absent at baseline.

- `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`: parses generic defender condition, replacement choice, operation, and inheritance policy.
- `Source/WandboundCore/Public/WBCardDefinition.h`: adds the generic own-current-defender condition.
- `Source/WandboundCore/Public/WBEffectRequest.h`: adds generic replacement operation, auxiliary selection, replacement kind, and inheritance policy.
- `Source/WandboundCore/Public/WBCardActivationCandidate.h`: carries per-effect auxiliary selections.
- `Source/WandboundCore/Public/WBCardActivationExpansion.h`: carries auxiliary selection through expansion.
- `Source/WandboundCore/Public/WBEffectRunner.h`: exposes exact pending-attack redirect integration for the generic replacement transaction.
- `Source/WandboundCore/Public/WBReplayTrace.h`: adds deterministic inheritance trace fields.
- `Source/WandboundCore/Public/WBUnitReplacementEffect.h`: declares the generic transactional replacement executor.
- `Source/WandboundCore/Private/WBCardActivationCandidateGenerator.cpp`: emits deterministic instance-distinct auxiliary choices and stable IDs.
- `Source/WandboundCore/Private/WBCardActivationExpansion.cpp`: recognizes and transports the generic operation.
- `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp`: validates supported generic combinations fail-closed.
- `Source/WandboundCore/Private/WBEffectRunner.cpp`: dispatches the generic operation and reuses exact pending-attack redirect authority.
- `Source/WandboundCore/Private/WBMatchCoordinator.cpp`: builds private Hand choices from repository metadata and preserves pending-frame selection.
- `Source/WandboundCore/Private/WBReplayTrace.cpp`: serializes the generic inheritance trace fields deterministically.
- `Source/WandboundCore/Private/WBRules.cpp`: validates replacement legality and immutable exact selection.
- `Source/WandboundCore/Private/WBUnitReplacementEffect.cpp`: implements the working-copy replacement/inheritance transaction.
- `Source/WandboundRuntime/Public/WBProductionCSNCrashInSmoke.h`: declares isolated production smoke result/entry point.
- `Source/WandboundRuntime/Private/WBProductionCSNCrashInSmoke.cpp`: exercises the real packaged production lifecycle and privacy/replay checks.
- `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`: routes the isolated smoke flag and exit status.
- `Source/WandboundRuntime/WandboundRuntime.Build.cs`: stages the isolated production bundle and match fixture.
- `Source/WandboundTests/Private/WBCSNCrashInTests.cpp`: provides focused behavior, authority, privacy, replay, and smoke coverage.
- `Data/CardDB/Production/CSNCrashIn/root_manifest.json`: production root manifest.
- `Data/CardDB/Production/CSNCrashIn/bundle_manifest.json`: isolated production bundle manifest.
- `Data/CardDB/Production/CSNCrashIn/cards.json`: actual card and smoke-supporting production definitions.
- `Data/CardDB/Production/CSNCrashIn/markers.json`: isolated valid marker definitions.
- `Data/Replay/CSNCrashInFixture/match_spec.json`: deterministic packaged production match specification.
- `Docs/CSN_Crash_In_Production_Transfer_Audit.md`: this human-readable audit.
- `Docs/CSN_Crash_In_Production_Transfer_Audit.json`: machine-readable audit.

## Deferred Scope

- The separate CSN passive that draws after inheritance is not transferred in this pass.
- No additional ordinary post-summon reaction checkpoint is added inside the suspended Crash-In continuation.
- Broader replacement-card kinds, Hybrids, and arbitrary inheritance policies remain unsupported and fail closed.
- Crash-In does not contain Hero replacement authority and therefore cannot avert canonical Hero-loss terminal resolution.

