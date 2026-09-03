# Private Exact-Instance Choice Foundation Audit

## Scope

This pass consolidates private exact-card-instance selection without making the
selection layer responsible for downstream gameplay mutation. The public
baseline was `792674a638b6b5549a24632bda32e7ec89315688`. A local private audit was
consulted only for capability requirements; no private inventory, blocked-card
list, or unpublished mechanic ledger is reproduced here.

## Existing-System Matrix

| Mechanic | Zone | Timing | Exact instance | Filter | Requirement | One candidate | Declared target | Result handler |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Rook | Deck | Resolution continuation | Yes | Character + CSN | Mandatory when non-empty | Explicit choice | Yes | Post-destruction summon |
| Patch | Deck | Resolution continuation | Yes | Character + CSN | Mandatory when non-empty | Explicit choice | Yes | Activated summon + inheritance |
| Crash-In | Hand | Activation declaration | Yes | Character + required faction | Required by activation | Selected before declaration | Yes | Unit replacement |
| Synthetic coverage | Discard | Resolution continuation | Yes | Typed definition metadata | Descriptor-controlled | Explicit choice | Descriptor-controlled | Test sink only |

## Architecture

`FWBPrivateCardChoiceDescriptor` is the single typed description for a
single-instance private choice. It records a deterministic choice ID, choosing
player, exact source zone, activation-versus-continuation timing, mandatory or
optional semantics, declared-target provenance, continuation kind, stable
action/effect context, typed definition filters, frozen exact instance IDs, and
the priority/phase resume snapshot.

`WBPrivateCardChoice` provides deterministic enumeration, freezing, and
submission-time validation for Deck, Hand, and Discard. Enumeration follows
authoritative zone order and uses exact `CardInstanceId`; duplicate `CardId`
copies remain separate candidates. Unsupported zones and ownership mismatches
fail closed. Pending choices use a frozen exact identity set, reject newly
entered candidates, and revalidate that the selected instance still occupies
the required zone and still satisfies immutable definition constraints.

The game state has one authoritative pending private-choice state. Temporary
compatibility aliases and the established member name normalize to that same
state; there is no parallel Deck-specific gameplay truth. Event-specific data
remains in typed post-destruction and activated-effect continuation payloads.

## Continuation and Mutation Boundaries

The generic layer validates one exact private instance and dispatches by typed
continuation kind. Post-destruction and activated-effect handlers retain their
existing mutation authority. Invalid submissions run against a working state
and do not commit partial zone movement, summons, inheritance, Wand release,
usage changes, or priority changes.

Rook preserves its automatic trigger and destruction snapshot. The selected
Deck card is player-declared, while the former tile is fixed. Patch preserves
declared activation, pre-choice sacrifice, source/owner/controller/tile/RL and
detached-Wand snapshots, explicit single-candidate selection, no shuffle, CSN
Inheritance, terminal Hero behavior, and exact priority/phase resume. Crash-In
keeps exact Hand selection at activation declaration; it does not gain a later
continuation.

## Privacy Boundary

Choosing-player legal-action generation may contain exact private selection
IDs. Viewer-scoped coordinator enumeration and public observations return no
pending private-choice actions to an opponent. Public board summaries do not
contain private zones, candidate identities, frozen sets, Deck indices, or
protected state/trace digests. Existing public trace translation remains the
authority for protected fields. Public receipts remain exactly eight fields;
accepted replay remains authoritative and private.

## Compatibility

- `WBActionCodec` is unchanged.
- Production replay schema remains version 1.
- Existing mandatory Deck action IDs remain compatible.
- Existing Rook, Patch, and Crash-In result behavior is preserved.
- No Godot, Config, map, asset, Blueprint, Meshy, CardDB, or status-tick file is changed.

## Validation

- Dedicated `Wandbound.PrivateExactInstanceChoice`: 8 tests, 73 assertions, 0 failures.
- Focused `Wandbound.CSN`: 85 succeeded, 0 failed.
- Full `Wandbound`: 2,464 succeeded, 0 failed, 0 not run; baseline 2,456, delta +8.
- Editor default: succeeded, 185 actions, 390.08 seconds.
- Game default: succeeded, 174 actions, 447.76 seconds.
- Editor non-unity: succeeded, 138 actions, 297.39 seconds.
- Game non-unity: succeeded, 128 actions, 221.21 seconds.
- Editor forced-unity with adaptive unity disabled: succeeded, 205 actions, 318.33 seconds.
- Game forced-unity with adaptive unity disabled: succeeded, 180 actions, 243.72 seconds.
- Clean BuildCookRun: succeeded, 552.00 seconds; build, cook, stage, package, and archive completed.
- Packaged Patch and Crash-In production smokes each passed twice with exit status 0, fresh replay verification, and byte-identical archive/receipt/startup artifacts.
- Patch final state/trace/replay digests: `f8cc43a4083540a3b61b7b6482709bf67fd53f3574194c5e5845e0034281cb2f`, `6a601eb2ef6f77e709ffda30c91a02dec092916b7a012da10e68f62e6eb6a634`, `b359c128414b96389ba6facb71ae2d6f938abe0aaa327411fcdb8e3ee01f017a`.
- Crash-In final state/trace/replay digests: `a274b44a9894faf29609a5945c747a27d3ef663ad59055a31926125819b52a0b`, `cdde411bd1959842bc43307087ad07df962b0d8a459732463adb0cce58d91bea`, `29ef598251771ce419b74bd839a52ac50d639335cc72cc93815ddad6bb658714`.
- Packaged public-output scan found no protected digest, zone-index, frozen-candidate, or private candidate-instance field.

## Deferred

Broad zone mutation, choose-N, optional-choice passing, new card transfers,
generic inheritance, broad stat/summon/destruction composition, status tick
ownership changes, AI, and telemetry remain out of scope.

## Exact Changed-File Manifest (19)

1. `Docs/Private_Exact_Instance_Choice_Foundation_Audit.json`
2. `Docs/Private_Exact_Instance_Choice_Foundation_Audit.md`
3. `Source/WandboundCore/Private/WBActivatedDeckSummonContinuation.cpp`
4. `Source/WandboundCore/Private/WBGameStateData.cpp`
5. `Source/WandboundCore/Private/WBMandatoryDeckChoice.cpp`
6. `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
7. `Source/WandboundCore/Private/WBPostDestructionTrigger.cpp`
8. `Source/WandboundCore/Private/WBPrivateCardChoice.cpp`
9. `Source/WandboundCore/Private/WBProductionMatchReplay.cpp`
10. `Source/WandboundCore/Private/WBUnitReplacementEffect.cpp`
11. `Source/WandboundCore/Public/WBGameStateData.h`
12. `Source/WandboundCore/Public/WBMandatoryDeckChoice.h`
13. `Source/WandboundCore/Public/WBMatchCoordinator.h`
14. `Source/WandboundCore/Public/WBPostDestructionTrigger.h`
15. `Source/WandboundCore/Public/WBPrivateCardChoice.h`
16. `Source/WandboundTests/Private/WBCSNPatchTests.cpp`
17. `Source/WandboundTests/Private/WBCSNRookTests.cpp`
18. `Source/WandboundTests/Private/WBCSNSableTests.cpp`
19. `Source/WandboundTests/Private/WBPrivateExactInstanceChoiceTests.cpp`
