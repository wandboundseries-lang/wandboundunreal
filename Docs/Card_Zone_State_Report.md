# Card Zone State Report

Date: 2026-06-28

## Purpose

This pass adds production-safe WandboundCore card zone state structs and deterministic query helpers. It prepares the project for player-perspective observation without adding gameplay movement, production CardDB import, activation action integration, UI, or runtime behavior changes.

## Structs Added

- `EWBCardZone`
- `FWBCardInstanceRef`
- `FWBZoneCardEntry`
- `FWBEquippedCardEntry`
- `FWBBoardCardReference`
- `EWBMarkerPublicState`
- `FWBMarkerPlaceholderEntry`
- `FWBPlayerCardZoneState`
- `FWBCardZoneState`
- `WBCardZoneState` helper class

`FWBGameStateData` now owns `FWBCardZoneState CardZoneState` and exposes test-facing helpers for access and clearing.

## Deck / Hand / Discard Behavior

Deck, Hand, and Discard are modeled as ordered per-player arrays of `FWBZoneCardEntry`.

Each entry carries card instance id, card id, owner player id, zone, and deterministic zone index.

Deck and Hand are private by default. Discard visibility remains unresolved for the full observation model, so this pass treats Discard as fail-closed private until a player-perspective policy is implemented.

No draw, shuffle, discard movement, hand play, or card movement rules were added.

## Equipped Behavior

Equipped cards are modeled as `FWBEquippedCardEntry` records with card instance data, `EquippedToUnitId`, and `SlotId`.

This stores future equip/wand attachment shape only. It does not enforce equip legality, RL overflow, wand destruction, or source-zone activation behavior.

## Board Reference Behavior

Board card references are derived from existing `FWBUnitState` records using `WBCardZoneState::BuildBoardCardReferencesForTest`.

The helper excludes defeated or removed units, uses `UnitId`, `OwnerId`, and visible unit `CardId`, sorts deterministically by Y, X, then UnitId, and does not mutate state.

Board unit state remains the source of truth. `FWBCardZoneState` does not duplicate board occupancy.

## Marker Placeholder Behavior

Marker placeholders store marker id, owner player id, tile, public state, and hidden internal marker card id.

`InternalMarkerCardId` is retained for future reveal/trap/NPC work and is not exposed through public board summaries or runtime presentation.

This pass does not add marker reveal, marker trigger, trap, NPC scheduling, or NPC phase behavior.

## Ordering / Identity Policy

- Deck, Hand, and Discard sort by `ZoneIndex`, then instance id, then card id.
- Player zone records sort by player id.
- Equipped cards sort by attached unit id, slot id, then instance id.
- Marker placeholders sort by tile Y, tile X, then marker id.
- Card instance ids must be unique across Deck, Hand, Discard, and Equipped when present.

## Validation Policy

`WBCardZoneState::ValidateZoneStateForTest` fails on invalid player ids, duplicate player zone records, zone mismatch within ordered zones, empty card ids, invalid owner ids, owner mismatch in player-owned ordered zones, duplicate card instance ids, equipped entries without a valid attached unit id, invalid marker owners, duplicate marker ids, marker tiles out of bounds, and duplicate marker tiles.

The validator is test-facing scaffolding. It is not production card movement logic.

## Hidden-Information Policy

- Deck identities are private.
- Hand identities are private except future owner-scoped observation.
- Discard identities are not globally public in this pass.
- Equipped identities are not exposed by this pass.
- Hidden marker identities are private.
- Public board summaries remain units/walls/terrain only.

## Out Of Scope

- CardDB import
- production CardDB loading
- draw
- shuffle
- discard movement
- summon
- equip gameplay
- activation legal action generation changes
- activation `FWBAction` integration
- `WBActionCodec` changes
- response windows
- UI widgets
- target picking
- marker reveal/trigger behavior
- NPC phase
- passives
- wands
- overflow
- card-specific behavior
- Blueprint gameplay
- `.uasset` / `.umap` edits
