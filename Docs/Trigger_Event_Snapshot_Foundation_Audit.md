# Trigger and Event Snapshot Foundation Audit

## Result

The production rules core now composes narrow typed event identity, source, and
unit-participant snapshots into existing event-specific contexts. This is not a
universal event payload and does not retain an unbounded event history.

## Event-family audit

| Event/Trigger | Existing context | Historical facts | Live facts retained | Eligibility policy |
| --- | --- | --- | --- | --- |
| Destruction self | `FWBUnitDestructionSnapshot` | event/unit/card, Owner, Controller, tile, Hero, RL, equipment | destination and Deck environment | snapshot at collection |
| Destruction observer | `FWBPostDestructionObserverSourceSnapshot` | observer identity, Owner, Controller, tile, order | source presence, card identity, Controller, passive eligibility | hybrid |
| AfterDamage | `FWBAfterDamageEventContext` | participant roles, Owner, Controller, card, tile, damage, HP, substitution, declaration | target mutation; nullable live effect handle | snapshot at collection |
| PreDamage | `FWBPendingAttackState` | attack identity, declared attacker/defender | current redirected defender, pending hit, passive/usage eligibility | live continuation with typed identity |
| Turn start | `FWBTurnStartTriggerInstance` | trigger/source identity, Owner, Controller, card, tile | source presence, card identity, Controller, passive eligibility | hybrid |
| Setup summon | local collected trigger | listener and summoned participant snapshots | draw mutation only | snapshot at collection |
| Inheritance | `FWBCSNInheritanceEventContext` | transaction, source and inheriting participants, transferred RL/Wands | inheriting unit presence, identity, Controller, passive eligibility | hybrid |
| Activated continuation | `FWBActivatedEffectSourceSnapshot` | action/frame, source, Owner, Controller, tile, Hero, RL, equipment | Deck and destination environment | snapshot at collection |
| Status | `FWBStatusSourceProvenance` | status source authority and origin | status-specific continuation only | existing status authority retained |
| Marker/NPC spawn | `FWBPendingNPCSpawnState` | spawn identity, marker, triggering participant, origin, order, turn | spawn environment | snapshot with live environment |

## Shared primitives

- `FWBEventIdentitySnapshot` carries deterministic event kind, ID, turn,
  existing action/continuation identity, and declaration provenance.
- `FWBEventSourceSnapshot` carries source unit/card/instance, separate Owner and
  Controller, source tile, Hero identity, and activation provenance.
- `FWBUnitParticipantSnapshot` carries the narrow historical unit facts shared
  by attack, damage, destruction, summon, inheritance, and NPC contexts.
- `EWBTriggerEligibilityPolicy` states whether a family is captured, live, or
  hybrid. Each family keeps its own payload and resolution rules.

## Semantic boundaries

Historical event facts are never reconstructed from later board state. Live
lookups remain only where the existing family requires a source or environment
to remain legal. AfterDamage triggers preserve their captured source even when
lethal cleanup removes the source; the generic effect request receives a live
unit handle only when one still exists.

Owner and Controller are captured independently. Caster is derived only from
explicit activation provenance. Status provenance cannot prove that a unit cast
an activation, so its adapter remains resolution-only and never invents Caster.

Existing deterministic action IDs, continuation IDs, transaction IDs, ordering,
trace fields, replay schema 1, and the eight-field public receipt are unchanged.
Legacy scalar fields remain as compatibility mirrors and retain their existing
digest participation; the typed fields deterministically mirror those same
facts and do not change public serialization.

## Tests and validation

- New focused suite: 8 succeeded, 0 failed, 0 warnings.
- AfterDamage regression suite: 35 succeeded, 0 failed, 0 warnings.
- Full `Wandbound` suite: 2,456 succeeded, 0 failed, 0 warnings, 0 not run.
- Baseline: 2,448; net increase: 8.
- Editor and Game builds passed in non-unity, default-unity, and forced-unity
  configurations.
- Clean BuildCookRun completed build, cook, stage, package, and archive.
- The packaged production AfterDamage smoke passed twice through the local-play
  map with package-relative fixture paths and process exit code 0.
- Repeated packaged artifacts were byte-identical: archive
  `0c4885788b98e938138b54a00b0ecc533eba13067c6ac068286cc5a72697e153`,
  receipt `040b15010129044d50bc6cac47bf8f6fc6457ef36cdaae123ae156679770ab61`,
  and startup JSON
  `d741233fc8d65646aa6862237dc197bae60164d926c61469ee716eb4b4f73616`.
- Fresh replay verification produced replay digest
  `d352fdf71ff94f607f75f18dfb2203395e2f429efa24767a1e4d14b617ac2c6c`,
  final state digest
  `10c7cdc8cdd620d728ca6247471b0f83bfff304be491fd915fe07d76ce85ce79`,
  and final trace digest
  `20ec88c84fc98e0716af3effb0554a1074f23c605728e4fb288015989dc7d083`.
- Replay schema remains 1. The public receipt remains exactly eight fields and
  its privacy scan found no protected digest, continuation, hidden-instance,
  fixture-path, or filesystem-path values.
- A pre-existing unity-only smoke helper collision was exposed by recompilation;
  one file-local helper was renamed with no behavior change.

## Exact changed-file manifest

1. `Docs/Trigger_Event_Snapshot_Foundation_Audit.json`
2. `Docs/Trigger_Event_Snapshot_Foundation_Audit.md`
3. `Source/WandboundCore/Private/WBActivatedDeckSummonContinuation.cpp`
4. `Source/WandboundCore/Private/WBAfterDamageTrigger.cpp`
5. `Source/WandboundCore/Private/WBCSNInheritance.cpp`
6. `Source/WandboundCore/Private/WBCSNInheritanceTrigger.cpp`
7. `Source/WandboundCore/Private/WBDeathResolution.cpp`
8. `Source/WandboundCore/Private/WBDeckSummon.cpp`
9. `Source/WandboundCore/Private/WBEffectRunner.cpp`
10. `Source/WandboundCore/Private/WBEventSnapshot.cpp`
11. `Source/WandboundCore/Private/WBInitialHeroSetup.cpp`
12. `Source/WandboundCore/Private/WBMandatoryDeckChoice.cpp`
13. `Source/WandboundCore/Private/WBMarkerResolution.cpp`
14. `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
15. `Source/WandboundCore/Private/WBPostDestructionTrigger.cpp`
16. `Source/WandboundCore/Private/WBTurnStartSequence.cpp`
17. `Source/WandboundCore/Private/WBUnitReplacementEffect.cpp`
18. `Source/WandboundCore/Public/WBAfterDamageTrigger.h`
19. `Source/WandboundCore/Public/WBCSNInheritance.h`
20. `Source/WandboundCore/Public/WBCSNInheritanceTrigger.h`
21. `Source/WandboundCore/Public/WBEventSnapshot.h`
22. `Source/WandboundCore/Public/WBGameStateData.h`
23. `Source/WandboundCore/Public/WBTurnStartSequence.h`
24. `Source/WandboundRuntime/Private/WBProductionStatusAuthoritySmoke.cpp`
25. `Source/WandboundTests/Private/WBTriggerEventSnapshotFoundationTests.cpp`

All listed files were clean at baseline. No Config, Godot, Meshy, map, Blueprint,
asset, action-codec, replay-schema, or public-receipt file is part of this pass.
