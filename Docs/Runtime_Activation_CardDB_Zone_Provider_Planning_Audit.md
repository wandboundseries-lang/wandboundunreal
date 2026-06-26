# Runtime Activation CardDB/Zone Provider Planning Audit

## Scope

This is a planning-only audit for a future CardDB/zone-backed activation data provider. No production provider, CardDB import, production zone state, runtime generation, tests, fixtures, UI, response windows, source edits, assets, Blueprints, `.uasset`, or `.umap` changes were added.

## Files Inspected

Runtime provider/session:

- `Source/WandboundRuntime/Public/WBRuntimeActivationDataProvider.h`
- `Source/WandboundRuntime/Private/WBRuntimeActivationDataProvider.cpp`
- `Source/WandboundRuntime/Public/WBRuntimeActivationDataProviderAdapter.h`
- `Source/WandboundRuntime/Private/WBRuntimeActivationDataProviderAdapter.cpp`
- `Source/WandboundRuntime/Public/WBRuntimeActivationDecisionSessionFacadeComponent.h`
- `Source/WandboundRuntime/Private/WBRuntimeActivationDecisionSessionFacadeComponent.cpp`

Core activation/state:

- `Source/WandboundCore/Public/WBCardDefinition.h`
- `Source/WandboundCore/Public/WBCardActivationCandidate.h`
- `Source/WandboundCore/Public/WBCardActivationCandidateGenerator.h`
- `Source/WandboundCore/Public/WBCardActivationLegalAction.h`
- `Source/WandboundCore/Public/WBCardActivationLegalActionGenerator.h`
- `Source/WandboundCore/Public/WBCardActivationSourceGate.h`
- `Source/WandboundCore/Public/WBCardActivationAffordability.h`
- `Source/WandboundCore/Public/WBPublicBoardSummary.h`
- `Source/WandboundCore/Public/WBGameStateData.h`

Test-only provider scaffolding:

- `Source/WandboundTests/Private/WBRuntimeActivationDataProviderContractVerifier.h`
- `Source/WandboundTests/Private/WBRuntimeActivationDataProviderContractTests.cpp`
- `Source/WandboundTests/Private/WBRuntimeActivationSessionTestHarness.h`
- `Source/WandboundTests/Private/WBRuntimeActivationSessionTestHarness.cpp`

Reports:

- `Docs/Runtime_Activation_Data_Provider_Interface_Report.md`
- `Docs/Runtime_Activation_Data_Provider_Contract_Report.md`
- `Docs/Runtime_Activation_Decision_Session_Facade_Report.md`
- `Docs/Card_Activation_Candidate_Generation_Report.md`
- `Docs/Card_Activation_Source_Gates_Report.md`
- `Docs/Card_Activation_Source_Zone_Ownership_Report.md`
- `Docs/Card_Activation_Board_Source_Parity_Report.md`
- `Docs/Card_Activation_Affordability_Query_Report.md`
- `Docs/Card_Activation_Cost_Gates_Report.md`
- `Docs/Card_Activation_Cost_Payment_Scaffolding_Report.md`
- `Docs/Wandbound_Unreal_Migration_Plan.md`

Godot reference, read-only:

- `Reference/GodotCanon/README.md`
- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/CardDB.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/effects_quick.json`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/wands_equip.json`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/data/assets/asset_manifest.schema.json`
- `Reference/GodotProject/godotcanon/data/assets/asset_manifest.json`

## Current Runtime Provider Interface

`IWBRuntimeActivationDataProvider` is a thin interface returning `FWBRuntimeActivationDataProviderResult` for a `FWBRuntimeActivationDataProviderRequest`.

The request currently carries:

- request kind: `Unknown`, `CurrentDecisionPoint`, or `PostActivation`
- player id
- decision point id
- selected activation action id
- optional request reason

The result carries:

- `bOk`
- reason
- copied request
- `FWBRuntimeActivationDecisionSessionRefreshInput`

The refresh input is the runtime facade's external-data surface:

- normal legal `FWBAction` list
- `FWBCardActivationLegalActionSet`
- `FWBPublicBoardSummary`

The provider interface does not accept `FWBGameStateData`. This preserves the current boundary: runtime facade consumes provider output, while future provider implementation details remain outside the facade.

## Current Provider Adapter Behavior

`WBRuntimeActivationDataProviderAdapter` only forwards provider output.

Refresh flow:

- fail if the session facade is missing
- ask provider for current decision data
- stop on provider failure and propagate provider reason
- pass `RefreshInput` into `RefreshSessionFromExternalData`

Execute-and-refresh flow:

- fail if the session facade is missing
- ask provider for post-activation decision data before execution
- stop before execution on provider failure
- call `ExecuteActivationActionIdAndRefreshFromExternalData` with provider-supplied post-action data

The adapter does not generate normal legal actions, activation candidates, activation legal actions, or public summaries.

## Current Session Facade

`UWBRuntimeActivationDecisionSessionFacadeComponent` coordinates externally supplied normal actions, activation actions, and public summaries. It refreshes normal decision-point presentation and activation presentation through the decision-point owner. It can resolve a selected activation action id, create a handoff, execute through the owner/sequencer path, and refresh from caller-supplied post-action data.

The facade does not:

- generate legal actions
- generate activation candidates
- generate activation legal actions
- build public summaries from state
- inspect hidden zones
- merge activation into `FWBAction`

## Current Card Definition And Activation Scaffolding

`FWBCardDefinition` contains card id, public name, kind, and activated effects. `FWBCardEffectDefinition` contains effect id, public label, target requirement, generic effect payloads, and source-gate definition.

`FWBCardActivationCandidateGenerator` converts supplied activation sources into deterministic candidates. It is read-only, preserves source/effect/target ordering, and wraps commands for later explicit application.

`WBCardActivationLegalActionGenerator` converts candidates into the separate activation action family. Activation action ids are not `WBActionCodec` ids.

`WBCardActivationSourceGate` validates source-zone/timing/source/cost/usage gate context. Current zone support is fixture-oriented, except Board-source parity which uses the visible board unit `CardId`.

`WBCardActivationAffordability` provides read-only RL/RR affordability queries and projects results into source-gate context. Cost payment remains an explicit activation execution concern, not a generation concern.

`WBPublicBoardSummary::Build` creates public board summaries from state in core. Runtime provider paths should receive summaries from an authorized owner instead of building them inside runtime.

## Current Zone Scaffolding

Unreal currently has:

- `FWBPlayerStateData::Deck`
- `FWBPlayerStateData::Hand`
- `FWBPlayerStateData::Discard`
- fixture-only `FWBCardActivationFixtureZoneContext`
- fixture-only equipped source metadata through `EquippedToUnitId`
- board units with public `CardId`
- no production equipped zone state
- no production marker identity model
- no production observation/perspective model

The current fixture source-zone gates can test Hand, Equipped, Discard, Board, Deck unsupported, and legacy Fixture behavior without mutating production player zones.

## Current Hidden-Information Policy

Current reports and tests enforce these boundaries:

- public board summaries expose board units, current public combat fields, wall/terrain summaries, and visible board unit `CardId`
- hidden fixture zone metadata does not appear in traces, public summaries, or runtime selected-action JSON
- usage keys, source effect ids, debug activation ids, and cost metadata stay out of public presentation
- activation public labels use clean labels, with `Activate` fallback
- hidden marker identity is not modeled in Unreal and must remain excluded

## Godot CardDB And Zone Shape

Godot `CardDB.gd` loads and normalizes multiple card data sources:

- `characters.json`
- `hybrids.json`
- `wands_equip.json`
- `effects_react.gd`
- `effects_quick.json`
- `traps.json`
- `npcs.json`

The loader stores master dictionaries by id, builds category filters, normalizes ids, categories, effects, passives, status metadata, tags, and public names, and returns deep copies for card queries.

The sampled Godot JSON data includes:

- quick/react effects with `timing`, `effects`, `rr`, `subclass`, notes, and target strings
- equip wands with `timing`, `restrictions`, `effects`, `rr`, and notes
- effect types beyond current Unreal generic payloads, including draw, movement, swap, wall operations, stat modification, rules, on-event directives, and empty/directive-driven effects

Godot `game_state.gd` stores:

- `hands`
- `decks`
- `discards`
- equipped wand arrays on units
- hidden tile markers with owner, reveal flags, card id, and revealed card id
- board units, terrain, walls, statuses, and pending tile spawns

Godot asset manifest documentation states hidden hand and hidden deck use card backs, hidden markers use the same generic marker for trap and NPC, and discard is public with top card face allowed.

Godot rules include hand reveal logic and many hand/deck/discard/equipped behaviors. Those are behavior references only. Unreal should port behavior into deterministic C++ instead of importing Godot files or copying the Godot architecture.

## Gaps Before Production Provider

Known gaps:

- no Unreal-owned CardDB schema/import pipeline
- no production card-zone structs for hand, deck, discard, or equipped
- no production observation scope or player-perspective response model
- no production marker identity model
- no production provider implementation
- no production mapping from Godot effect strings to Unreal generic payloads
- no production target-picker decision model
- no response-window activation model
- no passives/wands/negation support in provider output
- no provider-side version/state snapshot identity
- no contract tests for production zone-backed provider data

## Why Planning-Only

The provider is a boundary piece that will eventually coordinate rules, CardDB, zones, public summaries, and player perspective. Implementing it before the ownership and observation contracts are written would risk leaking hidden information or moving generation into runtime.

This pass documents the future shape so the next implementation milestones can stay small and deterministic.

## Why Production Provider Is Deferred

The production provider should wait until these are explicit:

- Unreal-owned CardDB schema and import/mapping policy
- read-only production zone state structs
- player-perspective observation/scope model
- hidden marker policy
- provider contract extensions for zones
- fixtures for each zone and hidden-info boundary

Until then, runtime should continue consuming externally supplied activation decision data through the existing provider interface and contract tests.
