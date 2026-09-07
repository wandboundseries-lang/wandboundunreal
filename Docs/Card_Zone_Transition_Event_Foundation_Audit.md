# Card Zone Transition Event Foundation Audit

## Scope

This pass adds immutable event facts for successful exact-instance transfers
among the authoritative ordered private Deck, Hand, and Discard zones. It does
not add cards that consume those events.

## Existing Zone Mutation Architecture

WBCardZoneMutation::TransferExact remains the atomic structural mutation
authority. It validates the exact card instance, same-owner transfer, ordered
source and destination zones, append placement, and final zone invariants.
ExtractExact remains a destination-free structural primitive.

## Structural And Semantic Event Boundary

Low-level zone mutation emits no gameplay event. WBCardLifecycle performs a
transfer on a working copy, constructs a transition snapshot only after the
structural mutation succeeds, and commits state with that snapshot as one
transaction. Semantic callers decide whether to publish the returned fact as a
redacted trace. Failed outer transactions discard both working state and facts.

## Transition Snapshot

FWBCardZoneTransitionSnapshot records an FWBEventIdentitySnapshot, exact internal
CardInstanceId, authoritative CardId and owner, source and destination zones,
typed cause, resulting zone counts, and deterministic resolution order. It
records no zone indexes, candidate alternatives, or unrelated private cards.
Snapshots are value facts and remain unchanged after later movement.

## Event Identity

EWBEventKind::CardZoneTransition is appended after all existing event kinds. Its
internal ID is derived deterministically from turn, owner/player, resolution
order, source and destination zones, and exact instance. Source action,
continuation, and declaration provenance use the existing event identity type.
No GUID, clock, pointer, or random value is used.

## Causes And Provenance

The narrow typed cause set is Unknown, Draw, Effect, Cost, Rule, and Setup.
Turn-start draws publish Draw. Trigger-driven draws publish Effect. Explicit
Discard and Hand activation-cost transfers publish player-declared Rule and Cost
facts respectively. Setup draws explicitly return no transition events.

## Exact-Instance Authority

CardInstanceId is authoritative internally. Duplicate CardIds remain distinct:
moving one copy records that exact instance and leaves the other copy untouched.
Ownership is copied from the immutable card instance and validated against the
same-player ordered-zone transfer.

## Supported Zones

The semantic lifecycle wrapper supports exactly:

- Deck to Hand
- Deck to Discard
- Hand to Deck
- Hand to Discard
- Discard to Deck
- Discard to Hand

Append ordering and existing no-shuffle behavior are preserved.

## Caller Publication Matrix

| Caller | Source | Destination | Publish now | Cause | Atomic parent |
| --- | --- | --- | --- | --- | --- |
| Turn-start draw | Deck | Hand | Yes | Draw | Turn-start working state |
| Turn-start trigger draw | Deck | Hand | Yes | Effect | Turn-start working state |
| CSN Inheritance draw | Deck | Hand | Yes | Effect | Trigger working state |
| Explicit Discard action | Hand | Discard | Yes | Rule | Coordinator working state |
| Hand activation cost | Hand | Discard | Yes | Cost | Coordinator working state |
| Generic lifecycle transfer | Supported pair | Supported pair | Returned to caller | Explicit or Unknown | Caller-owned |
| Setup draw | Deck | Hand | No | Setup | Setup working state |
| Hybrid payment | Hand | Discard | No publication | Deferred | Hybrid transaction |
| ExtractExact and summon extraction | Ordered zone | None | No | Not applicable | Existing authority |
| Equipped cleanup | Equipped | Discard | No | Deferred | Existing lifecycle |

The compatibility activation execution handoff remains test-only and is not a
production publication authority. The coordinator remains the sole production
action authority.

## Draw Integration

DrawOneCard returns one Deck-to-Hand transition after a successful commit.
DrawCards(N) retains sequential partial-commit semantics and returns one fact per
successful draw in draw order. If a later draw fails, earlier successful facts
remain and no false event is created for the failed draw.

## Extraction, Equipped, And Board Exclusions

ExtractExact, Character summon extraction, Rook, Patch, Crash-In, initial Hero
setup, Hybrid payment, Equipped-to-Discard cleanup, Board transitions, markers,
NPCs, and destruction cleanup do not publish generic CardZoneTransition events.
Existing summon, destruction, sacrifice, replacement, and equipment authorities
remain distinct.

## Privacy

Exact instance and CardId remain protected in the internal snapshot. The
redacted trace contains only event kind, owner/player, source zone, destination
zone, count, typed cause, turn, order, and success. It contains no instance,
CardId, zone index, ordered-zone contents, alternatives, state digest, or trace
digest. Public board summaries are unchanged and replay receipts remain exactly
eight fields.

## Replay And Determinism

Transition traces use the existing deterministic trace serializer and digest.
No permanent event history is added to game state, no continuation is opened,
and no random stream is consumed. Replay schema remains 1 and WBActionCodec is
unchanged.

## Validation

- Focused: 6 succeeded, 0 failed; 144 explicit assertions.
- Required affected groups: 95 succeeded, 0 failed.
- Full Wandbound automation: 2,509 succeeded, 0 failed, 0 not run; +6 from 2,503.
- Editor non-unity: succeeded.
- Game non-unity: succeeded.
- Editor default unity: succeeded.
- Game default unity: succeeded.
- Editor forced unity: succeeded.
- Game forced unity: succeeded.
- Clean BuildCookRun: succeeded; cook reported 0 errors and 0 warnings.
- Transition packaged smoke: two successful byte-identical runs, 6 transitions.
- Discard activation packaged regression: two successful byte-identical runs.
- Summon-negation packaged regression: two successful byte-identical runs.

Transition receipt SHA-256:
fd5bf90940652eb1801c59ff30d6de0d68723fe621175e2966dc98aa27c1b674

Transition final replay digest:
656b970057939cf7055f14b1101b49fce6d135ef84db756594a622d526d0a0b6

## Explicit Deferrals

Production zone-resident trigger scanning, trigger cards, Discard-to-Board,
Board-to-Discard, Equipped transitions, prevention, replacement, negation,
ownership transfer, cross-player moves, shuffle, Deck activation, new reaction
windows, choose-N, optional choice passing, AI, and telemetry remain deferred.

## Exact Changed Files

1. Docs/Card_Zone_Transition_Event_Foundation_Audit.json
2. Docs/Card_Zone_Transition_Event_Foundation_Audit.md
3. Source/WandboundCore/Private/WBCSNInheritanceTrigger.cpp
4. Source/WandboundCore/Private/WBCardLifecycle.cpp
5. Source/WandboundCore/Private/WBCardZoneTransition.cpp
6. Source/WandboundCore/Private/WBMatchCoordinator.cpp
7. Source/WandboundCore/Private/WBReplayTrace.cpp
8. Source/WandboundCore/Private/WBTurnStartSequence.cpp
9. Source/WandboundCore/Public/WBCardLifecycle.h
10. Source/WandboundCore/Public/WBCardZoneTransition.h
11. Source/WandboundCore/Public/WBEventSnapshot.h
12. Source/WandboundCore/Public/WBReplayTrace.h
13. Source/WandboundRuntime/Private/WBProductionCardZoneTransitionSmoke.cpp
14. Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp
15. Source/WandboundRuntime/Public/WBProductionCardZoneTransitionSmoke.h
16. Source/WandboundTests/Private/WBCardZoneObservationTests.cpp
17. Source/WandboundTests/Private/WBCardZoneStateTests.cpp
18. Source/WandboundTests/Private/WBCardZoneTransitionEventTests.cpp
