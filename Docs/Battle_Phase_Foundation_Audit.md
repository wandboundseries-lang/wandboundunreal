# Battle Phase Foundation Audit

## Status
Implementation, validation, and authorized process cleanup complete. READY.
No stage, commit, or push is authorized.
Public baseline: `a7d95c6020bd7fcd284c3637a594bf95018bca1a`.

## Canon
One nested Battle per accepted player Declared Attack, through reactions, damage, mandatory consequences, and Counter. Nonterminal completion resumes Action; terminal completion clears Battle.
The Rules Bible and glossary addenda define this envelope without adding a top-level
phase, new event kind, response timing, or battle-end trigger.

## Architecture
The coordinator opens Battle only after accepted player attack declaration and
before PreHit. Independent state survives working-state copies and nested Reacts.
PendingAttack continues to own every mutable damage stage and Counter field.
Battle identity includes coordinator generation/revision, turn, and root continuation,
so repeated identical Attack action IDs in one turn create distinct Battles.

Existing Counter conversion changes the current PendingAttack combatant authority.
The validator therefore checks player-declared authority for the root, while allowing
the existing automatic Counter (including a neutral combatant) inside that root.
NPC-origin combat does not create a player Battle.

Completion retains the envelope through mandatory destruction consequences and
private-choice suspension. The coordinator closes it before publishing restored
Action availability, or before terminal outcome commit. Death cancellation retains
the player continuation until its established cancellation processing consumes it.
These boundaries passed the complete 2,533-test automation suite.

## Conditions And Observation
Any remains the default. DuringBattle/OutsideBattle only restrict existing timing.
Normal activation timing does not gain priority during automatic stages.
The production parser, fixture parser, schema, repository validation, and non-default
CardDB condition digest support the new requirement.

Public summary exposes only active state, declaring player, and original public
unit participants. Start/end traces omit BattleId and continuation identifiers.
Protected state digest includes active Battle identity; inactive encoding preserves
existing noncombat state inputs. Replay schema remains 1 and receipt remains eight fields.

## Regressions And Deferrals
CardZoneTransitionTrigger remains its own authority with unchanged exact source
eligibility and FIFO processing. NPC roots, automatic attacks, standalone damage,
new cards, new response windows, UI, animation, and battle-end triggers are excluded.
Combat-stage enum values and top-level phase enums are unchanged.

## Validation
Full automation: **2,533 succeeded, 0 failed, 0 warnings, 0 not run**, delta +18
from 2,515. All 18 final Battle tests also passed independently after packaging.
The focused file contains 252 direct assertion sites.
Evidence: `Saved/AutomationReports/BattleFull/index.json` and
`Saved/AutomationReports/BattleFinalFocusedRetry/index.json`.

| Target | Configuration | Result | Seconds |
| --- | --- | --- | ---: |
| Editor | Non-unity | Succeeded | 6.18 |
| Game | Non-unity | Succeeded | 106.28 |
| Editor | Default unity | Succeeded | 71.23 |
| Game | Default unity | Succeeded | 90.09 |
| Editor | Forced unity | Succeeded, equivalent unity output up-to-date | 1.43 |
| Game | Forced unity | Succeeded, equivalent unity output up-to-date | 1.25 |

Final clean BuildCookRun succeeded in 1735.67 seconds, exit 0, 423 build actions,
zero build/cook error lines and zero warning lines. Build, cook, stage, package,
and archive completed. Output: `Saved/PackagedBuilds/BattlePhaseFoundation`.

Initial focused run: 9 passed, 2 failed expectations (automatic nested completion
and incorrectly oriented fixture wall). Corrected and verified in subsequent runs.
A later focused launch overlapped clean module removal and exited 1 before tests:
"The game module 'WandboundUE' could not be found." The post-package rerun passed.

Initial packaged launch omitted the explicit map and remained on the default map.
It never executed the Battle smoke. Corrected runs use the inner executable with
`WandboundUE /Game/Wandbound/Maps/Wandbound_LocalPlay_Dev` and package-relative
`Data/...` fixture paths. A validation-command property-count error was also
corrected; the receipts themselves were valid eight-field receipts.

## Packaged Evidence
Every Battle scenario passed twice: normal, counter, nested, prevented, terminal,
and terminal counter. Each pair had exactly equal archive, receipt, and startup
bytes. Fresh coordinator replay verified final state/trace digests and generation/
revision in every run. Schema is 1; every receipt has exactly 8 fields.
All runs had zero runtime errors and the same 21 pre-existing Datasmith/unspecified
actor-class warnings, with no new categories. All ten staged fixture hashes match
source. Detailed evidence is under `Saved/Validation/BattlePhaseFoundation`.

The synthetic bundle digest is
`ed79706efb4dc9ad8032eaaf3944f9ff4ea3f4ac89f5060125e267dc5aac25fe`.
All six match specifications use that loader-verified value.
All six Battle startup files hash to
`1dc38ca0dbba0a07fdbcd02d903259253ea2581a82705587bca565b865f275c5`.

### normal
Two passing runs, 3 accepted records, generation/revision 1/4.
- Archive: `3344e5229ff331b35a48da8641970fc7c0d2c670fd737517b01c341de74eb027`
- Receipt: `e167e63906f535abe022fa73361db03557fa7d99511ff4d2bf25d24d2b4f79a2`
- Replay: `2dfabe600359fb8150d84fd49e02212b1ba8f133ee5c263b82abd223f307e9ff`
- State: `c84b04e8edaec19717f69eb799e572e9cd4b3e80e9c39a10e1014656349b466f`
- Trace: `3a95797a5d0d99096b54ba91a879a03239e3a90af8d5b1e774f86fcbd1bc7dd0`

### counter
Two passing runs, 3 accepted records, generation/revision 1/4.
- Archive: `a5de02bd557d9d02cb8e8322a80a3be8f9323c38ee9a201950ae26c9738fd598`
- Receipt: `4567bd2141b3bd5415454639c166402c9f0832fe49c021e03c5e4d825f1713e1`
- Replay: `032210c3ed3858df28e58a25b15891d3cfa1529b09bf5727c404ff843d68a7ac`
- State: `22a2d2ce659178111fb02d8db0974234958b38197c95eb8968d1220b492043f8`
- Trace: `79288ddbb0a315062823523aeb7bc6d3b8e05e4cd580a75f95e03affe0cfa3c0`

### nested
Two passing runs, 6 accepted records, generation/revision 1/7.
- Archive: `233b7e48a82523dfc088972df223a663695d68b2915c46f800fe8edab6c5cc03`
- Receipt: `4e7a6e3ac36d30039ff653753b2a0a726b550bf322e2015e7ae7ff8b1bf4c613`
- Replay: `06e52d661c9ec2fcebac65152739d8af03760affdc8116b3aca96ca2dae02e81`
- State: `3d7b186516a110824523ce6bc41ea79f5ed4ce3ac215c1b6fe79c61d2ca21462`
- Trace: `f3dee02a964d2dd1fd861df3c96db0aeb153a1f3b1f2d21e34ce92ff7ea427ce`

### prevented
Two passing runs, 6 accepted records, generation/revision 1/7.
- Archive: `fb2eed0da6c259afeebf1d4cd2f7c03ef8bbe41d0c57882c843ce0529d7650ee`
- Receipt: `286fdafd74b6467e09f371283e8ab82062f31288b22eaa065f34b9d8576c0559`
- Replay: `e4dc5764660843b4df70414e9c4fe09d8823a13f7b72106f35788d8bd53a0266`
- State: `9d217d9780e321994f27c68bf93f369a04e78057c2c8cf99a83559294bcf6cc6`
- Trace: `759f9b3717827e7bada0d53cef035ccb7be509174a52c4159c2e7384a4aaa4ae`

### terminal
Two passing runs, 3 accepted records, generation/revision 1/4.
- Archive: `f9920c96b173118c81c2b444447330140104154d2976b4b46d33c9acd9c4d6cb`
- Receipt: `59a49ebe9b48486baddcc0ccc9277e36438323e5c1587953af33c68c55bb42c7`
- Replay: `a4ec15ee7f1c7b60c8274bcc5cb085e387357da5f98af88a0aedc2867a0a7f79`
- State: `db12ed1554c344f3614f0d9497ab5eadb399164d28aa13c9eda4153b29fe732b`
- Trace: `b52a24a0d471cb55887bfe8966fc2a2311759002f78ed2f2116043bcfe27cd02`

### terminal_counter
Two passing runs, 3 accepted records, generation/revision 1/4.
- Archive: `8c06ada9db9f7f21728fadc786174e565f56ca1c0cb03235d2c4d237cd5c6714`
- Receipt: `3d17106b9b61e0f26afcb2b3e94e9a0c8b05bad8d3261e5aa4c7c78bcd6ad4d5`
- Replay: `9e699a0d560ef9cc6ccc577187bb3f52f5a9ab0270e538c33efc6345d6aa44ff`
- State: `734740d37ee6383f9b6efa7291c5f01a8a09ad80b81ea3e54d4414a1f224ae33`
- Trace: `10f1664cc1380f3d6416481c5e18140d861cedeaba15053a2fbb4c6b9c83951a`

### Regression Comparisons
NPC combat passed twice with fresh replay and byte-identical archives/receipts/startup.
The existing NPC smoke also includes a later player-declared attack, so its complete
archive is not asserted to contain zero Battle events: the automatic NPC root
remains excluded, while the later player root correctly creates Battle.
NPC archive: `f3a6e6ca7a1037f603ab799605dadd9435b41f7fcb333ad1b7d6f94ce4a4d9f2`.
NPC final state: `b9c7c49302f76884a432c7f50de625b372c05122204581cd5297c86f672f4aed`.
NPC final trace: `9a988d7230b19ab094d53268c1a0e18bdb0e9f6121b0ea6ac99a72abeebf96d2`.

The zone-trigger regression, rerun twice with the exact baseline DiscardActivation
bootstrap, preserved the original receipt and replay hashes:
- Receipt: `9b4a8c016857a0b1134f460654dfe20129e94c48b2cf8997b4b311de56ddd856`
- Replay: `ed89ba26461b91dd7574b10582eccf925b52786e3134c411c1e23893a63d414e`

An earlier pair using the NPC bootstrap also passed but produced a different stable
standalone zone-trigger digest. That different invocation is retained separately,
not presented as a replacement baseline. No prior expected hash was changed.

## Safety And Readiness
Both audits equal the actual 34-path task manifest. No protected local-history
identifiers were found in task files. Protected local history remains outside
public HEAD and fetched remotes. Codec blobs match the required values; protocol
and existing phase/stage/event enums are unchanged. No stage, commit, or push.

## Packaged Process Cleanup
Verified packaged PID 19384 was terminated after validation, following immediate
revalidation of its name, full executable path, and smoke command line. No packaged
Wandbound process remains. Only PID 19384 was terminated. Termination was
cleanup-only and caused no source, test, fixture, replay, or runtime implementation
drift. The corrected smoke processes had exited automatically.

## Final Worktree Gates
HEAD and origin/main remain a7d95c6020bd7fcd284c3637a594bf95018bca1a.
The exact 34 task paths remain: 15 tracked modifications and 19 untracked task
files. Both audit manifests agree. There are zero staged files and no unexpected
task paths; git diff --check passes with only line-ending notices.
No source file is newer than the validated packaged build; all ten staged Battle
fixture hashes still match source. The three unrelated untracked files remain
untouched. No tests, builds, or packaged smokes were rerun for cleanup.
Only the two existing audit documents received cleanup/readiness updates.

## Readiness
All implementation, validation, repository-hygiene, process-cleanup, privacy,
protocol, and determinism gates are satisfied.
P0 BATTLE PHASE FOUNDATION READY

## Exact Changed Paths
Both audits contain the same 34 paths. All task paths were clean or absent at
baseline; no task overlaps pre-existing dirty work. Three unrelated untracked files
remain excluded. The manifest must be updated if any further task path is added.

1. `Data/CardDB/ProductionCardDB.schema.json`
2. `Data/Replay/BattlePhaseFixture/bundle_manifest.json`
3. `Data/Replay/BattlePhaseFixture/markers.json`
4. `Data/Replay/BattlePhaseFixture/match_counter.json`
5. `Data/Replay/BattlePhaseFixture/match_nested.json`
6. `Data/Replay/BattlePhaseFixture/match_normal.json`
7. `Data/Replay/BattlePhaseFixture/match_prevented.json`
8. `Data/Replay/BattlePhaseFixture/match_terminal.json`
9. `Data/Replay/BattlePhaseFixture/match_terminal_counter.json`
10. `Data/Replay/BattlePhaseFixture/root_manifest.json`
11. `Data/Replay/BattlePhaseFixture/units.json`
12. `Docs/Battle_Phase_Foundation_Audit.json`
13. `Docs/Battle_Phase_Foundation_Audit.md`
14. `Docs/Wandbound_Canonical_Glossary_Battle_Phase_Addendum_v1.md`
15. `Docs/Wandbound_Rules_Bible_Battle_Phase_Addendum_v1.md`
16. `Reference/GodotCanon/README.md`
17. `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`
18. `Source/WandboundCore/Private/WBBattlePhase.cpp`
19. `Source/WandboundCore/Private/WBCardActivationSourceGate.cpp`
20. `Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp`
21. `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp`
22. `Source/WandboundCore/Private/WBDeathResolution.cpp`
23. `Source/WandboundCore/Private/WBGameStateData.cpp`
24. `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
25. `Source/WandboundCore/Private/WBProductionMatchReplay.cpp`
26. `Source/WandboundCore/Public/WBBattlePhase.h`
27. `Source/WandboundCore/Public/WBCardDefinition.h`
28. `Source/WandboundCore/Public/WBGameStateData.h`
29. `Source/WandboundCore/Public/WBMatchCoordinator.h`
30. `Source/WandboundRuntime/Private/WBProductionBattlePhaseSmoke.cpp`
31. `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`
32. `Source/WandboundRuntime/Public/WBProductionBattlePhaseSmoke.h`
33. `Source/WandboundRuntime/WandboundRuntime.Build.cs`
34. `Source/WandboundTests/Private/WBBattlePhaseTests.cpp`
