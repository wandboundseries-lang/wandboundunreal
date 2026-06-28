# Card Zone Observation Audit

Date: 2026-06-28

## Current CardZoneState Behavior

- `FWBGameStateData` owns `FWBCardZoneState`.
- `FWBCardZoneState` contains per-player Deck, Hand, and Discard entries, equipped entries, and marker placeholders.
- Deck, Hand, and Discard entries carry card instance id, card id, owner player id, zone, and deterministic zone index.
- Equipped entries carry card instance data, attached unit id, and slot id.
- Marker placeholders carry marker id, owner player id, tile, public state, and hidden `InternalMarkerCardId`.
- `WBCardZoneState` already provides deterministic sorting, count, lookup, validation, board-reference, and marker lookup helpers.
- No rules or EffectRunner path moves cards between zones yet.

## Current Public Board Summary Behavior

- `WBPublicBoardSummary::Build` exposes board dimensions, visible units, walls, terrain, and visible board unit `CardId`.
- Deck, Hand, Discard, Equipped, and Marker placeholder data are not included in `FWBPublicBoardSummary`.
- Board unit card identity is public under the current Godot audit and Unreal public board summary policy.
- Public board summaries remain unchanged by this pass.

## Current Hidden Marker Identity Policy

- `InternalMarkerCardId` is hidden future marker data.
- Public summaries and player observations must not include `InternalMarkerCardId`.
- Marker public observation may include marker id, owner, tile, and public state, but not card identity.
- Reveal, trap, and NPC marker behavior remain future work.

## Current Discard Visibility Uncertainty

- Godot observations appear to expose discard information, but full Unreal discard visibility has not been finalized.
- Until canon is settled, public discard identity remains fail-closed.
- This pass exposes public discard counts only.
- This pass allows own discard identity in the owner-scoped player observation so future hand/discard workflows can be tested without making discard globally public.

## Chosen Observation Summary Shape

This pass adds a Core-only observation helper:

- `FWBCardZonePublicSummary`
- `FWBCardZonePlayerObservation`
- `FWBObservedZoneSummary`
- `FWBObservedEquippedSummary`
- `FWBObservedMarkerSummary`
- `FWBObservedCardRef`

The public summary contains count-only Deck, Hand, and Discard summaries for each player, count-only Equipped summary, marker public fields, and board card references derived from visible units.

The player observation contains the public summary plus viewer-scoped OwnHand, OwnDeck, and OwnDiscard summaries.

## Own Hand Visibility Policy

- The viewing player sees their own Hand card instance ids and card ids.
- Own Hand is ordered by `ZoneIndex`, then instance id, then card id.
- Opponent Hand identity is never exposed through a player observation.

## Opponent Hand Hiding Policy

- Opponent Hand is visible only as a count through `PublicSummary.PlayerHands`.
- Opponent Hand card instance ids and card ids are never included in public summary or player observation private fields.

## Deck Identity Hiding Policy

- Deck identity is hidden for both players.
- Public summary exposes Deck counts only.
- Player observation OwnDeck exposes count only.
- No Deck card instance ids or card ids are included in observation output.

## Discard Fail-Closed Policy

- Public summary exposes Discard counts only.
- Public Discard card identity remains hidden.
- OwnDiscard in the player observation may expose only the viewer player's discard identities.
- Opponent Discard identity remains hidden.

## Equipped Visibility Policy For This Pass

- Equipped public summary exposes count only.
- Equipped card identity remains hidden in both public and player observation.
- Own equipped visibility is intentionally not added yet because equipment/wand visibility and attachment policy need canon confirmation.

## Why This Pass Does Not Implement Gameplay Movement

Observation is read-only. Draw, shuffle, discard movement, summon, equip, marker reveal, and response windows require rules, effect mutation, traces, and fixture coverage. This pass only makes hidden-safe state views available for later systems.

## Why This Pass Does Not Implement UI

Runtime/UI must consume already-safe summaries rather than inspect hidden state directly. This pass stays in WandboundCore so future runtime/UI work can depend on a deterministic observation contract.

## Hidden-Information Policy

- Public summaries must not expose Deck identity.
- Public summaries must not expose Hand identity.
- Public summaries must not expose Discard identity.
- Public summaries must not expose Equipped identity.
- Player observations must not expose opponent Hand, Deck, Discard, or Equipped identity.
- Hidden marker identity must not appear anywhere in public or player observations.
- Source effect ids, debug activation ids, usage keys, fixture metadata, and CardDB internals remain out of scope and must not appear.

## Risks / Open Questions

- Discard public visibility still needs final canon confirmation.
- Equipped/wand public and owner-private visibility still needs final canon confirmation.
- Revealed marker identity and owner-visible hidden marker behavior are future work.
- Observation is not JSON-serialized in production yet; this pass keeps deterministic string scan helpers for tests only.
