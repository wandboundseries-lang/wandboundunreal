# Persistent Unit Stat Mutation Foundation

## Baseline and scope

Public HEAD and origin/main: `d4f0605c1d758e68c9fd8ac33f0bcd634247da5b`.
Baseline automation: 2,464. No staged files. The three unrelated untracked
paths are preserved. Private inventory was consulted read-only; its exhaustive
contents are not reproduced here.

## Pre-implementation audit matrix

| Mechanic | Stored Stats | Current Implementation | Persistent? | Continuous? | Genericizable Now? | Special Rule |
| --- | --- | --- | --- | --- | --- | --- |
| Sable observer growth | ATK, MaxHP, HP | WBPostDestructionTrigger -> WBUnitStatDelta | Yes | No | Yes | Captured eligibility, live source recheck, ordered observers; not Heal |
| Armor effects | CurrentArmor, MaxArmor | WBArmorEffect | Yes | No | Yes | Seven semantic operations; reductions saturate at zero |
| Explicit Armor pair | MaxArmor, CurrentArmor | New primitive | Yes | No | Yes | +2/+2 uses final cap, independent of entry order |
| Poison application/tick | MaxHP, HP cap | WBStatusEffect, WBEffectRunner | Yes | No | Yes, numeric step only | Min MaxHP 1; retain global tick ownership and status traces |
| Persistent ATK/AR adjustment | ATK, AR | Direct live fields | Yes | No | Yes | Nonnegative stored values, never definition edits |
| Highground and Vex | Effective AR only | WBUnitStatQuery | No | Yes | No | Intrinsic range excludes range-dependent enemy auras |
| Damage/Heal | HP, CurrentArmor | WBDamageResolution, WBHealEffect | No (semantic effects) | No | No | Preserve damage/healing/death events |
| RL recalculation | BaseRL, CurrentRL, RLUsed | WBResonanceRecalculation/Overflow | Mixed | Yes | Deferred | Equipment removal needs zone/definition authority; no raw RL write |
| Inheritance/replacement | RL, equipment, new unit stats | WBCSNInheritance, WBUnitReplacementEffect | Special | No | No | Transfer/new-instance policy is not a stat mutation |
| Summon/death | Initial stats, removed state | WBSummonExecution, WBDeckSummon, WBDeathResolution | Unit lifetime | No | No | New units use definitions, not old modified live fields |

Godot read-only evidence: `scripts/sim/game_state.gd` bump_max_hp,
increase_max_hp, decrease_max_hp and Armor helpers; Sable death handler.
Owner instruction supplies the explicit atomic Armor pair contract. Generic
equipment modify_stat code is not evidence for persistent duration semantics.

## Implemented contract

Typed Add/Set for ATK, AR, CurrentHP, MaxHP, CurrentArmor, MaxArmor.
Compatible Adds aggregate using int64; duplicate Sets and mixed Add/Set reject.
Evaluate against one pre-state, clamp current values against final maxima,
then commit one working unit copy. No zone or definition mutation.
MaxHP increases alone do not heal; MaxArmor increases alone do not refill.
Explicit CurrentHP reductions are deferred (no invented lethal stat behavior).
Armor Add reductions saturate at zero; negative Set is invalid.
RL, resources, counters, continuous modifiers and temporary durations excluded.
Sable/Armor adapters preserve their existing trace contracts.

Poison adapters supply a floor-at-one Set MaxHP request, preserving Frozen
interaction, source capture, duration, global ownership and existing traces.
Damage/Heal/death and initial unit construction retain their separate semantic
authorities; they are not persistent stat effects. Numeric writes remaining in
those paths were reviewed and intentionally not rerouted.

The working copy is one live unit, not the entire game: the operation cannot
change zones, definitions or other units, and commits only after all validation.
This preserves existing caller references to the unit. Generic traces use
`unit_stat_mutated`, `stat_id`, `previous_stat_value`, `new_stat_value` plus the
existing action/source/target context. Fields are omitted from older traces.
The adapters retain their semantic traces rather than appending generic duplicates.
Existing Armor/Poison APIs lack accepted-action IDs; their internal transaction
context derives from operation/target/status/turn, with no new sequence counter.
The caller retains the enclosing accepted action identity. These local identities
are not deduplication keys. Sable retains its event/observer identity exactly.

Event source snapshots are input provenance, not eligibility authority. Only the
public source unit ID enters generic traces; private instance/card metadata is
not serialized. The engine does not infer Caster or declared target from mutation.
Explicit negative CurrentHP requests now reject, including through the legacy
delta adapter. No production caller requires them. MaxHP cap reduction remains
supported, without synthesizing damage/healing/death events.

Armor negative Adds saturate at zero (legacy behavior); negative Sets reject.
Arithmetic is checked before clamping. Effective AR uses wide arithmetic and
saturates at int32 maximum so Highground cannot wrap a valid maximum stored AR.
Continuous aura range remains intrinsic and non-recursive.

RL is excluded from the enum's supported domain; unknown values reject atomically.
BaseRL/CurrentRL/RLUsed, modifier state, available RL, overflow and equipped Wands
remain owned by existing resonance/inheritance code. No transfers, temporary
modifiers, new cards or status ownership changes are introduced.

## Exact changed files

All paths were clean or absent at baseline, with no unrelated overlap.

- `Docs/Unit_Stat_Mutation_Foundation_Audit.md`
- `Docs/Unit_Stat_Mutation_Foundation_Audit.json`
- `Source/WandboundCore/Public/WBUnitStatMutation.h`
- `Source/WandboundCore/Private/WBUnitStatMutation.cpp`
- `Source/WandboundCore/Public/WBReplayTrace.h`
- `Source/WandboundCore/Private/WBReplayTrace.cpp`
- `Source/WandboundCore/Private/WBUnitStatDelta.cpp`
- `Source/WandboundCore/Private/WBArmorEffect.cpp`
- `Source/WandboundCore/Private/WBStatusEffect.cpp`
- `Source/WandboundCore/Private/WBEffectRunner.cpp`
- `Source/WandboundCore/Private/WBUnitStatQuery.cpp`
- `Source/WandboundTests/Private/WBUnitStatMutationTests.cpp`

## Validation

Automation completed against final source: 1,851 affected tests passed and full
suite 2,473 passed (+9), zero failed, warnings or not-run. The nine new focused
tests contain well over 53 assertions, including table-driven scalar/Armor cases.
Existing Sable tests cover captured eligibility, source removal, multiple ordered
observers, Rook continuation, terminal precedence and fresh coordinator replay.
Full-suite matching groups include Sable 9, Armor 52, Damage 112, Heal 27,
Poison 11, Death 41, Resonance 36, Inheritance 9, Vex 14, Geometry/Highground 16,
TriggerSnapshotFoundation 8, PrivateExactInstanceChoice 8, Replay 243,
PublicBoardSummary 19 and Privacy 41 (overlapping filters, not additive).

The initial focused run had one test-expectation error: public summary without
a repository reports intrinsic AR (4 with Highground), not raw stored AR (3).
The expectation was corrected to the existing API. All subsequent tests passed.
No gameplay correction was required for that failure.

Reports: `Saved/AutomationReports/UnitStatMutationAffected/index.json` and
`Saved/AutomationReports/WandboundUnitStatMutation/index.json`.
All six build modes passed with actual compilation:

| Mode | Actions | Seconds | Result |
| --- | ---: | ---: | --- |
| Editor default (final core, then corrected test) | 9 + 4 | 17.97 + 3.95 | Succeeded |
| Game default | 170 | 216.10 | Succeeded |
| Editor non-unity | 135 | 186.70 | Succeeded |
| Game non-unity | 130 | 301.69 | Succeeded |
| Editor forced unity | 10 | 49.98 | Succeeded |
| Game forced unity | 5 | 72.92 | Succeeded |

Non-unity used `-DisableUnity`; true forced unity used
`-ForceUnity -DisableAdaptiveUnity`. Generated unity units containing the changed
core files are retained under `Saved/Validation/UnitStatMutation/EditorUnityUnits`
and `GameUnityUnits`. Existing test-module non-unity policy remains unchanged.
Build logs are under `Saved/Validation/UnitStatMutation/`.

Clean `PackageWandboundLocalPlay.ps1 -CleanBuild` completed build, cook, stage,
package and archive with exit 0 in 828.14 seconds. Log:
`Saved/Logs/PackageWandboundLocalPlay.log`.

Two packaged Sable production smokes passed using the inner executable and
package-relative fixture paths. Each persisted and freshly replayed 28 accepted
records, checked growth twice and private Rook-choice continuation, and exited
normally with status 0. Armor pair and persistent AR composition are covered by
headless tests, not a new production card or a synthetic runtime mutation bypass.
Archive, receipt and startup JSON match the freshly captured prior-package
baseline byte-for-byte and match each other across the two new runs.

| Evidence | SHA-256 / digest (unchanged in both runs) |
| --- | --- |
| Archive | `c85c0086cfc6902e5919d81bf45cbcc3b9a11fabbc5282f988555ef12cc2775b` |
| Receipt | `580e3c5577c002225c752b358986b0cfe4ef844bd5e8e6208bbbccd0d2c1b642` |
| Startup JSON | `c1589b0b9f442008164c71854ffc1ded9dbf77f74160daef9a4ff550f194d99c` |
| State | `64e55c1bce7e969938bc4f3e64561604b7d54a38f4cad55bf810b2a8ca4d0b39` |
| Trace | `1a86686d391a7fa20cc003671461478bf7a5e4357eae772e573d9f9b1b589f52` |
| Replay | `d9ffbcdba36a5740b72873310159d9eb5fedf55a489d56913d894174f30e6661` |

The private archive contains authoritative replay evidence; it is not public
observation. Public receipt/startup scans found no private zone/instance fields
or state/trace digest values. Receipt remains exactly eight fields and schema 1.
Evidence is local under `Saved/Validation/UnitStatMutation/{Baseline,Run1,Run2}`.

## Completion report

### Result
Implemented and validated. No final build, test, package or smoke errors.
### Verified Public Baseline
HEAD and origin/main remain `d4f0605c1d758e68c9fd8ac33f0bcd634247da5b`.
### Private Audit Reference
Read-only local consultation; exhaustive inventory never copied or published.
### Existing Mutation Inventory
See pre-implementation matrix above, separating stored changes from semantic effects and live modifiers.
### Canonical Mutation Authority
WBUnitStatMutation owns persistent stored-stat calculation and atomic commit.
### Supported Stored Stats
ATK, AR, CurrentHP, MaxHP, CurrentArmor, MaxArmor.
### Deferred Stats
BaseRL/CurrentRL and all resource/counter fields remain excluded.
### Operation Types
Typed Add and Set only.
### Atomic Multi-Stat Transactions
One pre-state, final combined bounds, one working-unit commit.
### Duplicate Stat Entry Policy
Aggregate Adds; reject mixed Add/Set and duplicate Sets.
### HP / MaxHP
MaxHP >= 1, no implicit healing, final cap clamps HP. Explicit reductions reject.
### Armor / MaxArmor
Zero floor, final cap; max-only growth does not refill; atomic owner +2/+2 supported.
### ATK
Persistent nonnegative stored ATK, naturally consumed by damage calculations.
### AR
Persistent nonnegative stored AR; never stores a terrain/aura result.
### Stored vs Effective AR
Highground and Vex remain live queries; public no-repository summary is intrinsic AR.
### RL Decision
Deferred rather than bypassing repository-dependent overflow and equipment authority.
### RL Invariants
No mutations to BaseRL, CurrentRL, RLUsed, available RL or equipped Wand state.
### Resonance Modifier Separation
Existing modifier state/recalculation preserved; no transfer primitive.
### Sable Migration
Only downstream mutation changed; captured observer eligibility/live recheck/order unchanged.
### WBUnitStatDelta Compatibility
Delegating adapter with unchanged successful Sable trace; unsupported HP reductions fail closed.
### WBArmorEffect Compatibility
Seven semantic operations delegate; normal results/traces preserved; overflow safely rejected.
### Source / Event Provenance
Captured source accepted without inventing live eligibility; source unit ID preserved.
### Declared Target / Caster
Mutation target does not imply declaration or Caster; those belong to caller context.
### Transactional Failure
No partial stat changes or success traces; full-state digest rejection tests passed.
### Integer Overflow
int64 accumulation; int32 result checked before caps. AR query avoids int32 wrap.
### Unit Lifetime
Only live unit changes; definitions and newly summoned copies retain definition stats.
### Trace
Typed-order generic stat events; adapter traces preserve existing replay bytes.
### Public / Private Boundary
Private source metadata stays out of generic trace; receipt and startup scans passed.
### Replay / Determinism
Fresh coordinator replay and repeated package artifacts match, including baseline bytes.
### WBActionCodec
Unchanged blobs: cpp `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`, header `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
### Replay Schema
1, unchanged.
### Public Receipt
Eight fields, unchanged.
### Focused Tests
Nine new tests, more than 53 assertions; 1,851 affected tests passed.
### Full Automation
2,473 passed, +9; zero failed, warnings or not-run.
### Build Matrix
All six modes passed; exact counts/times listed above.
### BuildCookRun
Clean final-source build/cook/stage/package/archive succeeded, exit 0.
### Packaged Smoke
Two successful Sable runs, 28 records, generation/revision 1/29, normal process exit.
### Exact Changed Files
12 paths listed above, all clean/absent at baseline with no unrelated overlap.
### Public Audit Manifest Equality
Markdown/JSON path sets match the worktree task set exactly.
### Sensitive Audit Exposure Check
No sensitive audit path in main or staged index; no inventory publication.
### Private Branch Integrity
The private audit reference was consulted read-only and remains isolated from public main.
### Unrelated Files Preserved
`Content/Maps/NewProjectTest.umap`, `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`, `h origin main` untouched.
### Deferred Features
RL transfer, continuous/temporary modifiers, new production cards, zone composition and lethal stat reductions.
### Remaining Ambiguities
Direct RL mutation requires integrated overflow policy; explicit HP reduction requires death semantics. Global status ownership unchanged pending owner ruling.
### Readiness
Ready; zero staged files. No commit or push performed.

P0 GENERIC UNIT STAT MUTATION FOUNDATION READY
