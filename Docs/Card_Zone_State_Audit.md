# Card Zone State Audit

Date: 2026-06-28

## Current Card/Zone State Findings

- `FWBGameStateData` is the current deterministic rules-state root.
- `FWBPlayerStateData` still carries legacy `Deck`, `Hand`, and `Discard` `TArray<FString>` fields used by older hidden-info tests and fixture scaffolding.
- Those legacy arrays are not production-safe zone state: they do not model card instances, zone ordering metadata, equipped attachments, markers, or future player-perspective observation policy.
- Existing rules and EffectRunner code do not move cards through Deck, Hand, Discard, Equipped, or Marker zones.
- Existing activation source-zone support is fixture-only. `FWBCardActivationFixtureZoneEntry` and `FWBCardActivationFixtureZoneContext` let tests describe Hand, Equipped, Discard, Board, Deck, or Fixture source metadata without adding production zones.

## Existing Board Unit Card Identity Behavior

- Board units are represented by `FWBUnitState` records.
- Visible board unit identity is stored as `FWBUnitState::CardId`.
- Board occupancy remains unit-state truth. One unit per tile is enforced by existing test helpers and rules queries.
- Board-source activation parity already uses visible unit `CardId` as the source of truth rather than fixture zone entries.

## Existing Public Board Summary Hidden-Info Policy

- `WBPublicBoardSummary::Build` serializes public board state only: visible units, walls, and terrain.
- Public unit summaries include visible `CardId`, HP, armor, combat fields, attacks left, and canonical public statuses.
- Deck, Hand, Discard, pending choices, fixture metadata, and hidden marker identity are excluded.
- Markers are intentionally absent from the public board summary until marker state and player-perspective observation are modeled.

## Existing Fixture-Only Source-Zone Scaffolding

- `EWBCardActivationSourceZone` and fixture zone entries are activation scaffolding, not production card-zone state.
- Fixture source-zone ownership can prove candidate behavior for Hand, Equipped, Discard, Board, Deck unsupported, and Fixture legacy modes.
- Candidate generation remains read-only and does not mutate real player zone arrays.
- Activation remains separate from `FWBAction`, `WBActionCodec`, and `WBRules::GenerateLegalActions`.

## Chosen Production-Safe Card Zone State Shape

This pass adds a separate `FWBCardZoneState` model instead of reusing legacy player arrays.

The new model should include:

- `FWBCardInstanceRef` for stable instance id, card id, and owner id.
- `FWBZoneCardEntry` for Deck, Hand, and Discard entries with deterministic `ZoneIndex`.
- `FWBEquippedCardEntry` for equipped cards attached to a source unit id and slot id.
- `FWBBoardCardReference` as a derived read-only reference from existing units.
- `FWBMarkerPlaceholderEntry` for future marker state with tile, owner, public state, and hidden internal marker card id.
- `FWBPlayerCardZoneState` for each player's Deck, Hand, and Discard.
- `FWBCardZoneState` on `FWBGameStateData` as the production-facing zone container.

Board references must be derived from unit records and must not duplicate board truth in `FWBCardZoneState`.

## Why This Pass Does Not Implement Draw/Shuffle/Discard Movement

Draw, shuffle, and discard movement require turn timing, deck order ownership, card movement traces, discard visibility policy, and failure behavior. This pass intentionally adds only deterministic storage and validation scaffolding so later movement rules can use a clear state shape.

## Why This Pass Does Not Implement Player-Perspective Observation Yet

Player-perspective observation needs a separate public/private visibility contract for each viewer. This pass prepares the state required by that next pass but does not expose Deck, Hand, Discard, Equipped, or Marker details through runtime/public summaries yet.

## Hidden-Information Policy

- Deck contents are private by default.
- Hand contents are private except to the owning player in a future observation model.
- Discard visibility remains unresolved for the full observation model, so this pass treats Discard as not globally public.
- Equipped cards are stored but not surfaced publicly by this pass.
- Marker identity is hidden by default.
- `InternalMarkerCardId` must not appear in public board summaries, traces, runtime presentation, or public tests.

## Risks / Open Questions

- Discard visibility must be confirmed before production observation exposes discard contents.
- Equipped visibility and wand-specific behavior remain future work.
- Marker reveal/trap/NPC behavior remains future work.
- The legacy `FWBPlayerStateData::Deck/Hand/Discard` fields still exist for older tests and must be retired only through a separate migration pass.
- Zone validation is test-facing for now and should become production diagnostics only when production card movement is implemented.
