# Card Zone Mutation Foundation Audit

## Scope

This pass establishes one deterministic internal authority for exact-instance
structural mutation of the ordered private card zones. A private legacy
inventory was consulted read-only for capability coverage.

The implementation does not add cards, player actions, public zone data,
shuffle behavior, cross-player transfers, or a new replay format.

## Current Fragmentation

Before this pass, `WBCardLifecycle` separately implemented Deck-to-Hand draw,
Hand-to-Discard movement, and exact extraction from Deck and Hand. Character
summon already used those lifecycle operations, so the production call graph
had a narrow semantic boundary but duplicated structural array mutation.

Production bootstrap Deck construction remains an intentional setup operation.
Equipped-to-Discard remains specialized because equipped cards use unit, slot,
and equip-order metadata rather than ordered-zone entries.

## Supported Ordered Zones

`WBCardZoneMutation` supports exact structural operations within one player's:

- Deck
- Hand
- Discard

All three remain private. Equipped, Board, Marker, Unknown, cross-player
movement, and same-zone reordering fail closed.

## Exact Instance Authority

`CardInstanceId` is the selection identity. Duplicate `CardId` values remain
independent. An optional expected `CardId` is only a consistency check against
authoritative state and never a fallback selector.

## TransferExact

`TransferExact` validates the player, source and destination zones, exact
instance location, ownership, optional card identity, destination policy, and
the complete zone state. It mutates a working copy, preserves card identity and
remaining relative order, appends to the destination, normalizes private
ordered indexes, validates the final state, and commits only on success.

The only destination policy is explicit `Append`, matching current production
requirements. Top, bottom, exact-index insertion, and same-zone reorder are
deferred.

## ExtractExact

`ExtractExact` removes one exact instance from Deck, Hand, or Discard on a
working copy and returns the authoritative card reference to its semantic
caller. Extraction does not create a gameplay zone or publish the card.

## Order Preservation

The selected entry alone is removed. Remaining entries retain authoritative
`ZoneIndex` order. Every private ordered zone is normalized to contiguous
indexes after a successful mutation. Unique noncontiguous prestate indexes are
treated as harmless ordering metadata and normalized; duplicate, negative, or
contradictory indexes fail closed.

## Lifecycle Adapters

`WBCardLifecycle` remains the semantic API:

- `DrawOneCard` identifies the current Deck top, then delegates exact
  Deck-to-Hand append.
- `DrawCards` remains sequential and preserves prior successful draws if a
  later draw reaches an empty Deck.
- `MoveHandCardToDiscard` delegates exact Hand-to-Discard append.
- `RemoveExactCardFromDeck` and `RemoveExactCardFromHand` delegate extraction.
- `MoveEquippedCardToDiscard` remains specialized.

## Draw And Summon Behavior

Draw selection remains semantic and based on authoritative Deck order; the
mutation layer does not choose a card or consume RNG. Character summon still
consumes an exact Deck or Hand instance through lifecycle adapters, so Rook,
Patch, and Crash-In keep their existing choice, composition, and rollback
boundaries.

## Discard Support

Exact transfer and extraction structurally support private Discard scenarios
for future callers. No unreleased Discard mechanic or production card is added.

## Specialized Exclusions

Equipped, Board, and Marker are not represented as generic ordered-zone
sources. Search, tutor, choice enumeration, reveal, shuffle, declaration,
caster, target, and reaction semantics remain with their existing authorities.

## Privacy

The mutation result is authoritative internal data. The primitive emits no
trace and exposes no public CardId, CardInstanceId, ZoneIndex, Deck order, Hand
identity, Discard identity, or private candidate alternatives. Public board and
viewer observation schemas are unchanged.

## Replay And Determinism

The mutation uses no RNG, GUID, timestamp, pointer, or parallel action-ID
system. Identical state and request inputs produce identical ordered state.
Semantic callers remain responsible for replay records and privacy-safe trace
events. Replay schema version 1 and the eight-field public receipt are
unchanged. `WBActionCodec` is unchanged.

## Validation

- New focused assertions: 93 across 8 `Wandbound.CardZoneMutation` tests.
- Focused mutation suite: 8 succeeded, 0 failed, 0 not run.
- Existing lifecycle suite: 25 succeeded, 0 failed, 0 not run.
- Affected production suite: 260 succeeded, 0 failed, 0 not run.
- Full automation: 2,489 succeeded, 0 failed, 0 not run; +8 from the
  committed 2,481-test baseline.
- Editor non-unity: succeeded in 178.60 seconds.
- Game non-unity: succeeded in 179.86 seconds.
- Editor default: succeeded in 33.77 seconds.
- Game default: succeeded in 38.73 seconds.
- Editor forced-unity: succeeded in 22.95 seconds.
- Game forced-unity: succeeded in 39.67 seconds.
- Clean BuildCookRun: build, cook, stage, package, and archive succeeded in
  621.83 seconds.

## Packaged Production Smoke

The packaged inner executable ran the Undertow Archivist, Rook, and Crash-In
production scenarios twice with the cooked local-play map and package-relative
fixtures. The scenarios cover Deck-to-Hand draw, exact Deck consumption,
exact Hand activation consumption and discard, replay persistence, fresh
coordinator replay, and private public-observation checks.

Both runs of each scenario were byte-identical:

| Scenario | Records | Archive SHA-256 | Receipt SHA-256 | Final State Digest | Final Trace Digest |
| --- | ---: | --- | --- | --- | --- |
| Undertow | 8 | `1e829831e19b257c610f98c00625db37cc8bddaa2194d64e02ef03fb7dc7e97a` | `a204b26695a3d28c284acd4c263df7bfd485e7fae7785d20e5535a11cc2276fa` | `1ca20348b9ae042fca0e6339736db5f3278e6d60576e0cdad6bb495ce7c37599` | `eb760437e0320e38ea6a1add143a60c94275b64436cda121bbbccb3d710df8c6` |
| Rook | 15 | `0cb6ec323be965be6b7be1ad9d1841535a001c70fcaf67e5762e9083a42d79fc` | `f12343b4c95e340383d12782cfc521640c32cd21a11812e0676b76d22e3c483c` | `f0c029604f5a656b4a58c1effdb03023167bc3366caebb26a09b93d668210c97` | `caa6501547b22978039ff7a6c0c88ee0b75f13d8f8841294210fec87f8aad1d4` |
| Crash-In | 8 | `687aacb3c0faf57e0bce3e8bbdcb580b957d48d066ad1b71f7ef33ed6c2915f4` | `76a8ff0025fa4137a35262f52f43a89d24e1579871cccb87fcd5b51ce3c28701` | `a274b44a9894faf29609a5945c747a27d3ef663ad59055a31926125819b52a0b` | `a3ceda9dce70c0798d737f412ff3c1b08025e3fa3d36d09a12599a0f5992b8f4` |

The shared startup JSON SHA-256 is
`c1589b0b9f442008164c71854ffc1ded9dbf77f74160daef9a4ff550f194d99c`.
Every receipt has exactly eight fields and contains no protected state/trace
digest or private-zone identity.

## Exact Changed-File Manifest

1. `Docs/Card_Zone_Mutation_Foundation_Audit.json`
2. `Docs/Card_Zone_Mutation_Foundation_Audit.md`
3. `Source/WandboundCore/Private/WBCardLifecycle.cpp`
4. `Source/WandboundCore/Private/WBCardZoneMutation.cpp`
5. `Source/WandboundCore/Public/WBCardZoneMutation.h`
6. `Source/WandboundTests/Private/WBCardLifecycleTests.cpp`
7. `Source/WandboundTests/Private/WBCardZoneMutationTests.cpp`
