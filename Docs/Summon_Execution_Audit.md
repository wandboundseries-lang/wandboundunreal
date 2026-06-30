# Summon Execution Audit

This audit covers the deterministic summon execution pass. The pass is limited to moving an own-Hand Character card onto the Board as a new unit through C++ state mutation, using provider-supplied summon option data at the runtime handoff layer.

## Current Character Metadata

`FWBCardDefinition` now carries minimal production metadata for summon/equip foundation work:

- `Kind == EWBCardDefinitionKind::Character`
- `CharacterStats.HP`
- `CharacterStats.ATK`
- `CharacterStats.AR`
- `CharacterStats.RL`

Repository validation requires Character HP to be greater than zero and ATK, AR, and RL to be non-negative. These stats are sufficient to create a new board unit without importing Godot CardDB or adding full production card loading.

## Current Summon Option Provider

`FWBProductionSummonEquipDataProvider` reads `WBCardZoneObservation::BuildObservationForPlayer` and iterates the viewer's `OwnHand` only. It emits Character summon options for own-Hand cards whose definitions are Character cards.

The provider currently:

- finds the viewer Hero through `FWBPlayerStateData::HeroUnitId`
- enforces a max of four owned on-board units including the Hero
- emits empty, in-bounds, orthogonally adjacent Hero tiles sorted by Y then X
- omits summon options when no legal tiles exist
- does not mutate game state or repository state
- does not inspect opponent Hand or Deck identities

Provider options are read-only intent data. Execution still has to revalidate source, tile, Hero, unit cap, occupancy, marker, and definition safety.

## Current Hand Lifecycle

`WBCardLifecycle` supports draw and Hand-to-Discard movement. It validates card zone state, removes cards from ordered zones, normalizes zone indexes, and preserves hidden-info observation rules.

Summon execution should not use `MoveHandCardToDiscard`, because a summoned Character card leaves Hand to become a board unit and should not enter Discard. This pass adds a narrow Hand-to-Board transition in `WBSummonExecution`.

## Current Unit State Shape

`FWBUnitState` stores board unit truth:

- `UnitId`
- `OwnerId`
- `CardId`
- `X` / `Y`
- `HP` / `MaxHP`
- `ATK`
- `AR`
- `RLTotal`
- `RLUsed`
- `bDefeated`
- `bRemovedFromBoard`

Board card references are derived from on-board units by `WBCardZoneState::BuildBoardCardReferencesForTest`.

## Current Hero Detection

The viewer Hero is represented by `FWBPlayerStateData::HeroUnitId`. A valid Hero for this pass must:

- exist in `State.Units`
- be owned by the summoning player
- be on board
- not be defeated
- have a valid board tile

No new Hero-card metadata is added in this pass.

## UnitId Assignment

There is no production unit-id allocator yet. This pass uses deterministic allocation:

- scan all existing units
- choose `max(existing UnitId) + 1`
- fail if allocation would overflow or produce a duplicate

This is stable for replay-style tests and easy to replace later with a shared allocator.

## Unit Cap Behavior

The existing provider treats the cap as four owned on-board units including the Hero. Core execution revalidates the same cap using `State.GetUnitsForPlayer(PlayerId).Num()`, which already excludes defeated or removed units through `IsUnitOnBoard`.

## Selected Tile Validation

Runtime handoff validates the requested tile against provider-supplied `LegalTiles`.

Core execution independently revalidates:

- target tile is on the 9x9 board
- target tile is orthogonally adjacent to the Hero
- target tile is unoccupied by a unit
- target tile does not contain a marker placeholder

Walls are not part of summon adjacency for this pass, matching the foundation provider behavior.

## Hand-To-Board Movement Policy

On success only:

- remove the matching source card instance from the player's Hand
- normalize remaining Hand `ZoneIndex` values to `0..N-1`
- create a new on-board unit from Character stats
- do not add the card to Discard
- do not create a separate Board zone entry

On failure, no state mutation is applied.

## Marker Tile Policy

If a marker placeholder exists at the selected target tile, execution fails with `marker_trigger_deferred`. Marker reveal, trap behavior, NPC scheduling, and marker-triggered summon rules remain future work.

## Trace/Replay Event Policy

Successful summon execution emits one deterministic trace event:

- `event_type`: `summon_unit`
- `player_id`
- `source_instance_id`
- `source_card_id`
- `created_unit_id`
- `target_tile`

Failure emits no success trace event. Trace output excludes opponent Hand identities, Deck identities, hidden marker identity, raw fixture JSON, debug activation ids, and usage keys.

## Provider Refresh Policy

Runtime summon handoff refreshes `FWBProductionSummonEquipDataProvider` after successful execution. Refreshed data reflects:

- source card removed from own Hand
- public Hand count decreased
- new board unit available for future equip options if it has enough available RL
- unit cap changes

Activation-provider refresh is deferred to avoid mixing summon execution with activation source-gate behavior in this pass.

## Hidden-Info Policy

Execution results, trace events, and refreshed provider data may include the viewer's selected source card id/instance id and the created public board unit id/card id.

They must not include:

- opponent Hand instance or card ids
- Deck instance or card ids
- hidden marker internal card ids
- raw source JSON
- debug activation ids
- usage keys
- hidden fixture metadata

## Deferred Work

This pass does not implement equip execution because equip needs separate Hand-to-Equipped movement, RLUsed mutation, slot policy, overflow policy, and wand destruction rules.

This pass does not implement marker triggers because marker reveal and trigger timing are not yet modeled in the Unreal rules kernel.

This pass does not add UI or response windows because it is a deterministic state transition and runtime handoff layer only.

This pass does not change `WBRules::GenerateLegalActions` because summon is not a normal legal action family yet.

This pass does not change `WBActionCodec` because no summon `FWBAction` id is introduced.

## Risks / Open Questions

- A future shared unit-id allocator may replace the `max + 1` policy.
- Equipment slots and RL overflow may require revisiting refreshed equip-option behavior.
- Marker-triggered summons will need a clear timing boundary before allowing summons onto marker tiles.
- Future `FWBAction` integration will need a canonical summon action id format and replay verifier coverage.
