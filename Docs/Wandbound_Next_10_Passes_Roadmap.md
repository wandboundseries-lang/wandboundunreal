# Wandbound Next 10 Passes Roadmap

Date: 2026-06-28

This roadmap lists the recommended next implementation passes after the engine-transfer pivot. It is intentionally production-facing and ordered to create a playable vertical slice without losing deterministic rules discipline.

## Pass 1 - Production-Safe Card Zone State Structs

- Status: implemented as production-safe state/query scaffolding. No gameplay movement or observation model was added.
- Purpose: Add authoritative C++ state for card zones without importing Godot CardDB or changing gameplay behavior.
- Files likely touched: `Source/WandboundCore/Public/`, `Source/WandboundCore/Private/`, `Source/WandboundTests/Private/`, `Docs/Build_Test_Report.md`.
- Guardrails: Do not runtime-load Godot files; do not add UI; do not add production CardDB import; keep runtime from owning mutable rules state.
- Tests expected: zone construction, deterministic ordering, ownership fields, invalid zone references, empty zone behavior.
- Out-of-scope: draw rules, hand play rules, discard visibility policy finalization, equipped behavior.
- Success criteria: Deck, Hand, Discard, Equipped, Board references, and Markers placeholder can be represented deterministically in WandboundCore.

## Pass 2 - Player-Perspective Observation/Public-Private Zone Summary

- Status: implemented as read-only public/player-scoped card zone observation. No gameplay movement, CardDB import, action-codec change, legal-action generation change, or runtime/UI behavior was added.
- Purpose: Add observation APIs that expose only what a viewer is allowed to know.
- Files likely touched: `Source/WandboundCore/Public/`, `Source/WandboundCore/Private/`, `Source/WandboundTests/Private/`, `Docs/Build_Test_Report.md`.
- Guardrails: No hidden opponent hand, deck identity, hidden marker identity, source effect ids, or fixture debug metadata in public output.
- Tests expected: own hand visible to owner, opponent hand hidden, deck identities hidden, public board unchanged, deterministic sort order.
- Out-of-scope: UI view widgets, network replication, full replay dataset schema.
- Success criteria: The same state can produce different safe observations for Player 1, Player 2, and public spectators.
- Implemented notes: public Deck/Hand/Discard summaries are count-only, own Hand and own Discard are owner-private, Deck identity remains hidden for all viewers, equipped identity is count-only, and hidden marker identity is excluded.

## Pass 3 - Minimal Production Card Definition Repository Shell

- Status: implemented as a production-safe in-memory repository shell. No file loading, Godot CardDB import, provider integration, action-codec change, legal-action generation change, or runtime/UI behavior was added.
- Purpose: Create a production-safe query boundary for Unreal-owned card definitions.
- Files likely touched: `Source/WandboundCore/Public/`, `Source/WandboundCore/Private/`, `Source/WandboundTests/Private/`, `Docs/Build_Test_Report.md`.
- Guardrails: Do not import Godot CardDB; do not add broad file loading; do not change `WBActionCodec`; fail closed on missing/unsupported definitions.
- Tests expected: lookup success/failure, duplicate id rejection, deterministic enumeration, unsupported payload diagnostics.
- Out-of-scope: schema migration, manifest suite evaluation, asset import, runtime provider integration.
- Success criteria: A small in-memory or fixture-backed repository can return `FWBCardDefinition` data by card id.
- Implemented notes: repository validation covers repository id, card id, public name, effect id, duplicate ids, supported target requirement, and internal public-label terms; lookup/order helpers are deterministic by `CardId`.

## Pass 4 - Minimal Fixture CardDB Repository Loader, Unreal-Owned Only

- Status: implemented as a narrow Core fixture loader. No Godot CardDB import, provider integration, action-codec change, legal-action generation change, runtime/UI behavior, or effect execution was added.
- Purpose: Load a small Unreal-owned fixture bundle into the repository to support vertical-slice cards.
- Files likely touched: `Source/WandboundCore/`, `Source/WandboundTests/Private/`, documentation under `Docs/`.
- Guardrails: Use Unreal-owned fixture data only; do not read `Reference/GodotProject` at runtime; do not build a full importer; keep fixture schema narrow.
- Tests expected: valid fixture load, malformed fixture failure, unsupported payload failure, deterministic output, hidden-token safety.
- Out-of-scope: production manifest batches, Godot CardDB import, content pipeline integration.
- Success criteria: Vertical-slice card definitions can be loaded from a minimal supported fixture path.
- Implemented notes: `WBCardDefinitionFixtureLoader` loads `Reference/GodotCanon/CardDefinitionRepositoryFixtures/`, maps the existing `FWBCardDefinition` shape, supports damage/heal/status/armor payload families, reuses repository validation, and emits sanitized expected export snapshots.

## Pass 5 - Production Activation Data Provider Skeleton

- Status: implemented as a provider skeleton with board-source and own-hand activation decisions. Unit target options are now implemented; tile and wall targets remain deferred. No effect execution, CardDB import, `FWBAction` activation integration, `WBActionCodec` change, or normal legal action generation change was added.
- Purpose: Add the first production-shaped provider implementation that supplies runtime with externally owned decision data.
- Files likely touched: `Source/WandboundCore/`, `Source/WandboundRuntime/`, `Source/WandboundTests/Private/`, `Docs/`.
- Guardrails: Runtime remains a consumer; provider does not live in UI; activation remains separate from `FWBAction`; `WBRules::GenerateLegalActions` and `WBActionCodec` remain unchanged.
- Tests expected: provider current-decision output, provider post-action output, missing dependency diagnostics, hidden-info exclusion, runtime no-generation source guards.
- Out-of-scope: hand-source activation, draw/discard loop, response windows.
- Success criteria: Existing runtime facade can refresh from a production-shaped provider result.
- Implemented notes: `FWBProductionActivationDataProvider` emits board and own-hand source/effect activation decision data from const state, card-zone observation, and card definition repository input. Hidden opponent hand, deck identity, and marker internal identity stay excluded.

## Pass 6 - Production Activation Unit Target Options

- Status: implemented as read-only unit target-option enumeration for board-source and own-hand provider activations.
- Purpose: Connect visible board units and card definitions to provider-owned activation target choices.
- Files likely touched: `Source/WandboundCore/`, `Source/WandboundRuntime/`, `Source/WandboundTests/Private/`, `Docs/`.
- Guardrails: Use board-visible public card identity only; no hidden hand/deck reads; no UI target picker; no response windows; do not execute effects.
- Tests expected: board-source unit targets, own-hand unit targets, tile/wall deferred, deterministic ordering, hidden metadata excluded, runtime session consumes external options.
- Out-of-scope: activation selection/execution, summon, equip, draw/discard.
- Success criteria: Board-source and own-hand activation decisions expose deterministic visible unit target options through production-shaped provider output.
- Implemented notes: Unit target options are attached to `FWBCardActivationLegalAction` data, sorted by owner/tile/unit/card, and carried through the runtime session as external data.

## Pass 7 - Activation Unit Target Selection Bridge

- Status: implemented as a C++ bridge that binds provider-supplied unit target options to copied activation commands. No UI, response windows, effect execution, `FWBAction`, `WBActionCodec`, or normal legal-action generation changes were added.
- Purpose: Bind a selected provider-supplied unit target option to an activation command so the C++ vertical slice can execute a chosen target.
- Files likely touched: `Source/WandboundCore/`, `Source/WandboundRuntime/`, `Source/WandboundTests/Private/`, `Docs/`.
- Guardrails: Use provider-owned target option data only; no UI widgets; no response windows; keep activation separate from normal `FWBAction` unless explicitly changed.
- Tests expected: target option selection succeeds/fails deterministically, command target is filled, hidden data remains excluded, no `WBActionCodec` change.
- Out-of-scope: draw, discard movement after play, response windows, target-picker UI.
- Success criteria: A C++ harness can select a unit target option and produce an executable activation command without runtime target computation.
- Implemented notes: Board-source and own-hand unit target selection succeed, no-target activations succeed without a target, tile/wall targets fail closed, hidden zones remain hidden, and source guards prove the bridge does not call rules generation, EffectRunner, or ActionCodec.

## Pass 8 - Production Activation Execution Handoff

- Status: implemented as a narrow C++ provider -> selection -> execution handoff -> provider refresh path. No UI, response windows, `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` changes were added.
- Purpose: Prove provider-owned board/hand activations can execute through the existing runtime activation command path and refresh provider data afterward.
- Files likely touched: `Source/WandboundRuntime/`, `Source/WandboundTests/Private/`, `Docs/`.
- Guardrails: Use the existing target-selection bridge and existing runtime execution bridge; do not call `WBEffectRunner` directly from the production adapter; do not mutate repositories; do not expose debug activation ids, usage keys, opponent hand identity, deck identity, or hidden marker identity.
- Tests expected: missing inputs, selection failure, board-source execution, own-hand execution, no-target current failure behavior, tile/wall unsupported, provider refresh, no repository mutation, no pre-execution mutation, hidden-info scans, source guards.
- Out-of-scope: UI target picking, response windows, draw/discard movement, summon, equip, Godot CardDB import.
- Success criteria: A C++ harness can run provider entry -> selected target -> handoff execution -> provider refresh.
- Implemented notes: `FWBProductionActivationExecutionHandoff` rebuilds internal command metadata from `FWBCardDefinitionRepository`, executes through `WBRuntimeActivationExecutionBridge`, and refreshes a fresh production provider after success.

## Pass 9 - Draw/Hand/Discard Movement Loop

- Status: implemented as deterministic Core card lifecycle movement plus production hand-source post-resolution discard integration. Turn-start draw is exposed as a helper only.
- Purpose: Add deterministic card movement for turn-start draw and post-activation movement where supported.
- Files likely touched: `Source/WandboundCore/`, `Source/WandboundTests/Private/`, `Reference/GodotCanon/GoldenScenarios/` only if canon fixtures are explicitly needed, `Docs/`.
- Guardrails: Do not add random shuffle; keep deck order explicit in tests; confirm discard visibility policy before public exposure.
- Tests expected: draw from deck to hand, first-player first-turn exception, empty deck behavior, played card to discard where supported, trace coverage.
- Out-of-scope: full CardDB import, response discard timing, graveyard/death-trigger interaction.
- Success criteria: A deterministic vertical-slice turn can move cards through Deck, Hand, and Discard.
- Implemented notes: `WBCardLifecycle` draws deterministically from Deck to Hand, supports setup draw and first-player first-turn skip, moves used Hand-source activations to Discard after successful production execution, keeps provider refresh after that movement, and leaves `WBActionCodec` and `WBRules::GenerateLegalActions` unchanged.

## Pass 10 - Summon/Equip/RL Foundation

- Status: implemented as minimal production metadata, read-only RL/RR helper, and read-only own-hand summon/equip option generation. Execution remains future work.
- Purpose: Decide and begin the next production mechanic after the board/hand activation slice is stable.
- Files likely touched: `Docs/`, then likely `Source/WandboundCore/`, `Source/WandboundTests/Private/`, and provider integration files if implementation begins.
- Guardrails: Consult Rules Bible v2 and Godot reference first; do not copy Godot architecture blindly; keep Rules/EffectRunner/UI boundaries intact.
- Tests expected: if planning-only, no source tests; if implementation, summon adjacency/unit cap or equip/RL availability tests with replay traces.
- Out-of-scope: full wand destruction, response windows, marker/NPC phase unless explicitly selected.
- Success criteria: The project has a clear next mechanic route, or the first focused summon/equip/RL foundation slice lands with deterministic tests.
- Implemented notes: `FWBCardDefinition` now carries Character HP/ATK/AR/RL and Wand RR metadata, `WBResonanceLoad` reports available RL without mutation, and `FWBProductionSummonEquipDataProvider` emits hidden-info-safe summon/equip options from `OwnHand` only. No summon/equip execution, overflow, `FWBAction`, `WBActionCodec`, `WBRules::GenerateLegalActions`, UI, or Godot CardDB import was added.
