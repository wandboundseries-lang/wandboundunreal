# Wandbound Unreal Migration Plan

## Current Strategy

- Godot remains the behavior reference.
- Unreal starts as a rules-kernel-first prototype.
- Do not port visuals before rules are testable.

## Phase 1 - Project Setup

- AGENTS.md
- module plan
- build/test command discovery
- Git baseline

## Phase 2 - WandboundCore Rules Kernel

- board state
- unit state
- walls
- actions
- legal action generation
- effect runner
- trace events
- deterministic replay

## Milestone 1 - Board State and Movement

- WandboundCore module added
- pure data state added
- movement legality added
- ApplyMove added
- movement trace event added
- automation tests added
- no Actor/Blueprint/UI dependency

## Milestone: Turn Flow + Legal Action Generation

- Implemented state fields:
  - `CurrentPlayer`
  - `PriorityPlayer`
  - `TurnNumber`
  - `Phase`
  - `bGameOver`
  - player `RemainingMP` storage for deterministic future turn setup
- Added basic state helpers:
  - player ID validation
  - current-player lookup
  - priority-player lookup
  - normal/response phase checks
  - `AdvanceTurnBasic`
- Legal actions currently supported:
  - `Move`
  - `EndTurn`
  - `Pass`
  - `PassResponse`
- Deterministic action ordering:
  - units are traversed in stable state order
  - movement destinations are evaluated east, west, south, north
  - `EndTurn` is appended after movement
  - explicit response phase generates `PassResponse` as the sole response action for the priority player
- Intentionally not implemented yet:
  - attack
  - summon
  - equip
  - cast/effects
  - card draw
  - MP dice roll
  - start/end turn triggers
  - full response activations

## Milestone: Turn-Start Resource Setup

- MP model audit result:
  - Godot uses player-level `state.mp_pool[player_id]`.
  - Unreal movement has migrated to player `FWBPlayerStateData::RemainingMP` as rules truth.
  - `FWBUnitState::MPRemaining` remains only as a legacy fixture mirror during migration.
- Implemented deterministic resource setup:
  - `WBEffectRunner::ApplyTurnStartResourceSetup(State, PlayerId, ExplicitMPRoll)`
  - validates explicit MP roll through `WBRules::CanApplyTurnStartResourceSetup`
  - normalizes `CurrentPlayer`, `PriorityPlayer`, and `Phase`
  - records `LastMPRoll`
  - applies player `RemainingMP`
  - resets owned unit `AttacksLeft` from `MaxAttacksPerTurn`
  - resets `WallsLeft` to `1`
  - resets `WallRemovalsLeft` using the current Godot baseline unlock of `TurnNumber >= 30`
- Added trace event:
  - `turn_start_resource_setup`
  - includes player id, turn number, MP roll, remaining MP, walls left, and wall removals left
- Movement and legal action generation now use player `RemainingMP`.
- Added GodotCanon fixtures:
  - `turn_start_resource_setup_roll_4.json`
  - `turn_start_resource_setup_invalid_roll_fails.json`
  - `turn_start_resource_setup_resets_attacks_and_walls.json`
- Intentionally not implemented yet:
  - random dice roll generation
  - draw
  - status ticks
  - start-of-turn triggers
  - MP modifier hooks
  - NPC phase

## Milestone: Start-of-Turn Status Tick Scaffolding

- Status model audit result:
  - Godot stores status instances in `statuses_by_unit`.
  - `poison` ticks at `turn_start`.
  - `burn`, `root`, and `stun` tick/decay at `turn_end`.
  - Godot currently ignores the `active_player_id` argument to `tick_statuses`, so this Unreal pass ticks all units whose status timing matches start turn.
- Unreal status representation now keeps `FWBUnitState::Statuses` as authoritative and adds optional `StatusTurnsRemaining` metadata.
- Added status helper APIs on `FWBUnitState` for active checks, add/remove, duration access, and deterministic trace ordering.
- Added deterministic phase API:
  - `WBRules::CanApplyStartOfTurnStatusTicks`
  - `WBEffectRunner::ApplyStartOfTurnStatusTicks`
- Implemented Poison start-turn behavior:
  - MaxHP decreases by one, to a minimum of one.
  - HP clamps down to the new MaxHP.
  - timed Poison decrements and expires at zero.
  - Frozen pauses Poison tick and duration decay.
- Burn is intentionally not implemented at start turn.
- Rooted/Stunned remain movement-blocking and do not decay during start-turn ticks.
- Added trace event support:
  - `start_turn_status_ticks`
  - `status_tick`
  - `status_expired`
- Intentionally not implemented yet:
  - end-turn Burn
  - end-turn Rooted/Stunned duration decay
  - full death/prevention
  - draw
  - random MP generation
  - card triggers
  - NPC phase

## Milestone: End-of-Turn Status Tick Scaffolding

- End-turn status model audit result:
  - `autoload/Game.gd:end_turn_impl` calls `state.tick_statuses("turn_end", current_player)`.
  - Godot currently ignores the `active_player_id` argument to `tick_statuses`, so this Unreal pass ticks all units whose status timing matches end turn.
  - `burn`, `root`, `stun`, and `frozen` use `turn_end` timing.
  - Burn damage bypasses armor in Godot and can drive HP to zero before later death/prevention processing.
- Added deterministic phase API:
  - `WBRules::CanApplyEndOfTurnStatusTicks`
  - `WBEffectRunner::ApplyEndOfTurnStatusTicks`
- Implemented Burn end-turn behavior:
  - HP decreases by one.
  - MaxHP is unchanged.
  - HP clamps at zero.
  - timed Burn decrements and expires at zero.
- Implemented timed duration decay/expiration for:
  - `Rooted`
  - `Stunned`
  - `Frozen`
- Poison intentionally does not tick or decay at end turn.
- Added trace support:
  - `end_turn_status_ticks`
  - `status_tick` for Burn
  - `status_expired`
  - `expired_status`
  - `at_or_below_zero_hp`
- Status ticks remain separate deterministic phase operations for now; `ApplyEndTurn` replay behavior was not changed in this pass.
- Intentionally not implemented yet:
  - full death/prevention
  - end-turn card triggers
  - NPC phase
  - response windows
  - card effects

## Milestone: Deterministic Turn Orchestration

- Added an explicit orchestration API:
  - `WBRules::CanApplyDeterministicTurnTransition`
  - `WBEffectRunner::ApplyDeterministicTurnTransition(State, EndingPlayerId, NextPlayerExplicitMPRoll)`
- The composed deterministic sequence is:
  - end-turn status ticks
  - basic turn advancement
  - start-turn status ticks
  - explicit turn-start resource setup
- The orchestration runs on a copied `FWBGameStateData` and assigns back only after every substep succeeds, preventing partial mutation on invalid input.
- Added parent trace event:
  - `turn_transition`
- `turn_transition` records:
  - ending player
  - next player
  - turn number before/after
  - explicit next-player MP roll
- Sub-trace ordering is deterministic:
  - `turn_transition`
  - `end_turn_status_ticks`
  - end-turn `status_tick` / `status_expired`
  - `end_turn`
  - `start_turn_status_ticks`
  - start-turn `status_tick` / `status_expired`
  - `turn_start_resource_setup`
- Existing player-facing `ApplyEndTurn` remains basic and was not wired to the full transition in this pass.
- Intentionally not implemented yet:
  - draw
  - random MP roll
  - start/end turn card triggers
  - NPC phase
  - attacks
  - effects
  - full death/prevention

## Milestone: Turn Command Controller Coverage

- Added a pure C++ command/controller gateway:
  - `WBTurnController::CanApplyTurnCommand`
  - `WBTurnController::ApplyTurnCommand`
- Added command modes:
  - `BasicEndTurn`
  - `DeterministicFullTransition`
- `BasicEndTurn` delegates to the existing player/action-level `ApplyEndTurn` only.
- `BasicEndTurn` ignores `NextPlayerExplicitMPRoll` and does not run status ticks or resource setup.
- `DeterministicFullTransition` delegates to the existing full deterministic turn transition sequence.
- `DeterministicFullTransition` requires an explicit MP roll from `1` to `6`.
- Legal action generation still emits player-facing `EndTurn` only, not full transition commands.
- No action IDs or replay player actions were changed.
- UI/runtime integration is intentionally not wired yet; this pass only makes the low-level command choice explicit and testable.
- This protects player action semantics while giving tests and future runtime code a deterministic full-transition gateway.

## Milestone: Turn Command Replay Fixtures

- Added an explicit replay fixture operation:
  - `operation = apply_turn_command`
- Supported command fixture modes:
  - `basic_end_turn`
  - `deterministic_full_transition`
- `apply_turn_command` fixtures parse `FWBTurnCommand` and call `WBTurnController::ApplyTurnCommand`.
- `apply_action` fixtures continue to use `FWBAction` and `WBActionCodec`.
- `FWBAction` legal action replay remains unchanged:
  - `Move`
  - `EndTurn`
  - `Pass`
  - `PassResponse`
- Full deterministic transition remains test/controller command only and is not emitted by legal action generation.
- Added replay fixture coverage for:
  - basic end turn only
  - full transition with Burn, Poison, and explicit MP setup
  - invalid explicit MP roll with no mutation
  - player `EndTurn` action replay staying basic
- Added trace serialization coverage for full transition command traces.
- No player-facing action semantics changed.
- Intentionally not implemented yet:
  - network replay
  - UI/runtime wiring
  - random MP roll generation
  - draw
  - NPC phase
  - card triggers
  - attacks/combat

## Milestone: Selected Action Runtime Execution Gateway

- Added a pure C++ selected-action execution gateway:
  - `FWBSelectedActionExecutionContext`
  - `WBSelectedActionExecutor::ApplySelectedAction`
- Runtime-selected `Move` delegates to the existing `WBEffectRunner::ApplyMove`.
- Runtime-selected `PassResponse` delegates to the existing `WBEffectRunner::ApplyPassResponse`.
- Runtime-selected `EndTurn` preserves basic behavior by default through `WBEffectRunner::ApplyEndTurn`.
- Runtime-selected `EndTurn` can use the full deterministic turn transition only when explicitly requested with:
  - `bUseFullTurnTransitionForEndTurn = true`
  - a valid `NextPlayerExplicitMPRoll` from `1` to `6`
- Full selected end turn delegates through `WBTurnController::ApplyTurnCommand`.
- Invalid or missing full-transition MP rolls fail without mutation.
- `FWBAction` replay remains unchanged.
- `WBActionCodec` action IDs remain unchanged.
- Legal action generation remains unchanged and does not emit full transition commands.
- Added fixture coverage for:
  - selected basic end turn
  - selected full transition end turn
  - selected full transition invalid roll
  - selected move
  - selected pass response
- Intentionally not implemented yet:
  - UI/Blueprint wiring
  - random MP roll generation
  - draw
  - NPC phase
  - card triggers
  - attacks/combat
  - network replay

## Milestone: Runtime Turn Resolution Adapter

- Added a deterministic MP roll source abstraction:
  - `IWBMPRollSource`
  - `FWBFixedMPRollSource`
  - `FWBQueuedMPRollSource`
- Added a pure C++ runtime-facing adapter:
  - `FWBRuntimeTurnResolutionContext`
  - `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction`
- Runtime-selected `EndTurn` can resolve the full deterministic transition by requesting an explicit MP roll from the configured roll source.
- Runtime-selected `EndTurn` can still execute basic end-turn behavior when full transition resolution is disabled.
- Runtime-selected `Move` and `PassResponse` do not require or consume MP rolls.
- Queued deterministic rolls are consumed only after the next queued value is valid.
- Missing roll sources, empty queues, and invalid rolls fail without mutating game state.
- Added fixture support for:
  - `operation = apply_runtime_selected_action`
- `FWBAction` replay remains unchanged.
- `WBActionCodec` action IDs remain unchanged.
- Legal action generation remains unchanged and does not emit full transition or roll actions.
- Intentionally not implemented yet:
  - random MP roll generation
  - UI/Blueprint wiring
  - draw
  - NPC phase
  - card triggers
  - attacks/combat
  - network replay

## Milestone: Runtime Turn Result Envelope

- Added public turn summary structs:
  - `FWBPublicPlayerTurnSummary`
  - `FWBPublicTurnSummary`
  - `WBPublicTurnSummary::Build`
- Added runtime selected-action result envelope:
  - `FWBRuntimeSelectedActionResult`
  - `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult`
- Existing `ApplyRuntimeSelectedAction` remains available and behavior-compatible by returning the envelope's `ApplyResult`.
- The envelope reports:
  - selected action type
  - selected action id from `WBActionCodec::MakeActionId`
  - whether an MP roll was consumed
  - consumed MP roll value
  - existing apply result and traces
  - final public turn summary
- Public turn summary is limited to turn/player resource state and does not include deck, hand, hidden choices, or private card data.
- Added fixture support for:
  - `operation = apply_runtime_selected_action_with_result`
- `FWBAction` replay remains unchanged.
- `WBActionCodec` action IDs remain unchanged.
- Legal action generation remains unchanged.
- Intentionally not implemented yet:
  - random MP roll generation
  - UI/Blueprint wiring
  - draw
  - NPC phase
  - card triggers
  - attacks/combat
  - network replay

## Milestone - Runtime Result Serialization

- Added deterministic JSON serialization for `FWBRuntimeSelectedActionResult`.
- Added `WBRuntimeResultSerializer` with schema version 1.
- Serialized runtime results include:
  - selected action type/id
  - resolved `ok` state and failure reason
  - consumed MP roll fields
  - emitted trace events
  - final public turn summary
- `WBReplayTrace::TraceEventToJsonObject` is now public so runtime result serialization reuses the same trace field logic as replay traces.
- Final public turn summary serialization includes only turn/player resource fields.
- Hidden card/deck/hand/discard data is excluded.
- `WBActionCodec` action IDs remain unchanged.
- Replay `apply_action` semantics remain unchanged.
- Legal action generation remains unchanged.
- Added serialization fixtures:
  - `runtime_result_serialization_full_transition.json`
  - `runtime_result_serialization_basic_end_turn.json`
  - `runtime_result_serialization_hidden_data_exclusion.json`
- Added optional fixture operation:
  - `apply_runtime_selected_action_with_result_and_serialize`
- Intentionally not implemented yet:
  - random dice generation
  - draw
  - NPC phase
  - attacks/combat
  - card effects
  - UI/Blueprint/3D runtime
  - network replay envelope

## Milestone - Public Board Summary for Runtime Results

- Added deterministic public board summaries:
  - `FWBPublicUnitStatusSummary`
  - `FWBPublicUnitBoardSummary`
  - `FWBPublicBoardSummary`
  - `WBPublicBoardSummary::Build`
- Runtime selected-action results now include `FinalPublicBoardSummary`.
- Runtime result JSON now includes `final_public_board_summary`.
- Public board summary includes visible unit:
  - position
  - owner id
  - card id
  - combat fields
  - public statuses and turns remaining
- Visible board unit card identity is treated as public, matching Godot public unit observations.
- Hidden marker identity remains excluded.
- Deck, hand, and discard contents remain excluded.
- Unit ordering is deterministic by `y`, then `x`, then `unit_id`.
- Status ordering is deterministic by canonical public status id.
- No gameplay, rule, legal-action, `WBActionCodec`, or replay `apply_action` behavior changed.
- Intentionally not implemented yet:
  - marker summaries
  - wall summaries
  - terrain summaries
  - discard viewer/public discard summaries
  - network replay envelope
  - UI/Blueprint/3D runtime

## Phase 3 - Movement

- 9x9 bounds
- orthogonal movement
- MP cost
- wall blocking
- unit blocking
- root/stun checks

## Phase 4 - Attack Declaration

- attacks_left
- AR
- orthogonal LOS
- wall blocking
- stunned/cannot attack
- trace event

## Phase 5 - Damage and Counterattacks

- damage
- HP death
- counter timing
- prevention hooks later

## Phase 6 - Turn Structure

- turn start
- draw
- MP roll
- action phase
- turn end
- status ticks
- NPC phase later

## Phase 7 - Card Data Import

- keep source of truth data-driven
- import from JSON or generated data
- validate card IDs/effects/passives

## Phase 8 - EffectRunner

- effect dispatch
- targeting
- statuses
- stat modification
- healing
- wall effects
- terrain effects

## Phase 9 - Response Windows

- pre-hit
- post-hit
- post-move
- post-summon
- post-effect
- pass priority
- negation

## Phase 10 - AI/Replay/Logs

- observations
- legal actions
- chosen actions
- resolved trace events
- semantic logs
- hidden-information firewall

## Phase 11 - 3D Runtime

- board actor
- tile interaction
- unit actors
- walls
- deck/discard slots
- camera
- UMG

## Phase 12 - Asset Migration

- card art
- slash.png
- 3D models
- materials/textures
- sounds

## First Milestone Definition of Done

- project builds
- WandboundCore module exists
- movement tests pass
- no Actor/Blueprint dependency in rules kernel
