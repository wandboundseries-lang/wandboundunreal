# CSN Rook Production Transfer Audit

## Result

CSN Rook is implemented as production CardDB data backed by generic destruction,
mandatory private choice, exact Deck-instance summon, and reusable CSN Inheritance
services. No production rules service branches on Rook's CardId. The implementation
preserves replay schema 1 and leaves `WBActionCodec` byte-identical to baseline.

## Baseline

- Required and verified `HEAD`: `62a57c809001723b834b6ad0f958a85788720a6d`
- Required and verified `origin/main`: `62a57c809001723b834b6ad0f958a85788720a6d`
- Commit: `Add production CSN Undertow Archivist trigger`
- Baseline automation: 2,321 succeeded, 0 failed, 0 warnings, 0 not-run
- Baseline dirty files: `Content/Maps/NewProjectTest.umap` and
  `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`, both untracked
- Baseline staged files: none
- Replay schema: 1

## Canonical Card Definition

Production `char_csn_rook` is a CSN Character with HP 16, ATK 3, AR 2, and
RL 2. Its public text contains the full CSN Inheritance rule and the mandatory
self-destruction Deck-summon rule. The data definition declares one generic
`after_unit_destroyed_triggers` entry with `destroyed_self`,
`summon_character_from_deck_to_destroyed_tile`, CSN faction filtering, one
mandatory summon, ordinary-condition bypass, and CSN Inheritance enabled.

The tracked June Godot snapshot and the baseline temporary Unreal definition both
used RL 3. The later owner-issued balance value RL 2 is authoritative and is now
used by production data and tests.

## Godot Behavior Audited

Read-only sources inspected:

- `Reference/GodotProject/godotcanon/scripts/data/CardDB/characters.json`
- `Reference/GodotProject/godotcanon/scripts/sim/death_triggers/death_trigger_setup.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/death_triggers/handlers/csn_rook_handler.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/tools/ci/smoke_sim.gd`

They establish a self-only trigger, the vacated tile, a controller Deck search,
Character-only eligibility, cap enforcement, direct placement, exact removal of
the selected Deck entry, and CSN Inheritance from the source snapshot. The audited
path contains no explicit shuffle. Godot files were not modified, compiled, or
loaded by Unreal.

## Destruction Event Boundary

`FWBUnitDestructionSnapshot` is captured before cleanup and published only after
death commits. It contains the stable event ID, destroyed unit and definition IDs,
controller, last tile, Hero flag, cause, BaseRL/CurrentRL/RLUsed snapshots, exact
equipped Wand entries, passive eligibility, resolution order, and trigger cursor.
Faction eligibility remains definition-owned and is resolved from the snapshotted
CardId; duplicating faction data in the event was not required.

The supported causes are battle damage, effect damage, status damage, explicit
Destroy, and replacement-effect destruction. Mere board removal and sacrifice-like
removal do not publish events. The existing prevention boundary remains before
publication; no Juno behavior was added. Hero death commits terminal state and the
post-destruction queue is cleared without opening a choice.

Events are stable-sorted by resolution order, destroyed unit ID, then event ID.
Triggers on one definition are sorted by trigger ID. The coordinator advances the
queue at legal action/effect boundaries, never inside a parent transaction.

## Generic Trigger and Choice

`WBPostDestructionTrigger` reads definition data and supports only the minimum
validated operation required by this pass. Unsupported scopes, operations, counts,
optional flags, or inheritance flags fail closed. Negated, Stunned, and Frozen use
the shared `WBCharacterPassiveEligibility` policy and emit a suppressed trace.

If no legal Deck card, valid tile, empty tile, or unit-cap space exists, the
mandatory trigger resolves without a summon or player choice. Whenever at least
one legal Deck candidate exists, the controller receives a private
`MandatoryDeckChoice` and must explicitly choose an exact legal card instance. A
single legal candidate is still presented as a mandatory choice; two or more
candidates use the same choice flow. Exact eligible instances, including duplicate
copies, remain distinct and are ordered by Deck `ZoneIndex`, then `InstanceId`.
Only the controlling player's observation receives `MandatoryDeckChoice` actions.
Submission is revalidated by the coordinator and recorded under the generic replay
family `mandatory_deck_choice`.

The private replay records the exact authoritative action ID. Opponent public
observation, public zones, traces, and the receipt do not expose Deck candidates,
the selected exact instance before summon, Deck order, or private digests.

## Deck Summon and Inheritance

`WBDeckSummon::SummonExactCharacterToTile` verifies the exact instance remains in
the controller's Deck, resolves its production definition, requires Character and
the configured faction, rejects Hybrid, validates the captured tile, emptiness,
unit cap, zone state, and terminal boundary, then uses a working-copy transaction.

The operation bypasses Hero adjacency, normal summon-action use, and player-driven
placement. It does not bypass one-unit-per-tile, ownership, type/faction, cap,
terminal, or zone invariants. The selected exact instance is removed once through
`WBCardLifecycle::RemoveExactCardFromDeck`. Remaining Deck entries retain their
relative order and are canonically reindexed; no shuffle was added.

`WBCSNInheritance` is now the shared mutation used by both Crash-In and Rook. Rook
recovers each snapshotted Wand from the controller's Discard, requiring the exact
owner, instance, CardId, and kind. It never recreates or substitutes a Wand.
Missing Wands fail the trigger transaction without undoing the historical death.

The summoned Character starts from printed stats. Its BaseRL becomes printed RL
plus destroyed-source CurrentRL. Canonical recalculation derives CurrentRL and
RLUsed, then existing deterministic overflow runs. Zero CurrentRL and zero Wands
still constitute successful inheritance. `AfterCSNInheritance` runs once.

## Composition and Ordering

- Rook to real Undertow: exact Deck instance summoned on the old tile, exact Wand
  recovered, BaseRL 4/CurrentRL 4/RLUsed 1, and one private Undertow draw.
- Crash-In destroying Rook: Crash-In finishes atomically first. Its replacement
  occupies the old tile; queued Rook resolution then fails closed without overwrite,
  duplicate inheritance, or rollback.
- Combat: a summoned unit is not a retroactive defender or attacker, cannot counter
  for Rook, does not cause another declaration, and consumes no extra attack budget.
- Hero Rook: terminal Hero loss wins precedence; no choice or replacement Hero is
  created.

## Traces and Replay

The generic path emits `post_destruction_triggered`,
`post_destruction_trigger_suppressed`, `post_destruction_trigger_resolved`, or
`post_destruction_trigger_failed`, plus `post_destruction_deck_summon`, existing
`inherited_wand_transferred`, `csn_inheritance`, and existing
`AfterCSNInheritance` traces. Private state digest serialization includes the
destruction queue and pending mandatory choice.

Replay schema remains 1. Receipt shape remains exactly eight fields. Rejected or
stale choices do not enter accepted-action replay. Fresh replay reproduced the
same generation, revision, final state digest, and final trace digest.

## Tests

Focused Rook automation contains 11 tests and 154 named assertions covering
production data, all destruction causes, publication timing, suppression, exact
Deck choices, duplicate instances, privacy, stale choices, transactional summon,
printed stats, no shuffle, bounds, cap and one-for-one replacement, zero-value
inheritance, overflow, Undertow composition, missing-Wand atomicity, occupied-tile
ordering, combat, Hero terminal behavior, alternate IDs, deterministic queues,
coordinator authority, replay, receipt privacy, and production smoke.

- Focused `Wandbound.CSNRook`: 11 succeeded, 0 failed, 0 warnings, 0 not-run
- Combined Rook/Crash-In/Undertow regression: 34 succeeded before the final edge
  test; the final full suite includes all three families
- Final full `Wandbound`: 2,332 succeeded, 0 failed, 0 warnings, 0 not-run

## Build Matrix

All six requested modes succeeded on the final source:

| Target | Mode | Result | Time |
| --- | --- | --- | --- |
| WandboundUEEditor | non-unity | Succeeded | 229.96 s |
| WandboundUE | non-unity | Succeeded | 116.77 s |
| WandboundUEEditor | default/adaptive | Succeeded | 38.15 s |
| WandboundUE | default/adaptive | Succeeded | 65.25 s |
| WandboundUEEditor | forced unity | Succeeded | 55.30 s |
| WandboundUE | forced unity | Succeeded | 88.24 s |

A prior true forced-unity `-Rebuild` also succeeded for Editor in 199.81 s and
Game in 248.36 s after service-local helper names fixed the collisions that this
validation intentionally exposed.

## Package and Determinism

Final clean BuildCookRun succeeded in 717.55 s (AutomationTool 12m03s), with a
full cook, stage, package, and archive. The final packaged inner executable was
run twice with the local-play map and package-relative production CardDB/Rook
fixture paths. Both runs exited 0 and no packaged process remained.

| Artifact | Run 1 SHA-256 | Run 2 SHA-256 |
| --- | --- | --- |
| Replay archive | `bbc8ae9b83875831ce5c2ede0a39c0fac9089d29dd444ddf4e7e6eee1f55c43e` | same |
| Eight-field receipt | `30162dedc3b2345bd89d6558c5e9827944215c1d42bce010334b0931b7b37566` | same |
| Startup JSON | `765b5ff3ea6a74f83c1365407dd910d9550f60d61f309fbd3d5568d3b1ea3dc2` | same |

- Replay records: 15
- Final state digest: `f0c029604f5a656b4a58c1effdb03023167bc3366caebb26a09b93d668210c97`
- Final trace digest: `feaf192fb00ffa999ae556890d7202dd00a107e13383f3abba6d4c26d479d376`
- Production bundle digest: `49278d2c07ead256bf7e01e1e3b59fb52dc8c43087bccb894d97398a89fe8486`
- Receipt privacy scan: no card-instance pattern, `state_digest`, or
  `trace_digest`

## WBActionCodec

No codec file changed. Baseline and current Git blob hashes are identical:

- `Source/WandboundCore/Public/WBActionCodec.h`:
  `44ef87156beb5799066c2a5ecbc98f04928d98c0`
- `Source/WandboundCore/Private/WBActionCodec.cpp`:
  `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`

## Changed Files

All implementation files were clean at baseline and contain only this pass:

- `Data/CardDB/Production/CSNCrashIn/cards.json`: real Rook stats/text/trigger
- `Data/CardDB/ProductionCardDB.schema.json`: generic trigger schema
- `Data/Replay/CSNRookFixture/match_spec.json`: isolated production smoke fixture
- `Data/Replay/CSNCrashInFixture/match_spec.json`: updated bundle digest pin
- `Data/Replay/CSNUndertowArchivistFixture/match_spec.json`: updated digest pin
- `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`: parse, validate,
  and digest generic trigger data
- `Source/WandboundCore/Public/WBCardDefinition.h`: typed trigger definition
- `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp`: fail-closed
  definition validation
- `Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp`: fixture parser
- `Source/WandboundCore/Public/WBGameStateData.h` and
  `Private/WBGameStateData.cpp`: destruction snapshot and mandatory choice state
- `Source/WandboundCore/Public/WBDeathResolution.h` and
  `Private/WBDeathResolution.cpp`: capture/publish boundary and explicit Destroy
- `Source/WandboundCore/Public/WBEffectRunner.h` and
  `Private/WBEffectRunner.cpp`: cause-aware canonical death calls
- `Source/WandboundCore/Private/WBMarkerResolution.cpp`: effect-damage cause
- `Source/WandboundCore/Public/WBCharacterPassiveEligibility.h` and
  `Private/WBCharacterPassiveEligibility.cpp`: shared suppression policy
- `Source/WandboundCore/Public/WBCardLifecycle.h` and
  `Private/WBCardLifecycle.cpp`: exact Deck removal/reindex
- `Source/WandboundCore/Public/WBCSNInheritance.h` and
  `Private/WBCSNInheritance.cpp`: reusable transactional inheritance
- `Source/WandboundCore/Private/WBCSNInheritanceTrigger.cpp`: shared eligibility
- `Source/WandboundCore/Public/WBDeckSummon.h` and
  `Private/WBDeckSummon.cpp`: exact-instance effect summon
- `Source/WandboundCore/Public/WBPostDestructionTrigger.h` and
  `Private/WBPostDestructionTrigger.cpp`: generic event/trigger/choice resolver
- `Source/WandboundCore/Private/WBUnitReplacementEffect.cpp`: shared inheritance
  and deferred destruction publication
- `Source/WandboundCore/Public/WBMatchCoordinator.h` and
  `Private/WBMatchCoordinator.cpp`: sole mandatory-choice authority
- `Source/WandboundCore/Private/WBProductionMatchReplay.cpp`: private state digest
- `Source/WandboundRuntime/Public/WBProductionCSNCrashInSmoke.h`,
  `Private/WBProductionCSNCrashInSmoke.cpp`, and
  `Private/WBRuntimeMatchBootstrapActor.cpp`: production Rook smoke
- `Source/WandboundRuntime/WandboundRuntime.Build.cs`: stage Rook fixture
- `Source/WandboundTests/Private/WBCSNRookTests.cpp`: focused 154-assertion suite
- `Source/WandboundTests/Private/WBProductionMatchReplayTests.cpp`: generic replay
  family coverage
- `Docs/CSN_Rook_Production_Transfer_Audit.md` and `.json`: this audit

## Preserved and Excluded

The pre-existing untracked `Content/Maps/NewProjectTest.umap` and
`Plugins/meshy/Content/Materials/M_MeshyPBR.uasset` remain untouched. No Config,
Meshy import, Godot, map, Blueprint, `.uasset`, `.umap`, LFS policy, codec, replay
schema, or unrelated CSN card was changed. Nothing was staged, committed, or
pushed.

## Remaining Limitations

- Only the `DestroyedSelf` mandatory one-Character Deck-summon operation is
  production-supported; future Echo/Sonia/Sable scopes need separate canon passes.
- Juno belongs at the existing pre-destruction prevention boundary and remains
  unimplemented.
- Multiple mandatory destruction choices resolve serially; no UI was added.

## Readiness

The production transfer and its generic infrastructure satisfy the requested
determinism, replay, privacy, build, package, and source-control boundaries.
