# Card Zone Observation Report

Date: 2026-06-28

## Purpose

This pass adds deterministic player-perspective observation summaries for `FWBCardZoneState`.

The observation layer is read-only. It does not move cards, import CardDB data, generate legal actions, mutate `FWBGameStateData`, or change runtime/UI behavior.

## Public Summary Fields

`WBCardZoneObservation::BuildPublicSummary` produces `FWBCardZonePublicSummary` with:

- per-player Deck summaries
- per-player Hand summaries
- per-player Discard summaries
- equipped-card count summary
- marker placeholder public fields
- board card references derived from existing unit state

Deck, Hand, and Discard public summaries expose count only. Their `Cards` arrays remain empty.

## Player Observation Fields

`WBCardZoneObservation::BuildObservationForPlayer` produces `FWBCardZonePlayerObservation` with:

- `ViewerPlayerId`
- the public summary
- owner-private hand summary
- owner-private discard summary
- own deck count-only summary

Invalid viewer ids return deterministic empty private summaries while still producing the public summary.

## Own Hand Policy

The viewer's own hand is visible through `OwnHand`.

Own hand cards include `InstanceId`, `CardId`, and `OwnerPlayerId`, ordered by `ZoneIndex`, then instance id, then card id.

## Production Provider Consumption

`FWBProductionActivationDataProvider` consumes `WBCardZoneObservation::BuildObservationForPlayer` for own-hand activation sources.

Only `OwnHand.Cards` are used for hand-source activations. Opponent hand, deck identity, and hidden marker identity remain outside provider output.

Provider unit target options use visible board units from `FWBPublicBoardSummary`. They do not inspect hidden card zones.

`FWBProductionSummonEquipDataProvider` also consumes `WBCardZoneObservation::BuildObservationForPlayer` and reads `OwnHand` only. It emits read-only summon/equip options for viewer-owned Character/Wand cards while keeping opponent Hand, Deck, and hidden marker identities out of provider output.

## Opponent Hand Policy

Opponent hand identity is hidden.

Opponent hands appear only as count-only entries in `PublicSummary.PlayerHands`. Opponent hand `InstanceId` and `CardId` are not included in the observation.

## Deck Identity Policy

Deck identity is hidden for both players.

Own deck and opponent deck observations expose counts only. Deck `InstanceId` and `CardId` values are not included.

## Discard Policy

Public discard visibility remains unresolved, so public discard summaries are fail-closed count-only.

The viewer's own discard is visible through `OwnDiscard` for now. Opponent discard identity is not exposed.

## Lifecycle Update Behavior

`WBCardZoneObservation` required no code changes for draw/discard lifecycle movement. It reads the current `FWBCardZoneState`, so public counts, owner-private Hand, and owner-private Discard update after `WBCardLifecycle` moves cards.

Lifecycle tests now prove that draw updates public Deck/Hand counts while keeping Deck identity hidden, and that hand-to-discard updates public Discard count while keeping Discard identity hidden publicly.

## Equipped Policy

Equipped card identity is fail-closed in this pass.

The public summary exposes only equipped-card count. `OwnVisibleEquippedCards` remains empty until an explicit owner/public equipped visibility policy is confirmed.

## Marker Identity Policy

Marker summaries include only marker id, owner player id, tile, and public state.

`InternalMarkerCardId` is intentionally not present in observation structs or public scan output.

## Board Reference Policy

Board card identity remains public only through the existing board-unit `CardId` policy.

`FWBCardZonePublicSummary.BoardCardReferences` uses the existing card-zone board reference helper, which derives visible board card references from active unit state and excludes defeated or removed units.

## Deterministic Ordering Policy

- Player zone summaries sort by player id.
- Zone card entries sort by `ZoneIndex`, then instance id, then card id.
- Markers sort by tile Y, tile X, then marker id.
- Board references use the existing board-reference ordering policy.

Equipped identities are not exposed yet, so equipped identity ordering is deferred.

## Hidden-Info Tests

Automation coverage asserts that public and player observations do not include:

- opponent hand identity
- deck identity
- opponent discard identity
- equipped identity
- hidden marker identity
- removed board unit card ids

Source guards also verify that public observation structs do not contain `InternalMarkerCardId` and that only approved production providers consume the observation helper in runtime code.

## Out Of Scope

- CardDB import
- production CardDB loading
- shuffle
- summon
- equip gameplay
- marker reveal behavior
- marker trigger behavior
- player UI
- target picking
- response windows
- NPC phase
- passives
- wands
- overflow
- activation `FWBAction` integration
- `WBActionCodec` changes
- `WBRules::GenerateLegalActions` changes
- runtime visual/UI changes
- Blueprint gameplay
- `.uasset` or `.umap` edits

## Next Planned Pass

Add an activation selection bridge that binds a selected provider target option to an executable activation command without changing hidden-information policy.
