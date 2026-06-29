# Summon / Equip / RL Foundation Audit

## Current Scope

This audit covers the production-safe summon/equip/RL foundation pass. The pass is limited to minimal card metadata, read-only RL/RR query helpers, and read-only option generation for own-hand Character and Wand cards.

It intentionally does not execute summon, execute equip, move cards, mutate RL, resolve overflow, create `FWBAction`, update `WBActionCodec`, update `WBRules::GenerateLegalActions`, add UI, add response windows, or import Godot CardDB data.

## Current Unit RL / RL Used Fields

`FWBUnitState` already stores:

- `RLTotal`
- `RLUsed`

Existing activation affordability and cost-payment scaffolding already reads those fields for activation costs. This pass will add a narrow `WBResonanceLoad` query helper for summon/equip foundation code instead of duplicating affordability/cost-payment behavior.

Chosen policy:

- `CurrentRL = Unit.RLTotal`
- `RLUsed = Unit.RLUsed`
- `AvailableRL = CurrentRL - RLUsed`, clamped to zero in successful summaries
- negative total/used values return `invalid_rl_state`
- `RLUsed > RLTotal` returns `overflow_pending`
- no overflow destruction or equip mutation is performed

## Current CardZoneState Equipped State

`FWBCardZoneState` already contains `EquippedCards` as `FWBEquippedCardEntry` records with card instance data, `EquippedToUnitId`, and `SlotId`.

This is scaffold state only. This pass will not add entries to `EquippedCards`, remove cards from Hand, mutate slots, destroy wands, or update `RLUsed`.

## Current CardDefinition Shape

`FWBCardDefinition` already has:

- `CardId`
- `PublicName`
- `Kind`
- `ActivatedEffects`

`EWBCardDefinitionKind` already supports `Character`, `Wand`, `Action`, `Terrain`, `Wall`, and `Fixture`.

Missing for this pass:

- minimal Character stat template
- minimal Wand RR metadata

Chosen additions:

- `FWBCardCharacterStatsDefinition` with `HP`, `ATK`, `AR`, and `RL`
- `FWBCardWandStatsDefinition` with `RR`
- `FWBCardDefinition::CharacterStats`
- `FWBCardDefinition::WandStats`

Repository validation will fail invalid Character/Wand stat values while preserving existing activation-only Fixture definitions.

## Current Fixture Loader Card Kind Support

The Unreal-owned fixture loader already parses `kind` values including `character` and `wand`. It does not currently parse `character_stats` or `wand_stats`.

This pass will extend the fixture loader only for Unreal-owned fixture JSON strings/files. It will not import Godot CardDB or add a broad production loader.

Chosen fixture fields:

- `character_stats`: `{ "hp": number, "atk": number, "ar": number, "rl": number }`
- `wand_stats`: `{ "rr": number }`

Malformed or invalid stats fail closed through existing fixture diagnostics and repository validation.

Compatibility note:

- existing activation-only fixtures may use `kind: character` with `activated_effects` and no `character_stats`
- those legacy fixtures remain activation Fixtures internally so existing activation tests keep passing
- stat-bearing Character/Wand definitions still require their metadata and are the only definitions consumed by summon/equip options

## Current Hand / Deck / Discard Lifecycle

`WBCardLifecycle` now provides deterministic Deck-to-Hand draw, setup draw, turn-start draw helper, and Hand-to-Discard movement. Production activation execution already moves successful Hand-source activation cards to Discard before provider refresh.

This pass only reads own Hand and does not move cards. Future summon/equip execution can reuse lifecycle movement policy.

## Current Hero Representation

`FWBPlayerStateData::HeroUnitId` identifies a player's Hero unit. A Hero is found by looking up the viewer player, then `State.GetUnitById(Player.HeroUnitId)`, and requiring the unit to be owned by that player and on board.

If a Hero is missing or removed, summon options are suppressed with `hero_not_found`.

## Current Unit Cap Representation

There is no dedicated production unit-cap helper. Existing unit ownership can be queried through `FWBGameStateData::GetUnitsForPlayer`, which returns owned on-board units.

Chosen policy:

- max 4 owned on-board units including Hero
- unit cap reached suppresses Character summon options with `unit_cap_reached`

## Chosen Character Definition Metadata

Character cards use `EWBCardDefinitionKind::Character` plus `CharacterStats`.

Validation policy:

- `HP > 0`
- `ATK >= 0`
- `AR >= 0`
- `RL >= 0`

The summon option provider includes these stats in its read-only option output for future summon execution.

## Chosen Wand / Equipment Definition Metadata

Wand cards use `EWBCardDefinitionKind::Wand` plus `WandStats`.

Validation policy:

- `RR >= 0`

This pass treats Wand as the only equip option family. It does not add generic equipment slots beyond future-proof output fields.

## Chosen RL Query Helper API

Create `WBResonanceLoad` in `WandboundCore` with:

- `BuildUnitLoadSummary`
- `CanPayRR`
- `ResultReasonToString`

Stable reasons:

- `success`
- `unit_not_found`
- `invalid_unit_owner`
- `invalid_rr`
- `invalid_rl_state`
- `overflow_pending`

The helper is read-only and does not equip, mutate RL, move cards, or resolve overflow.

## Chosen Summon Option Provider API

Create `FWBProductionSummonEquipDataProvider` in `WandboundRuntime`.

Summon options:

- use `WBCardZoneObservation::BuildObservationForPlayer`
- inspect `OwnHand` only
- include only own-hand cards whose repository definition is `Character`
- require the viewer's Hero
- enforce max 4 owned on-board units including Hero
- generate orthogonally adjacent Hero tiles that are in bounds and unoccupied
- omit options with no legal tiles and emit `no_summon_tiles_available`
- do not consider walls for adjacency in this foundation pass
- do not summon or move cards

## Chosen Equip Option Provider API

Equip options:

- use `WBCardZoneObservation::BuildObservationForPlayer`
- inspect `OwnHand` only
- include only own-hand cards whose repository definition is `Wand`
- use `WandStats.RR`
- eligible units are viewer-owned, on-board, not defeated, and have enough available RL by `WBResonanceLoad::CanPayRR`
- omit options with no eligible units and emit `no_eligible_equip_unit`
- do not equip, move cards, change `RLUsed`, destroy wands, or resolve overflow

## Hidden-Info Policy

Provider output and diagnostics must not include:

- opponent hand instance ids
- opponent hand card ids
- deck instance ids
- deck card ids
- hidden marker internal card ids
- raw source JSON
- schema/dev labels
- debug activation ids
- usage keys
- hidden fixture metadata

Safe output may include viewer-owned hand card ids/instance ids, viewer-owned board unit ids, validated public card names, and already-public board identity.

## Exclusions

No summon/equip execution is added because this pass prepares option data only; execution will need deterministic mutation, trace, lifecycle movement, and provider refresh in a later pass.

No overflow resolution is added because overflow destruction semantics and wand/equipment replacement rules need a separate canon audit.

No UI or response windows are added because runtime/UI selects only and response timing is a separate action-contract problem.

`WBRules::GenerateLegalActions` is unchanged because summon/equip options are not normal legal action families yet.

`WBActionCodec` is unchanged because no summon/equip `FWBAction` ids are created in this pass.

## Risks / Open Questions

- Whether walls should block summon adjacency is deferred.
- Whether non-wand equipment exists and how slots are named is deferred.
- Whether public discard/equipped identity policies change after equip execution is deferred.
- How summon/equip option ids eventually relate to `FWBAction` remains deferred.
- Overflow, replacement, wand destruction, and response timing remain future passes.
