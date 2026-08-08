# Wandbound Non-Hero Hybrid Summon Report

## Result

The existing atomic Hybrid Hero replacement has been generalized into one
deterministic Hybrid summon transaction with two branches. A controlled
non-Hero Character can now be sacrificed to summon the Hybrid to a legal
orthogonally adjacent tile around the current Hero while the current Hero
remains authoritative. The existing Hero branch remains externally compatible.

Final validation:

| Check | Result |
| --- | --- |
| Editor build | Succeeded, final rebuild 26.34 seconds |
| Game build | Succeeded, final rebuild 36.42 seconds |
| Focused `Wandbound.Hybrid.NonHero` | 65 succeeded, 0 failed |
| Focused `Wandbound.Replay.Hybrid.NonHero` | 9 succeeded, 0 failed |
| Focused `Wandbound.Authority.Hybrid.NonHero` | 9 succeeded, 0 failed |
| Affected groups | 897 succeeded, 0 failed, 0 warnings |
| Full automation | Initial 2,034; final 2,122 succeeded, 0 failed, 0 not run |
| BuildCookRun | Succeeded, exit 0, final run 160.24 seconds |
| Packaged Development smoke | Passed, exit 0 |
| Canonical startup, two runs | Passed; byte-identical baseline hash |
| Partial replay regression | Passed; archive and receipt baseline-identical |
| Terminal replay regression | Passed; archive and receipt baseline-identical |
| Hero Hybrid regression, two runs | Passed; archive and receipt baseline-identical |
| Non-Hero Hybrid smoke, two runs | Passed; both exited 0 |
| Fresh non-Hero replay | Passed twice inside the packaged smoke |
| Repeated archive / receipt | Byte-identical |
| Privacy scan | 4 public files, 9 forbidden tokens, 0 findings |
| `git diff --check` | No whitespace errors; LF-to-CRLF notices only |

The packaged Development smoke reported `success=true`, the expected local-play
map and game mode, 81 tiles, two visible Heroes, eight concealed markers,
nonterminal state, and process exit code 0.

### Exact errors encountered and resolved

1. The initial PowerShell package invocation was blocked by execution policy;
   the approved `-ExecutionPolicy Bypass` invocation succeeded.
2. `TArray::CountByPredicate` was unavailable in one helper; an explicit loop
   replaced it and compilation succeeded.
3. One startup-probe process remained after a command-line experiment and held
   Runtime DLLs. PID 11192 was verified as that probe and stopped; no unrelated
   process was touched.
4. The conceptual `Wandbound.Summon` automation prefix matched no tests and
   exited 255. The actual registered affected prefixes were run and all 897
   tests passed.
5. The first full run found one authority source-guard violation caused by a
   Runtime public-zone observation dependency. The smoke was changed to inspect
   immutable public DTOs only; the guard and final full suite passed.
6. One combined Editor/Game wrapper timed out after the Game build had entered
   final metadata shutdown. The build log recorded `Result: Succeeded`; the
   final standalone Game rebuild also succeeded with exit 0.

No final errors remain.

## Baseline

- Commit and `origin/main`: `58ed67adcfa3432e1b736674ba3f5dfd15f6cd01`
  (`Add atomic Hybrid Hero replacement`), synchronized at audit time.
- Initial automation: 2,034 succeeded.
- Replay schema version: 1.
- Production bundle digest:
  `87d2644aeb479e84a3e96967fd57901ac52aa7e283fd5cfee142d35e8659f00c`.
- Production match-spec SHA-256:
  `5fd3ba8d78af9681e3acdc4c4b58e7c2934aacbcfed83cd3504a8a37e23b03da`.
- Canonical startup SHA-256:
  `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- Partial archive / receipt:
  `d30304a936fd3b5c2163209546b9063a64ed7a223a65b217092cb64ef6495463` /
  `881ffb586544d5ed78156635754b5eeddf555c7ef000c472f79ac607cc4d2dd9`.
- Terminal archive / receipt:
  `4e30424a56b613cbbda225295a0775473ed661cda390f172b609e529450235cc` /
  `5bcce2e1e9361e8848e4757a634cf82acdee30d2463a01f6f9f0023157e1ca76`.
- `WBActionCodec.h`: source SHA-256
  `c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949`,
  Git blob `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- `WBActionCodec.cpp`: source SHA-256
  `ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99`,
  Git blob `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.

The baseline working tree was already broadly dirty. All files used for this
pass were clean at baseline, so there was no task/pre-existing overlap in a
task file. No files were staged before or after the pass.

## Canonical Rules

The production rule now supports exactly one controlled active Character as a
sacrifice. The current Hero selects the existing replacement branch; a
controlled non-Hero Character selects the new ordinary summon branch. Exactly
one Wand is paid from the acting player's hand or from equipment attached to
the exact sacrificed unit. Remaining sacrificed-unit equipment is discarded.

Non-Hero placement is one of the four orthogonally adjacent tiles around the
authoritative current Hero. Occupancy is evaluated after the selected sacrifice
is removed, so the sacrificed unit's tile is eligible only when it is itself
adjacent to the Hero. Diagonals, out-of-bounds tiles, and tiles occupied by any
other unit are rejected. Existing Turn-1 restrictions remain authoritative.

The four-unit cap is evaluated as `active owned - one sacrifice + one Hybrid`.
Sacrifice is not defeat and does not invoke death or Hero-loss processing.
Sacrifice-trigger timing, card-specific summon-trigger timing, response
windows, and state inheritance remain unsupported.

## Planner Generalization

`FWBHybridSummonPlan` now represents both branches with `SacrificedUnitId` and
`bBecomesReplacementHero`. `BuildSummonPlans` enumerates complete immutable
transactions for every Hybrid in hand, eligible controlled Character,
completed-state destination, and eligible payment. The Hero-only planner remains
as a compatibility wrapper.

Candidates exclude opponents, inactive/removed units, NPCs, and Hybrid units.
Hand Wands are owner-checked. Equipped Wands are eligible only when attached to
the exact sacrifice. Canonical order is sacrifice unit ID, destination Y/X,
payment source, then payment instance ID. Equivalent state therefore produces
the same plan list regardless of source-container insertion order.

## Stable Action IDs

Both branches retain the existing Summon-family ID:

```text
hybrid_summon:p{player}:i{hybrid_instance}:s{sacrificed_unit}:w{source}:i{wand_instance}:x{x}:y{y}
```

Sacrifice, destination, and payment differences are encoded independently.
Tests prove same-plan stability, distinct IDs for each choice, canonical
ordering, and unchanged Hero-replacement IDs. `WBActionCodec` and replay schema
were not modified.

## Preflight

Execution regenerates the current legal plan list and requires an exact match
before touching a working copy. Validation covers generation, revision, acting
player, Hybrid instance/definition/hand zone, active Character sacrifice,
ownership, authoritative Hero and branch mode, payment source/ownership/zone or
attachment, completed destination occupancy, bounds, Turn-1 restrictions,
completed unit cap, and zone integrity.

Typed failure codes include:

```text
hybrid_wrong_player
hybrid_plan_stale
hybrid_definition_invalid
hybrid_not_in_hand
hybrid_sacrifice_required
hybrid_sacrifice_invalid
hybrid_hero_sacrifice_invalid
hybrid_wand_payment_required
hybrid_wand_payment_invalid
hybrid_destination_invalid
hybrid_destination_occupied
hybrid_unit_cap_exceeded
hybrid_unit_id_allocation_failed
hybrid_zone_state_invalid
hybrid_replacement_not_supported
```

Invalid, altered, or stale plans leave state and replay unchanged.

## Atomic Commit

The generalized executor copies state, consumes the selected Wand payment,
discards remaining sacrifice equipment deterministically, removes the Hybrid
from hand, removes the selected Character from active board occupancy, clears
applicable pending attack references, creates a fresh printed-stat Hybrid at the
planned destination, conditionally changes `HeroUnitId` only for the Hero
branch, validates zone integrity, and commits once. Traces are appended only
after successful commit; the coordinator then performs existing marker,
terminal, replay, and next-decision processing.

## Non-Hero Sacrifice

The selected non-Hero Character remains as the existing inactive historical
unit representation and is absent from board occupancy. It is not sent through
`WBDeathResolution`, does not emit defeat events, and cannot produce game-over.
The acting player's original `HeroUnitId` remains `0` throughout the packaged
scenario. The fresh Hybrid receives new unit ID `4` and is not marked Hero.

## Placement

The packaged transaction sacrifices unit `2` and places the Hybrid at `(4,7)`,
the vacated sacrifice tile adjacent to Hero `0`. Planner tests separately prove
all four orthogonal candidates, bounds filtering, occupied-tile filtering,
diagonal exclusion, and completed-state eligibility of the sacrificed tile.

## Payment and Cleanup

Payment can come from hand or from one Wand attached to the selected sacrifice.
The packaged choice uses `sacrificed_unit` payment. The exact paid Wand is found
once in discard after the transaction, all remaining sacrifice equipment is
discarded deterministically, unrelated equipment remains untouched, and no
orphan attachments remain. The packaged smoke explicitly requires zero final
equipped cards and fresh-replay discard counts equal to live coordinator counts.

## Unit Cap

Tests exercise a player already at four active units. Removing one selected
Character and adding one Hybrid leaves four, so the transaction is legal. Any
plan whose completed state would exceed four fails preflight.

## Marker Integration

The coordinator continues to call the existing post-summon marker path with the
new Hybrid unit ID. Existing Trap damage and NPC behavior remain unchanged. A
Trap can damage or remove the ordinary Hybrid without defeating the still-live
original Hero or making the match terminal. Marker visibility continues through
the existing observation policy.

## Terminal Boundary

Non-Hero sacrifice leaves `terminal=false`, winner and loser `-1`, and no Hero
loss reason. It emits no `hero_sacrifice_committed`,
`hero_replacement_committed`, `hero_defeated`, or `game_over`. The Hero branch
still installs its replacement atomically, and later ordinary Hero loss still
terminates with `hero_defeated_without_replacement`.

## Replay

One accepted Hybrid action creates one Summon-family replay record. Rejected
actions create none. The packaged smoke replays its persisted archive through a
fresh coordinator and explicitly compares original Hero ID, generation,
revision, state digest, trace digest, discard counts, equipped-card count,
terminal state, and completion state. New-Hybrid identity and board/zone state
are covered by the identical complete state digest.

## Privacy

Runtime selects only coordinator-produced legal IDs. Public observation confirms
the original Hero and ordinary Hybrid while hiding the paid Wand and alternate
payment candidates. Public payment traces contain neither paid instance nor Wand
definition. The receipt remains exactly eight fields and excludes seeds,
actions, hidden cards/markers, state/trace digests, and paths.

The final scan checked both non-Hero receipts and both startup JSON files for
nine forbidden private tokens and found zero matches.

## Existing Hybrid Regression

Prior and final packaged Hero evidence is identical:

| Artifact | Prior | Final run 1 | Final run 2 |
| --- | --- | --- | --- |
| Archive | `e1fa69301728e8129e69866ce0a91fbeaf77cc990d08f0a33133943bc629be20` | same | same |
| Receipt | `7cdba9356c9fbb6c796aaaedfbeef7fc884ec74522a4a20231d082961cc6f156` | same | same |
| Replay digest | `9c73493e6931a627969a7472b49c858a623150de49da522ef963663f41e3f98e` | same | same |
| Final state | `9abce6721fb022c2769f8207b4dd07c79811a65bb8fc25f97962feb4e09f1897` | same | same |
| Final trace | `fdc47bcbab4c83987b4c9749cf1e905074a90a6d6fd914f4606201b8c83b7703` | same | same |

## Packaged Non-Hero Smoke

```text
fixture: Data/Replay/HybridNonHeroFixture/
fixture bundle digest: 42f743203d5d07b674fcc68c603effb35700160d4443e765835f2662ed34c108
sacrificed unit ID: 2
original Hero ID: 0
new Hybrid ID: 4
destination: (4,7)
payment source: sacrificed_unit
archive SHA-256: f3b0cb64cb2bd45ae6816ddc871b0a08d89cac55272110a102e94fb666380c1d
receipt SHA-256: f3dab3075d43922bd1bcd59e377370f69bb848d5268a04d8bbfc4c5b9e1aa3f1
replay digest: e84384339113dc407708b223ef29532eeee595e4da3b3434c6ba97159369bac4
state digest: 4c30735d3f70def1c8ca7c3c63787ac71f364399a1f32da7a74d2de0605803ba
trace digest: c4c3e2fed801da6de224b9ab5b76563e2e1ea37127855d17a9f72eadefc846b3
record count: 5
generation / revision: 1 / 6
terminal / complete: false / false
winner / loser: -1 / -1
repeated archive equality: true
repeated receipt equality: true
repeated startup equality: true
receipt field count: 8
```

The chosen action ID was:

```text
hybrid_summon:p0:ip0_card_000_hybrid_nonhero_summon:s2:wsacrificed_unit:ip0_card_001_hybrid_nonhero_wand:x4:y7
```

Canonical startup remained byte-identical at
`cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
The inner packaged executable was invoked with required `WandboundUE` bootstrap
argument and package-relative `Data/...` paths.

## Tests

`WBHybridNonHeroSummonTests.cpp` adds one fixture-loading test and all 87
requested exact registrations. The exact names are grouped below.

```text
Wandbound.Hybrid.NonHero.Fixture.BundleLoads
Wandbound.Hybrid.NonHero.Planner.ControlledCharacterEligible
Wandbound.Hybrid.NonHero.Planner.HeroAndNonHeroBranchesCoexist
Wandbound.Hybrid.NonHero.Planner.OpponentCharacterExcluded
Wandbound.Hybrid.NonHero.Planner.InactiveCharacterExcluded
Wandbound.Hybrid.NonHero.Planner.HybridUnitExcludedAsCharacterSacrifice
Wandbound.Hybrid.NonHero.Planner.AdjacentDestinationsEnumerated
Wandbound.Hybrid.NonHero.Planner.DiagonalDestinationExcluded
Wandbound.Hybrid.NonHero.Planner.OutOfBoundsDestinationExcluded
Wandbound.Hybrid.NonHero.Planner.OccupiedDestinationExcluded
Wandbound.Hybrid.NonHero.Planner.SacrificedTileEligibleWhenAdjacent
Wandbound.Hybrid.NonHero.Payment.HandWandEligible
Wandbound.Hybrid.NonHero.Payment.SacrificedUnitWandEligible
Wandbound.Hybrid.NonHero.Payment.OtherUnitEquipmentExcluded
Wandbound.Hybrid.NonHero.Payment.OpponentWandExcluded
Wandbound.Hybrid.NonHero.Payment.DeterministicOrdering
Wandbound.Hybrid.NonHero.Payment.NoDuplicatePaymentPlan
Wandbound.Hybrid.NonHero.Cap.AtFourUnitsSacrificeThenSummonLegal
Wandbound.Hybrid.NonHero.Cap.CompletedCountRemainsFour
Wandbound.Hybrid.NonHero.Cap.NoCompletedStateAboveFour
Wandbound.Hybrid.NonHero.Atomic.InvalidSacrificeNoMutation
Wandbound.Hybrid.NonHero.Atomic.InvalidPaymentNoMutation
Wandbound.Hybrid.NonHero.Atomic.InvalidDestinationNoMutation
Wandbound.Hybrid.NonHero.Atomic.StalePlanNoMutation
Wandbound.Hybrid.NonHero.Atomic.AlteredPlanNoMutation
Wandbound.Hybrid.NonHero.Execution.SacrificedCharacterRemoved
Wandbound.Hybrid.NonHero.Execution.HybridCreated
Wandbound.Hybrid.NonHero.Execution.PrintedStatsUsed
Wandbound.Hybrid.NonHero.Execution.HeroIdUnchanged
Wandbound.Hybrid.NonHero.Execution.HybridNotHero
Wandbound.Hybrid.NonHero.Execution.DestinationAdjacentToHero
Wandbound.Hybrid.NonHero.Execution.NewUnitCannotActImmediately
Wandbound.Hybrid.NonHero.Cleanup.HandPaymentDiscardedOnce
Wandbound.Hybrid.NonHero.Cleanup.EquippedPaymentDiscardedOnce
Wandbound.Hybrid.NonHero.Cleanup.RemainingEquipmentDiscarded
Wandbound.Hybrid.NonHero.Cleanup.OtherUnitEquipmentUntouched
Wandbound.Hybrid.NonHero.Cleanup.NoOrphanAttachments
Wandbound.Hybrid.NonHero.Terminal.SacrificeNotDefeat
Wandbound.Hybrid.NonHero.Terminal.MatchRemainsNonterminal
Wandbound.Hybrid.NonHero.Terminal.WinnerUnset
Wandbound.Hybrid.NonHero.Terminal.LoserUnset
Wandbound.Hybrid.NonHero.Terminal.NoHeroLossReason
Wandbound.Hybrid.NonHero.Terminal.OriginalHeroLaterLossStillTerminal
Wandbound.Hybrid.NonHero.Marker.PostSummonResolutionRuns
Wandbound.Hybrid.NonHero.Marker.TrapCanDamageSummonedHybrid
Wandbound.Hybrid.NonHero.Marker.HybridDeathDoesNotDefeatLivingHero
Wandbound.Hybrid.NonHero.Marker.HiddenInfoPreserved
Wandbound.Hybrid.NonHero.ActionId.SamePlanStable
Wandbound.Hybrid.NonHero.ActionId.SacrificeChangesId
Wandbound.Hybrid.NonHero.ActionId.DestinationChangesId
Wandbound.Hybrid.NonHero.ActionId.PaymentChangesId
Wandbound.Hybrid.NonHero.ActionId.HeroReplacementIdsUnchanged
Wandbound.Hybrid.NonHero.Trace.DeterministicOrder
Wandbound.Hybrid.NonHero.Trace.UnitSacrificePresent
Wandbound.Hybrid.NonHero.Trace.SafePaymentPresent
Wandbound.Hybrid.NonHero.Trace.HybridSummonedPresent
Wandbound.Hybrid.NonHero.Trace.NoHeroSacrificeCommitted
Wandbound.Hybrid.NonHero.Trace.NoHeroReplacementCommitted
Wandbound.Hybrid.NonHero.Trace.NoHeroDefeated
Wandbound.Hybrid.NonHero.Trace.NoGameOver
Wandbound.Replay.Hybrid.NonHero.AcceptedActionRecordedOnce
Wandbound.Replay.Hybrid.NonHero.FreshReplaySameSacrifice
Wandbound.Replay.Hybrid.NonHero.FreshReplaySameDestination
Wandbound.Replay.Hybrid.NonHero.FreshReplaySamePayment
Wandbound.Replay.Hybrid.NonHero.FreshReplaySameHero
Wandbound.Replay.Hybrid.NonHero.FreshReplaySameHybrid
Wandbound.Replay.Hybrid.NonHero.StateDigestMatches
Wandbound.Replay.Hybrid.NonHero.TraceDigestMatches
Wandbound.Replay.Hybrid.NonHero.RejectedActionNotRecorded
Wandbound.Hybrid.Regression.HeroReplacementStillAtomic
Wandbound.Hybrid.Regression.HeroReplacementArchiveUnchanged
Wandbound.Hybrid.Regression.HeroReplacementReceiptUnchanged
Wandbound.Hybrid.Regression.HeroReplacementActionIdsUnchanged
Wandbound.Hybrid.Regression.OrdinaryCharacterSummonUnchanged
Wandbound.Hybrid.NonHero.Privacy.PaymentCandidatesHidden
Wandbound.Hybrid.NonHero.Privacy.PaidWandIdentityHiddenFromPublicTrace
Wandbound.Hybrid.NonHero.Privacy.OpponentHandHidden
Wandbound.Hybrid.NonHero.Privacy.ReceiptExactlyEightFields
Wandbound.Hybrid.NonHero.Privacy.StartupJsonUnchanged
Wandbound.Authority.Hybrid.NonHero.CoordinatorOwnsAction
Wandbound.Authority.Hybrid.NonHero.NoRuntimeMutation
Wandbound.Authority.Hybrid.NonHero.NoReplayMutation
Wandbound.Authority.Hybrid.NonHero.NoSmokeDirectStateMutation
Wandbound.Authority.Hybrid.NonHero.NoDeathResolutionForSacrifice
Wandbound.Authority.Hybrid.NonHero.NoActionCodecChange
Wandbound.Authority.Hybrid.NonHero.NoGodotChange
Wandbound.Authority.Hybrid.NonHero.NoMeshyChange
Wandbound.Authority.Hybrid.NonHero.NoAssetOrMapChange
```

Existing Hero tests were updated only for the internal `SacrificedUnitId` field
rename. The combined Hybrid group passed 128 tests and the affected real groups
passed 897.

## Changed Files

All task files were clean at baseline and have no overlap with pre-existing
dirty hunks.

| Path | Purpose | Baseline dirty | Overlap | Impact |
| --- | --- | --- | --- | --- |
| `Source/WandboundCore/Public/WBHybridSummon.h` | Unified plan/API and compatibility wrappers | No | None | Core contract |
| `Source/WandboundCore/Private/WBHybridSummon.cpp` | Unified planning, preflight, atomic execution, traces | No | None | Core behavior |
| `Source/WandboundCore/Public/WBMatchCoordinator.h` | Typed unified Hybrid action marker | No | None | Authority contract |
| `Source/WandboundCore/Private/WBMatchCoordinator.cpp` | Unified planner/executor integration | No | None | Production authority |
| `Source/WandboundRuntime/Public/WBProductionHybridNonHeroSmoke.h` | Packaged smoke result/API | No | None | Validation runtime |
| `Source/WandboundRuntime/Private/WBProductionHybridNonHeroSmoke.cpp` | Production packaged scenario, privacy, replay and zone parity | No | None | Validation runtime |
| `Source/WandboundRuntime/Private/WBProductionHybridReplacementSmoke.cpp` | Internal plan-field rename only | No | None | Hero regression compatibility |
| `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp` | New self-terminating smoke flag | No | None | Packaged validation entry |
| `Source/WandboundRuntime/WandboundRuntime.Build.cs` | Stage isolated fixture | No | None | Packaging |
| `Source/WandboundTests/Private/WBHybridNonHeroSummonTests.cpp` | 88 deterministic tests | No | None | Coverage |
| `Source/WandboundTests/Private/WBHybridHeroReplacementTests.cpp` | Internal plan-field rename | No | None | Existing test compatibility |
| `Data/Replay/HybridNonHeroFixture/root_manifest.json` | Isolated fixture root | No | None | Test data |
| `Data/Replay/HybridNonHeroFixture/bundle_manifest.json` | Isolated fixture manifest | No | None | Test data |
| `Data/Replay/HybridNonHeroFixture/units.json` | Hero, Character, Hybrid, Wand definitions | No | None | Test data |
| `Data/Replay/HybridNonHeroFixture/markers.json` | Isolated marker definitions | No | None | Test data |
| `Data/Replay/HybridNonHeroFixture/match_spec.json` | Deterministic packaged scenario | No | None | Test data |
| `Docs/Non_Hero_Hybrid_Summon_Audit.md` | Pre-code architecture audit | No | None | Documentation |
| `Docs/Non_Hero_Hybrid_Summon_Audit.json` | Machine-readable audit | No | None | Documentation |
| `Docs/Non_Hero_Hybrid_Summon_Report.md` | Final implementation evidence | No | None | Documentation |

No canonical CardDB/deck/match-spec, Config, Godot, Meshy, map, Blueprint,
`.uasset`, `.umap`, networking, AI, save/load, analytics, or LFS-policy file was
changed by this pass.

## Git Status

- Task changes are the files listed above.
- Pre-existing tracked changes remain in Config, CardDB schema reporting,
  activation affordability/cost, game state, public summary, resonance,
  runtime-result serialization, equip declarations/tests, production handoff
  tests, and runtime decision/presentation tests.
- Pre-existing untracked work remains under `Content/Maps/`,
  `Content/MeshyImports/`, `Plugins/meshy/Content/`, two unrelated audit docs,
  `MaxHP`, `RLTotal`, and the existing sentinel-named files.
- Staged files: none.
- LFS: no objects staged or queued; pre-existing `DefaultEditor.ini` working-copy
  state remains untouched.
- Generated package, replay, automation, log, and validation output remains
  ignored and untracked.

No stage, commit, push, clean, reset, restore, checkout, revert, or project-file
deletion was performed.

## Remaining Risks

1. Sacrifice-trigger timing remains unspecified and unsupported.
2. Card-specific summon-trigger timing remains unspecified and unsupported.
3. Simultaneous dual-Hero resolution remains the existing fail-closed case.
4. The top-level packaged Windows launcher still mangles unquoted `.json`
   arguments; validation should continue using the inner executable and
   package-relative paths.

## Recommended Next Task

Add deterministic production turn-state transitions for active player, turn
number, phase, and first-player handling, preserving the completed Hybrid summon
and replay behavior without adding UI or response windows.
