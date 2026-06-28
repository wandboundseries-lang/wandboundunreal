# Card Definition Repository Report

Date: 2026-06-28

## Purpose

This pass adds a minimal production-safe WandboundCore repository shell for Unreal-owned `FWBCardDefinition` values.

The repository itself is static data/query scaffolding only. The separate `WBCardDefinitionFixtureLoader` can now populate repositories from narrow Unreal-owned fixture JSON files.

Neither the repository nor the fixture loader imports Godot CardDB data, generates actions, generates activation candidates, executes effects, inspects hidden zones, or mutates game state.

## Repository Vs Card Zone State

`FWBCardZoneState` stores mutable card instance state: Deck, Hand, Discard, Equipped, board references, and marker placeholders.

`FWBCardDefinitionRepository` stores static card definitions keyed by `CardId`.

Future provider work should bridge observed/owned card instances to repository definitions while preserving player-perspective hidden-information policy.

## Static Definition Vs Card Instance

Card instances carry instance id, owner, zone, and card id.

Card definitions carry static authored data such as public name, card kind, activated effects, target requirements, payloads, and source gates.

The repository owns definitions only. It does not own card instances or zones.

## Repository Fields

`FWBCardDefinitionRepository` contains:

- `RepositoryId`
- `SourceVersion`
- `Definitions`

`SourceVersion` is carried for future loader/importer context but may be empty in this shell.

## Validation Policy

`WBCardDefinitionRepository::ValidateRepository` fails closed on:

- `repository_id_missing`
- `card_id_missing`
- `duplicate_card_id`
- `public_name_missing`
- `effect_id_missing`
- `duplicate_effect_id`
- `public_label_contains_internal_term`
- `unsupported_target_requirement`

Activated effects may be empty. Payload family validation remains deliberately minimal here and is left to schema validation plus activation expansion/execution paths.

## Deterministic Ordering Policy

Public query helpers avoid `TMap` order dependence.

`GetCardIdsInDeterministicOrder` returns card ids sorted ascending.

`GetDefinitionsInDeterministicOrder` returns definitions sorted by `CardId` ascending.

Lookup uses exact `CardId` matching.

## Public Label Policy

`PublicName` and effect `PublicLabel` are player-facing. Repository validation rejects internal implementation or schema terms such as:

- `EffectRunner`
- `Rules`
- `damage_effect`
- `heal_effect`
- `armor_effect`
- `status_effect`
- `effect.type`
- `from_tile`
- `to_tile`
- `chosen_tile`
- `player 0`
- `player 1`
- `player id`
- `schema`
- `hook`

## Lookup Helpers

The repository exposes:

- `ContainsCardId`
- `FindCardById`
- `GetCardIdsInDeterministicOrder`
- `GetDefinitionsInDeterministicOrder`
- `HasDuplicateCardIds`

Empty lookup ids fail with `card_id_missing`. Missing cards fail with `card_definition_not_found`.

## Fixture Loader

`WBCardDefinitionFixtureLoader` loads the narrow Unreal-owned fixture shape under `Reference/GodotCanon/CardDefinitionRepositoryFixtures/`.

The loader maps supported fixture data into existing `FWBCardDefinition`, `FWBCardEffectDefinition`, `FWBCardActivationSourceGateDefinition`, and `FWBGenericEffectPayload` structures.

Supported payload families are damage, heal, status, and armor. Unsupported fields, payloads, timings, zones, malformed data, invalid statuses, and invalid numeric values fail closed with deterministic diagnostics.

The loader calls `WBCardDefinitionRepository::ValidateRepository` after parsing succeeds, so duplicate ids and public-label internal-term failures still use the repository boundary.

## Production Provider Consumption

`FWBProductionActivationDataProvider` consumes `FWBCardDefinitionRepository` definitions by exact `CardId`.

Board-source activations use visible board unit `CardId` values. Own-hand activations use card ids from `WBCardZoneObservation::OwnHand`.

Missing definitions fail closed for that source with `card_definition_not_found`; unsupported source zones are ignored with diagnostics.

The provider also reuses repository public-label guard behavior to reject internal player-facing labels.

Effect `TargetRequirement` now feeds provider target-option behavior. `Unit` requirements enumerate visible board unit options, `None` emits no options, and `Tile`/`WallEdge` remain deferred.

## Game State Policy

The repository is not stored on `FWBGameStateData`.

Game state owns mutable rules state and card instances. The repository is immutable-ish static reference data that future loader/provider work can own externally.

## Out Of Scope

- Godot CardDB import
- full production CardDB loading
- broad schema parser
- zone movement
- draw
- shuffle
- discard movement
- summon
- equip gameplay
- activation legal action generation changes
- activation provider skeleton
- activation `FWBAction` integration
- `WBActionCodec` changes
- `WBRules::GenerateLegalActions` changes
- response windows
- UI widgets
- target picking
- marker reveal behavior
- NPC phase
- passives
- wands
- overflow
- Blueprint gameplay
- `.uasset` or `.umap` edits

## Next Planned Pass

Add an activation selection bridge that binds selected unit target options to activation commands.
