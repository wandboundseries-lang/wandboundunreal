# Summon / Equip / RL Foundation Report

Date: 2026-06-28

## Purpose

This pass adds production-safe data contracts and read-only option generation for future summon/equip work.

It does not execute summon or equip, move cards, mutate `FWBGameStateData` from the provider, resolve overflow, add response windows, add UI, create `FWBAction`, change `WBActionCodec`, or change `WBRules::GenerateLegalActions`.

## Character Definition Metadata

`FWBCardDefinition` now includes `FWBCardCharacterStatsDefinition`:

- `HP`
- `ATK`
- `AR`
- `RL`

Repository validation requires Character cards with production stats to have `HP > 0`, `ATK >= 0`, `AR >= 0`, and `RL >= 0`.

The fixture loader supports `character_stats` for stat-bearing Character definitions. Legacy activation-only fixture entries that used `kind: character` with `activated_effects` and no stat block are preserved as Fixture definitions so existing activation fixtures continue to load.

## Wand Definition Metadata

`FWBCardDefinition` now includes `FWBCardWandStatsDefinition`:

- `RR`

Repository validation requires Wand cards to have `RR >= 0`.

The fixture loader supports `wand_stats` for stat-bearing Wand definitions. Missing or malformed stat blocks fail closed unless the fixture is an older activation-only entry with `activated_effects`.

## RL / RR Helper

`WBResonanceLoad` provides read-only RL/RR checks:

- `BuildUnitLoadSummary`
- `CanPayRR`
- `ResultReasonToString`

Stable reasons are `success`, `unit_not_found`, `invalid_unit_owner`, `invalid_rr`, `invalid_rl_state`, and `overflow_pending`.

`AvailableRL` is computed from `RLTotal - RLUsed`. Overflow is reported but not resolved.

## Summon Options

`FWBProductionSummonEquipDataProvider` emits summon options for viewer-owned Hand cards whose repository definition is `Character`.

Summon options:

- use `WBCardZoneObservation::BuildObservationForPlayer`
- inspect `OwnHand` only
- require the viewer Hero via `FWBPlayerStateData::HeroUnitId`
- enforce max 4 owned on-board units including Hero
- enumerate empty orthogonal adjacent Hero tiles
- sort tiles by Y, then X
- omit no-tile options and emit `no_summon_tiles_available`

Walls do not block summon adjacency in this foundation pass.

Follow-up summon execution now consumes these read-only options through `FWBProductionSummonExecutionHandoff`. Runtime handoff validates the selected source and target tile against provider data before the core summon execution helper revalidates state safety and mutates `FWBGameStateData`.

## Equip Options

The same provider emits equip options for viewer-owned Hand cards whose repository definition is `Wand`.

Equip options:

- use `WandStats.RR`
- enumerate viewer-owned on-board units only
- require enough available RL through `WBResonanceLoad::CanPayRR`
- sort eligible units by owner, tile Y, tile X, then unit id
- omit no-target options and emit `no_eligible_equip_unit`

No card is equipped, no `RLUsed` is changed, and no hand card is moved.

## Hidden Information

Provider output and diagnostics exclude opponent Hand identities, Deck identities, hidden marker internal card ids, raw JSON, debug activation ids, and usage keys.

Safe output may include the viewer's own Hand card instance/card ids, viewer-owned unit ids, public names accepted by repository validation, and already-public board identity.

## Boundary Confirmation

- No summon execution.
- No equip execution.
- No overflow resolution.
- No UI or runtime visual changes.
- No response windows.
- No `FWBAction` creation.
- No `WBActionCodec` changes.
- No `WBRules::GenerateLegalActions` changes.
- No Godot CardDB import.
- `Reference/GodotProject` was not modified.
- `.uasset` and `.umap` files were not modified.

## Tests

Added:

- `Wandbound.Core.ResonanceLoad.*`
- `Wandbound.Runtime.ProductionSummonEquipDataProvider.*`

Updated:

- `Wandbound.Core.CardDefinitionRepository.*`
- `Wandbound.Core.CardDefinitionFixtureLoader.*`
- card-zone/repository source guards for the new allowed provider boundary

## Validation

Build:

```text
Result: Succeeded
Total execution time: 9.28 seconds
```

Full Wandbound automation:

```text
succeeded=1281
succeededWithWarnings=0
failed=0
notRun=0
```

## Risks / Open Questions

- Summon execution and hand-to-board lifecycle movement remain future work.
- Equip execution, equipped-state mutation, `RLUsed` mutation, and overflow destruction remain future work.
- Wall-blocking policy for summon adjacency is deferred.
- Generic non-wand equipment and slot rules are deferred.
- Future action ids for summon/equip remain separate from `FWBAction` until explicitly designed.

## Next Planned Pass

Add deterministic equip execution from own Hand to equipped state using provider-owned equip option data, with RLUsed mutation and refreshed summon/equip provider data while keeping overflow policy deferred unless explicitly selected.
