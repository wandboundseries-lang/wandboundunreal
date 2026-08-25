# Wandbound CSN Patch Production Transfer Audit

## Result

CSN Patch is production-reachable through the existing CardDB, activation,
coordinator, replay, privacy, and packaged-runtime paths. Its activated effect is
implemented as a generic definition-driven continuation. The source is
sacrificed, not destroyed; an immutable activated-effect source snapshot preserves
its tile, Current RL, and exact equipped Wands; and an explicit private Deck choice
can later summon one exact eligible CSN Character to that tile with CSN
Inheritance.

## Verified Baseline

- `HEAD`: `1784944335631db96c77aa32ddfbb6c3347ae55a`
- `origin/main`: `1784944335631db96c77aa32ddfbb6c3347ae55a`
- Initial Wandbound automation baseline: 2,356 tests
- Staged files before and after the pass: none
- Pre-existing unrelated untracked files were preserved unchanged:
  `Content/Maps/NewProjectTest.umap`,
  `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`, and
  `h origin main`.

## Canonical Card Definition

Production `char_csn_patch` is a CSN Character with HP 12, ATK 2, AR 2, RL 2,
orthogonal-adjacent movement, and orthogonal effective-AR attack range. Its board
activation has no target, is reactable through the existing effect-response
checkpoint, and uses the existing once-per-turn source gate.

The owner clarification supersedes the obsolete Godot Wand-tutor implementation.
The production text and payload contain only the sacrifice-then-Deck-summon
behavior. No Wand tutor remains in the production card definition or runtime
resolution.

## Godot Behavior Audited

The read-only Godot CardDB and resolution flow were inspected for source timing,
source-tile capture, Deck filtering, and the former Wand tutor. Godot files were
not edited, imported, compiled, or runtime-loaded. Owner canon replaces only the
obsolete tutor behavior; established deterministic timing and CSN Inheritance
semantics were retained.

## Generic Authority

`EWBGenericEffectOp::SacrificeSourceThenSummonCharacterFromDeckToSourceTile`
expresses the behavior in card data. Repository and production-loader validation
fail closed on invalid source faction, summon faction, summon kind, or inheritance
policy. Core semantic code contains no `char_csn_patch` branch.

`WBActivatedDeckSummonContinuation` owns the activated-effect source snapshot and
committed sacrifice continuation. `WBMandatoryDeckChoice` is the shared
coordinator-facing choice router for both the existing post-destruction Rook path
and the activated-effect path. `WBMatchCoordinator` remains the production action
authority. Each accepted coordinator action is atomic; the activation continuation
and later mandatory-choice submission are separate accepted actions and are not one
cross-decision transaction.

## Activation, Reaction, and Usage

The effect is generated only for the owning on-board source during normal-turn
priority. Stunned and otherwise illegal sources fail through existing source
gates. The established Frozen activation policy is unchanged. Successful use marks
the existing once-per-turn key; turn reset behavior is unchanged.

The activation enters the existing effect reaction window. Negation leaves Patch,
its Wands, zones, and Deck unchanged. On successful continuation there is no new
normal post-summon response checkpoint.

## Sacrifice and Snapshot

The continuation captures the source before mutation, emits
`activated_effect_source_snapshotted`, detaches exact equipped Wand instances,
and removes the source with the non-destruction `unit_sacrificed` path. It does
not create a destruction event, destruction cause, or post-destruction trigger.

Patch uses `FWBActivatedEffectSourceSnapshot`, which contains only activated-effect
continuation data. It neither creates nor stores `FWBUnitDestructionSnapshot`.
`FWBUnitDestructionSnapshot` remains reserved for genuine destruction events such
as Rook's post-destruction trigger path. Patch sacrifice never enters the
destruction queue and never supplies Sable's destruction observer.

## Mandatory Deck Choice

- Zero legal candidates: the sacrifice remains committed, detached Wands are
  released through normal cleanup, no choice opens, and no unit is summoned.
- One legal candidate: a private `MandatoryDeckChoice` still opens and the
  controller explicitly submits that exact instance.
- Multiple legal candidates: the controller explicitly chooses one exact eligible
  instance in deterministic Deck order.

Eligible candidates must be exact Deck instances whose production definition is
Kind Character and has explicit CSN faction metadata. Card-ID prefixes, public
names, Wands, actions, and non-CSN Characters do not qualify. Submission
revalidates the instance and destination tile. Duplicate CardIds remain distinct
instances. Opponents do not receive candidate instance IDs or Deck order.

The sacrifice is already committed when the private choice is published. A stale
candidate submission is rejected without mutation and leaves the still-valid
private continuation available. A later valid exact-instance submission resolves
normally and cannot duplicate detached Wands.

## Summon and CSN Inheritance

The selected Character is summoned to the captured source tile. Unit capacity is
checked after Patch has left the board. Normal summon conditions are intentionally
bypassed for this effect, while exact instance, tile occupancy, bounds, and
definition eligibility are revalidated.

`WBDeckSummon` invokes `WBCSNInheritance` with neutral
`FWBCSNInheritanceSourceData`. Genuine destruction paths convert their destruction
snapshot into that neutral form, while Patch converts its activated-effect
snapshot. Exact Wand instance IDs, owners, and deterministic order are preserved.
The summoned unit's Base RL increases by Patch's snapshotted Current RL. The Deck
is not shuffled.

Composition coverage confirms:

- Undertow Archivist receives Patch inheritance, then performs its existing draw.
- Sable observes no destruction from Patch's sacrifice.
- Vex aura behavior remains definition-driven after the summoned unit enters play.
- Rook's post-destruction mandatory Deck choice remains compatible with the shared
  choice router, retains a genuine destruction snapshot, and preserves explicit
  one-candidate choice semantics.

If Patch is the active Hero, self-sacrifice follows the existing non-Hybrid
Hero-loss path. Terminal cleanup releases every exact detached Wand to Discard,
opens no choice, summons no replacement, and creates no destruction event. There
is no Hero-less reaction checkpoint.

Choice submission is one coordinator action with a working-copy transaction for
the selected Deck summon and inheritance. If an Undertow summon cannot complete
its required draw, that submitted action restores the exact candidate to Deck and
rolls back the created unit, inheritance, and draw. It does not roll back the
earlier accepted Patch sacrifice. The failed continuation closes and releases the
detached Wands to Discard. Occupied-destination failure follows the same committed
sacrifice boundary and exact Wand disposition.

## Replay and Privacy

Replay schema remains 1. The accepted player decisions are End Turn, Summon,
Equip, Activate, and exact Mandatory Deck Choice. The protected state digest now
includes generalized pending-choice origin/context and immutable snapshot data so
fresh replay detects continuation drift. Rejected choices are not accepted replay
records.

The packaged smoke persisted and reloaded the replay, then replayed it through a
fresh coordinator and matched final state and trace digests. The public receipt
remains exactly eight fields and contains no exact Deck/Wand instance, private
state digest, private trace digest, seed, path, legal-action set, opponent hand, or
Deck-order disclosure.

`WBActionCodec` is unchanged:

- Header blob: `44ef87156beb5799066c2a5ecbc98f04928d98c0`
- Source blob: `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`

## Production Data

- Production bundle digest:
  `2d5995fa766ef14396be7b06a6b00c0ce2ff3bf33326a4925e6169598baaa52e`
- Isolated fixture: `Data/Replay/CSNPatchFixture/match_spec.json`
- Existing CSN production fixtures pin the same computed bundle digest.

## Tests

Focused Patch automation: 13 succeeded, 0 failed. The tests contain more than
120 assertions across production definition, owner supersession, source gates,
once-per-turn reset, reaction negation, zero/one/multiple candidates, exact
instance privacy, stale submissions, tile revalidation, unit cap, exact Wand and
Current-RL inheritance, no tutor/no shuffle/no extra response, CSN composition,
Hero loss, definition-driven alternate identity, coordinator replay, and production
smoke.

The remediation adds direct coverage for Hero Patch terminal cleanup with exact
Wands and replay determinism, genuine Patch death growing Sable while Patch
self-sacrifice does not, and Patch into Undertow with an empty Deck rolling back
only the submitted summon transaction. Existing stale-candidate and occupied-tile
tests now assert the committed sacrifice boundary and exact detached-Wand state.

Focused CSN regression: 85 succeeded, 0 failed:

- Body Double: 13
- Crash-In: 9
- Patch: 13
- Rook: 11
- Sable: 9
- Undertow Archivist: 16
- Vex: 14

Full Wandbound automation: 2,369 succeeded, 0 failed, 0 warnings, 0 not run. This
is a net increase of 13 over the 2,356-test baseline and three tests over the
initial Patch transfer validation.

## Build Matrix

All six requested build commands succeeded:

1. Editor Development non-unity: succeeded, 104.43 s, 137 actions.
2. Game Development non-unity: succeeded, 103.41 s, 126 actions.
3. Editor Development default: succeeded, 266.07 s, 211 actions.
4. Game Development default: succeeded, 272.68 s, 186 actions.
5. Editor Development forced-unity: succeeded, 336.86 s, 195 actions.
6. Game Development forced-unity: succeeded, 267.97 s, 170 actions.

The forced-unity runs used `-ForceUnity -DisableAdaptiveUnity` and compiled the
actual aggregate Core module. Their first compile exposed duplicate anonymous
namespace helper names in the two continuation translation units; the helpers were
given file-specific names and both forced-unity targets then rebuilt successfully.

The final clean BuildCookRun then performed an independent 397-action Editor/Game
rebuild, discovered 532 cooked packages, wrote 459 runtime package-store packages,
staged, packaged, and archived successfully. AutomationTool exited 0 after
667.79 s (11m10s).

## Packaged Smoke

The freshly archived inner executable was run twice with the required
`WandboundUE` bootstrap argument, explicit local-play map, and package-relative
paths to the production CSN bundle and isolated Patch fixture. Both runs completed
the real production-provider/coordinator flow and exited with status 0. No packaged
process remained.

Repeated artifacts were byte-identical:

- Archive SHA-256:
  `0202434248a6a54ad5395f3c58364f24cbad437c60221cbe4dea619b7e535773`
- Receipt SHA-256:
  `5b6f07006faf172c90b9d15dbf6ec14bbd6a688ef3cfbeb6daa5e25674083949`
- Startup JSON SHA-256:
  `5aadc00dd0e67180c2d0e51374c63335b4e766a7922280d6b48b43b5de9ac047`
- Replay digest:
  `81bff6507026ce08b14301ec517e2589b4c9a91d2ea9e66c66e90d16934cfaa8`
- Final state digest:
  `f8cc43a4083540a3b61b7b6482709bf67fd53f3574194c5e5845e0034281cb2f`
- Final trace digest:
  `46e1d60ca1abb4251a8baa580e1078ae8c16cf8be3e6610ea77a98b282750f81`
- Generation/revision: 1/6
- Accepted records: 5
- Receipt fields: 8

## Changed Paths

All task paths were clean at baseline and have no unrelated hunk overlap.

1. `Data/CardDB/Production/CSNCrashIn/cards.json` - production Patch definition.
2. `Data/Replay/CSNCrashInFixture/match_spec.json` - current bundle digest.
3. `Data/Replay/CSNRookFixture/match_spec.json` - current bundle digest.
4. `Data/Replay/CSNSableFixture/match_spec.json` - current bundle digest.
5. `Data/Replay/CSNUndertowArchivistFixture/match_spec.json` - current bundle digest.
6. `Data/Replay/CSNVexFixture/match_spec.json` - current bundle digest.
7. `Data/Replay/CSNPatchFixture/match_spec.json` - isolated production smoke fixture.
8. `Docs/CSN_Patch_Production_Transfer_Audit.md` - human-readable audit.
9. `Docs/CSN_Patch_Production_Transfer_Audit.json` - machine-readable audit.
10. `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp` - production parse, validation, and digest authority.
11. `Source/WandboundCore/Public/WBEffectRequest.h` - generic operation type.
12. `Source/WandboundCore/Public/WBGameStateData.h` - separate activated-effect and genuine-destruction choice snapshots.
13. `Source/WandboundCore/Public/WBActivatedDeckSummonContinuation.h` - generic continuation API.
14. `Source/WandboundCore/Private/WBActivatedDeckSummonContinuation.cpp` - non-destruction snapshot, committed sacrifice, and choice publication.
15. `Source/WandboundCore/Public/WBMandatoryDeckChoice.h` - shared mandatory-choice API.
16. `Source/WandboundCore/Private/WBMandatoryDeckChoice.cpp` - deterministic enumeration and exact-instance submission.
17. `Source/WandboundCore/Public/WBCSNInheritance.h` - neutral inheritance source data.
18. `Source/WandboundCore/Private/WBCSNInheritance.cpp` - detached exact-Wand transfer validation.
19. `Source/WandboundCore/Public/WBDeckSummon.h` - configurable inheritance and trace request.
20. `Source/WandboundCore/Private/WBDeckSummon.cpp` - configured inheritance/trace execution.
21. `Source/WandboundCore/Private/WBCardActivationExpansion.cpp` - recognizes the coordinator-owned operation.
22. `Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp` - fixture parsing.
23. `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp` - semantic validation.
24. `Source/WandboundCore/Private/WBEffectRunner.cpp` - coordinator-owned continuation boundary.
25. `Source/WandboundCore/Private/WBMatchCoordinator.cpp` - authoritative continuation and choice routing.
26. `Source/WandboundCore/Private/WBPostDestructionTrigger.cpp` - converts genuine Rook destruction data to neutral inheritance input.
27. `Source/WandboundCore/Private/WBProductionMatchReplay.cpp` - origin-typed protected continuation-state digest coverage.
28. `Source/WandboundCore/Private/WBRules.cpp` - generic payload legality validation.
29. `Source/WandboundRuntime/Public/WBProductionCSNCrashInSmoke.h` - Patch smoke API.
30. `Source/WandboundRuntime/Private/WBProductionCSNCrashInSmoke.cpp` - packaged production Patch scenario and replay/privacy verification.
31. `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp` - packaged Patch smoke flag and exit status.
32. `Source/WandboundRuntime/WandboundRuntime.Build.cs` - stages the isolated Patch fixture.
33. `Source/WandboundTests/Private/WBCSNPatchTests.cpp` - focused Patch matrix.

## Remaining Limitations

- This pass adds only the exact single-unit Patch effect; it does not introduce a
  general multi-select Deck choice or unrestricted nested continuation stack.
- Invalid external mutation between choice publication and submission fails closed;
  production coordinator transactions do not expose such mutation authority.
- Card-specific trigger families beyond the established Undertow, Sable, Vex,
  Rook, and CSN Inheritance composition remain separate future transfers.

## Readiness

Implementation, focused/regression/full automation, six build commands, clean
BuildCookRun, twice-repeated packaged smoke, fresh replay, deterministic hashes,
privacy checks, source-control checks, and codec/schema guards all pass. CSN Patch
is ready as an unstaged 33-path production transfer.
