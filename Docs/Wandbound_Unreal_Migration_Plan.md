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

## Milestone - Attack Declaration Legality

- Added `WBRules::CanDeclareAttack` for deterministic attack declaration legality.
- Added orthogonal range/LOS helpers:
  - `OrthogonalDistance`
  - `AreTilesOrthogonallyAligned`
  - `HasOrthogonalLineOfSight`
- Implemented baseline attack declaration checks:
  - current normal-turn priority
  - attacker and defender existence
  - attacker ownership
  - enemy or neutral target
  - `AttacksLeft > 0`
  - Stunned/Frozen/CannotAttack/no_attack restrictions
  - orthogonal alignment
  - AR range
  - wall and intervening-unit LOS blocking
- Added `WBEffectRunner::ApplyAttackDeclare`.
- Added `Attack` selected-action execution.
- Added attack legal action generation after movement and before EndTurn.
- Added `WBActionCodec` attack serialization and stable IDs using `attack:p{player}:u{source}:t{target}`.
- Runtime selected-action result serialization now reports selected attack IDs, `attack_declared` traces, and public board summaries after declaration.
- Added GodotCanon attack declaration and legal action fixtures.
- Intentionally still future work:
  - damage
  - Frozen break-on-hit resolution
  - pre-hit/post-hit responses
  - counterattacks
  - attack passives
  - wands
  - terrain attack modifiers
  - UI/VFX/runtime presentation
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

## Milestone - Attack Damage Resolution

- Added deterministic pending attack state:
  - attacker unit id
  - defender unit id
  - attacking player id
  - declaration tiles
  - declaration action id
- Attack declaration now stages pending attack data after spending `AttacksLeft`.
- Attack declaration fails while another pending attack is active.
- Added deterministic attack damage resolution through `WBEffectRunner::ApplyPendingAttackDamage`.
- Damage uses the current attacker's non-negative `ATK` and clamps defender HP at zero.
- Frozen defenders break before damage:
  - `Frozen` is removed
  - no HP damage is applied
  - pending attack is cleared
- Friendly targets remain illegal except for the Godot-confirmed friendly Frozen break case.
- Added `attack_damage_resolved` replay traces.
- Added `status_removed` traces for Frozen break resolution.
- Added `damage_amount` replay trace serialization.
- Pending attack state is intentionally excluded from public runtime summaries.
- Legal action generation does not expose attack damage resolution as a player-selectable action.
- Intentionally not implemented yet:
  - pre-hit/post-hit response windows
  - armor/prevention/replacement effects
  - counters
  - passives
  - wands
  - terrain attack modifiers
  - UI/Blueprint/3D runtime

## Milestone - Zero-HP Death/Removal Scaffolding

- Added defeated/removed state to units:
  - `bDefeated`
  - `bRemovedFromBoard`
- Added `WinnerPlayerId` for simple public game-over outcome reporting.
- Added `WBEffectRunner::ApplyZeroHPDeathRemoval`.
- Zero-HP cleanup marks units defeated, removes them from board occupancy, and preserves unit records for replay/debug.
- Non-Hero zero-HP units emit:
  - `unit_defeated`
  - `unit_removed_from_board`
- Simple no-replacement Hero loss sets `bGameOver`, sets `WinnerPlayerId`, and emits `hero_defeated`.
- Lethal attack damage now appends zero-HP cleanup after `attack_damage_resolved`.
- Removed units no longer:
  - occupy tiles
  - block movement or LOS
  - move
  - attack
  - become attack targets
  - appear in generated legal actions
  - appear in public board summaries
- Runtime public turn summary JSON includes `winner_player_id`.
- Intentionally not implemented yet:
  - prevention
  - replacement
  - Hybrid Hero replacement
  - death triggers
  - on-destroy buffs
  - counters/responses
  - passives
  - wands
  - cards/effects
  - discard movement
  - UI/Blueprint/3D runtime

## Milestone - Damage and Death Prevention Scaffolding

- Godot audit found generic armor state in `current_armor` and `max_armor`, plus card-specific prevention/replacement embedded in `apply_damage` and game-controller flows.
- Added `WBDamageResolution` as the future damage-prevention and armor seam.
- Added `WBDeathResolution` as the future death-prevention and replacement seam.
- Attack damage now routes through `WBDamageResolution::ResolveDamageRequest`.
- Direct damage resolution applies final damage, clamps HP at zero, and does not remove units.
- Zero-HP cleanup now checks `WBDeathResolution::EvaluateDeathPrevention` before defeat/removal.
- Current gameplay behavior is unchanged because default damage and death prevention both return no prevention.
- Frozen break behavior remains unchanged and is not treated as prevention.
- Added replay trace fields for prevention defaults:
  - `damage_prevented`
  - `prevented_damage_amount`
  - `final_damage_amount`
  - `prevention_reason`
- Armor was documented as future work in this pass and is now handled by the later Generic Armor Damage Handling milestone.
- Intentionally not implemented yet:
  - card-specific prevention
  - active armor gameplay
  - Oathchain
  - Backfill
  - Juno
  - Hybrid Hero replacement
  - death triggers
  - discard movement
  - equipped wand fallout
  - responses/counters/passives/wands
  - UI/Blueprint/3D runtime

## Milestone - Generic Armor Damage Handling

- Added generic armor fields to `FWBUnitState`:
  - `CurrentArmor`
  - `MaxArmor`
- Added armor helper APIs:
  - `GetCurrentArmor`
  - `GetMaxArmor`
  - `SetArmorForTest`
- `WBDamageResolution::ResolveDamageRequest` now applies non-bypass damage through armor before HP loss.
- Bypass damage ignores armor and applies HP damage directly.
- Attack damage is non-bypass and uses stable damage cause `Attack`.
- Burn status damage routes through damage resolution as bypass damage with stable damage cause `Burn`.
- Poison remains start-turn MaxHP reduction and does not use damage or armor.
- Added trace fields for armor and actual HP loss:
  - `previous_armor`
  - `new_armor`
  - `armor_absorbed_amount`
  - `bypassed_armor`
  - `hp_damage_amount`
  - `damage_cause`
- Public board summaries and runtime result JSON now serialize:
  - `current_armor`
  - `max_armor`
- Action IDs remain unchanged.
- Legal action generation remains unchanged by armor.
- Added GodotCanon fixtures and `Wandbound.Core.GenericArmor.*` automation coverage.
- Intentionally not implemented yet:
  - armor-granting cards
  - card-specific prevention
  - card-specific replacement
  - Oathchain
  - Backfill
  - Juno
  - Hybrid Hero replacement
  - death triggers
  - discard movement
  - equipped wand fallout
  - responses/counters/passives/wands
  - UI/Blueprint/3D runtime

## Milestone - Armor Effect Scaffolding

- Added pure C++ generic armor effect scaffolding:
  - `EWBArmorEffectOp`
  - `FWBArmorEffectRequest`
  - `FWBArmorEffectResult`
  - `WBArmorEffect::ApplyArmorEffect`
- Supported deterministic armor operations:
  - add/reduce/set current armor
  - add/reduce/set max armor
  - restore current armor to max
- Added `WBEffectRunner::ApplyArmorEffect`.
- Added `armor_modified` trace events with:
  - previous/new current armor
  - previous/new max armor
  - armor effect operation
  - armor effect amount
- Added replay trace serialization for:
  - `armor_effect_operation`
  - `armor_effect_amount`
  - `previous_max_armor`
  - `new_max_armor`
- Public board/runtime summaries already serialize current/max armor and now reflect armor-effect mutations.
- Added fixture support for `operation = apply_armor_effect`.
- Legal action generation remains unchanged.
- `WBActionCodec` action IDs remain unchanged.
- Intentionally not implemented yet:
  - card-specific armor cards
  - card database import
  - target selection UI
  - effect activation timing
  - responses/counters/passives/wands
  - UI/Blueprint/3D runtime

## Milestone - Card Effect Request Scaffolding

- Added deterministic generic card-effect request transport:
  - `EWBGenericEffectOp`
  - `FWBEffectSourceRef`
  - `FWBEffectTargetRef`
  - `FWBGenericEffectPayload`
  - `FWBEffectRequest`
  - `FWBEffectRequestResult`
- Added `WBRules::CanApplyEffectRequest`.
- Added `WBEffectRunner::ApplyEffectRequest`.
- Supported only one generic payload operation:
  - `armor_effect`
- `armor_effect` payloads route to existing `WBEffectRunner::ApplyArmorEffect`.
- Multi-payload requests are atomic:
  - validate first
  - apply to a working state copy
  - assign back only if all payloads succeed
  - failures return no success traces and no mutation
- Added parent `effect_request_resolved` traces before child payload traces.
- Parent traces use only safe fields: player id, source unit id, target unit id, ok.
- Source card id and source effect id are fixture/debug metadata only and are not serialized into traces or public summaries.
- Added fixture support for:
  - `operation = apply_effect_request`
- Legal action generation remains unchanged.
- `WBActionCodec` action IDs remain unchanged.
- Intentionally not implemented yet:
  - Godot CardDB import
  - JSON card database loading
  - real card activation timing
  - effect legal action generation
  - target selection UI
  - response windows
  - effect negation
  - passives
  - wands
  - card-specific effects
  - UI/Blueprint/3D runtime

## Milestone - Status Effect Request Scaffolding

- Added deterministic generic status effect operations:
  - `apply_status`
  - `remove_status`
  - `set_status_duration`
  - `add_status_duration`
  - `reduce_status_duration`
  - `cleanse_status`
  - `cleanse_all_statuses`
- Added `WBStatusEffect`.
- Added canonical public status aliases for:
  - `Burn`
  - `Poison`
  - `Rooted`
  - `Stunned`
  - `Frozen`
- Added standalone `WBEffectRunner::ApplyStatusEffect`.
- Added `status_modified` traces.
- Added replay trace serialization for:
  - `status_effect_operation`
  - `removed_statuses`
- Extended `FWBEffectRequest` with `status_effect` payload support alongside `armor_effect`.
- `status_effect` payloads route to `WBEffectRunner::ApplyStatusEffect`.
- Multi-payload effect requests remain atomic through working-state dispatch.
- Added fixture support for:
  - `operation = apply_status_effect`
  - `status_effect` payloads inside `operation = apply_effect_request`
- Added standalone status fixtures and mixed armor/status effect request fixtures under `Reference/GodotCanon/GoldenScenarios/`.
- Status operations are fixture-owned only in this milestone.
- Existing status tick behavior remains unchanged.
- Legal action generation remains unchanged except when status state itself changes existing legality, such as Rooted or Stunned.
- `WBActionCodec` action IDs remain unchanged.
- Intentionally not implemented yet:
  - Godot CardDB import
  - JSON card database loading
  - real card activation timing
  - effect legal action generation
  - target selection UI
  - response windows
  - effect negation
  - passives
  - wands
  - card-specific status effects
  - UI/Blueprint/3D runtime

## Milestone - Damage/Heal Effect Request Scaffolding

- Added deterministic generic damage effect scaffolding:
  - `FWBDamageEffectRequest`
  - `FWBDamageEffectResult`
  - `WBDamageEffect::ApplyDamageEffect`
- Added deterministic generic heal effect scaffolding:
  - `FWBHealEffectRequest`
  - `FWBHealEffectResult`
  - `WBHealEffect::ApplyHealEffect`
- Added `WBEffectRunner::ApplyDamageEffect`.
- Added `WBEffectRunner::ApplyHealEffect`.
- `damage_effect` uses `WBDamageResolution` with `DamageKind = Effect`.
- Generic effect damage uses armor by default and can explicitly bypass armor through fixture-owned `bBypassArmor`.
- Lethal damage effects run existing zero-HP cleanup through the EffectRunner wrapper.
- `heal_effect` restores HP up to `MaxHP`, reports effective healing, does not change max HP, and does not revive removed/defeated units.
- Extended `FWBEffectRequest` with `damage_effect` and `heal_effect` payload support alongside `armor_effect` and `status_effect`.
- Mixed `armor_effect` / `status_effect` / `damage_effect` / `heal_effect` requests remain atomic through working-state dispatch.
- Added `damage_effect_resolved` and `heal_effect_resolved` traces.
- Added replay trace serialization for:
  - `heal_amount`
  - `effective_heal_amount`
- Added fixture support for:
  - `operation = apply_damage_effect`
  - `operation = apply_heal_effect`
  - `damage_effect` payloads inside `operation = apply_effect_request`
  - `heal_effect` payloads inside `operation = apply_effect_request`
- Legal action generation remains unchanged.
- `WBActionCodec` action IDs remain unchanged.
- No Godot CardDB import, card activation, target selection UI, response windows, effect negation, passives, wands, card-specific effects, UI, Blueprint, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Command Scaffolding

- Added deterministic card activation command scaffolding:
  - `FWBCardActivationSource`
  - `FWBCardActivationCommand`
  - `FWBCardActivationCommandResult`
- Added `WBRules::CanApplyCardActivationCommand`.
- Added `WBEffectRunner::ApplyCardActivationCommand`.
- The command consumes externally supplied `FWBEffectRequest` data and delegates mutation to the existing effect request dispatcher.
- Missing effect-request source metadata is filled from the command source before dispatch.
- Source-less/global fixture effects are allowed when the command has a valid player and `SourceUnitId == -1`.
- Successful activation traces emit:
  - `card_activation_resolved`
  - `effect_request_resolved`
  - child effect traces in payload order
- Source card id, source effect id, and debug activation id remain fixture/debug metadata and are excluded from traces and public runtime summaries.
- Added fixture support for:
  - `operation = apply_card_activation_command`
- Added GodotCanon fixtures for armor, status, damage, heal, mixed atomic success/failure, source failures, missing target, and hidden metadata exclusion.
- Legal action generation remains unchanged.
- `WBActionCodec` action IDs remain unchanged.
- No Godot CardDB import, card activation legal action generation, costs, timing windows, target selection UI, response windows, effect negation, passives, wands, card-specific effects, UI, Blueprint, `.uasset`, or `.umap` work was added.

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

## Milestone - Public Wall and Terrain Summary for Runtime Results

- Added public wall summary structs and serialization:
  - `FWBPublicWallEdgeSummary`
  - `final_public_board_summary.walls`
- Wall summaries include normalized endpoints and orientation.
- Wall ordering is deterministic by `ay`, `ax`, `by`, `bx`, then `orientation`.
- Existing wall state and movement blocking behavior were not changed.
- Added minimal public terrain state for reporting/test scaffolding:
  - `FWBGameStateData::DefaultTerrainId`
  - `FWBGameStateData::TerrainByTileIndex`
  - `GetTerrainAt`
  - `SetTerrainForTest`
  - `ClearTerrainForTest`
- Added public terrain summary structs and serialization:
  - `FWBPublicTerrainTileSummary`
  - `final_public_board_summary.default_terrain_id`
  - `final_public_board_summary.terrain_tiles`
- Terrain summaries are sparse and include only tiles whose terrain differs from `DefaultTerrainId`.
- Unreal uses `Normal` as the public default terrain id while Godot's reference default is `"none"`.
- Wall and terrain summaries are reporting-only.
- No movement, LOS, wall placement/removal, terrain movement, or terrain effect gameplay rules changed.
- Hidden markers remain excluded because Unreal marker state is not modeled yet.
- Deck, hand, discard, pending choices, and private marker identity remain excluded.
- Intentionally not implemented yet:
  - marker summaries
  - wall placement/removal actions
  - wall action traces
  - terrain movement rules
  - terrain effect rules
  - network replay envelope
  - UI/Blueprint/3D runtime

## Milestone - Runtime Visual Scaffold

- Added `WandboundRuntime` module.
- Added `WBBoardViewTypes` board-to-world coordinate helpers.
- Added `AWBBoardViewActor`.
- Added `WBBoardViewDemoData` for manual visual smoke testing.
- The board view actor consumes `FWBPublicBoardSummary` only.
- Placeholder rendering covers:
  - tiles
  - public units
  - public walls
  - public terrain tiles
- Mesh references are editable and null by default.
- Null meshes skip instance creation without crashing.
- Render count accessors expose intended public-summary counts for tests and future runtime wiring.
- No gameplay rules, legal action generation, input, selection, UI, animation, VFX, sound, camera logic, CardDB import, or marker visuals were added.
- No `.uasset`, `.umap`, Blueprint, UMG, material, mesh, or level assets were created or edited.
- Hidden deck, hand, discard, pending choices, and marker identity remain excluded by consuming public summaries only.

## Milestone - Runtime Board Summary Bridge

- Added `WBBoardSummaryBridge`.
- Added `FWBBoardViewRefreshResult`.
- Bridge path:
  - rules state
  - `WBPublicBoardSummary::Build`
  - `FWBPublicBoardSummary`
  - `AWBBoardViewActor::RenderPublicBoardSummary`
- The bridge consumes public summary data for rendering and does not store rules state.
- `RenderStateToBoardView` accepts a const state only to use the existing public-summary conversion.
- Hidden deck, hand, discard, pending choices, and marker identity remain excluded.
- No gameplay mutation, legal action generation, input, selection, UI, camera, animation, VFX, sound, Blueprints, UMG, or asset work was added.

## Milestone - Runtime Demo Harness Actor

- Added `AWBBoardViewDemoHarnessActor`.
- The harness can manually render `WBBoardViewDemoData` through `WBBoardSummaryBridge`.
- `bRenderDemoOnBeginPlay` defaults false.
- `bFindBoardViewActorIfMissing` defaults false and only finds an existing board view actor when enabled.
- The harness consumes public summary data only.
- No gameplay mutation, legal action generation, hidden information access, input, selection, UI, camera, animation, VFX, sound, Blueprints, UMG, `.uasset`, `.umap`, or asset work was added.

## Milestone - Runtime Board View State Applier

- Added `UWBBoardViewStateApplierComponent`.
- The component caches and applies `FWBPublicBoardSummary`.
- It can build a public summary from const rules state through `WBBoardSummaryBridge`.
- It stores public summary data only, not full `FWBGameStateData`.
- It applies the latest summary to an assigned `AWBBoardViewActor`.
- It does not mutate game state or generate actions.
- It does not add input, selection, UI, camera, animation, VFX, sound, Blueprints, UMG, `.uasset`, `.umap`, or asset work.

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

## Milestone - Runtime Visual Controller Shell

- Added `UWBRuntimeVisualControllerComponent` as a C++ visual controller shell.
- The shell accepts `FWBPublicBoardSummary` directly.
- The shell accepts `FWBRuntimeSelectedActionResult` only to read `FinalPublicBoardSummary`.
- It does not own, cache, or mutate rules state.
- It does not generate legal actions.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Selected Action Visual Harness

- Added `WBSelectedActionVisualHarness` as a C++ selected-action-to-visual-refresh seam.
- It executes externally supplied selected actions through `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult`.
- It refreshes the visual controller from the runtime result's `FinalPublicBoardSummary`.
- It accepts external mutable state by reference but does not own or cache state.
- It does not generate legal actions.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Runtime Controller Facade

- Added `UWBRuntimeControllerFacadeComponent` as a C++ runtime controller facade.
- It accepts externally selected `FWBAction` values and an external `FWBRuntimeTurnResolutionContext`.
- It delegates selected-action execution and refresh policy to `WBSelectedActionVisualHarness`.
- It stores the latest `FWBRuntimeSelectedActionResult` and `FWBBoardViewRefreshResult` for future runtime/UI consumers.
- It does not own or cache `FWBGameStateData`.
- It does not generate legal actions or call `WBRules` legality APIs.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Runtime Action Selection Bridge

- Added `WBRuntimeActionSelectionBridge` as a pure C++ action-selection bridge.
- It resolves selected `WBActionCodec` action IDs from externally supplied legal action lists.
- It delegates resolved actions to `UWBRuntimeControllerFacadeComponent`.
- It treats missing IDs as `selected_action_id_not_found`.
- It treats duplicate matching IDs as `selected_action_id_ambiguous`.
- It requires a non-null runtime controller facade for execution.
- It does not generate legal actions.
- It does not decide legality or call `WBRules` legality APIs.
- It does not own or cache `FWBGameStateData`.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Runtime Legal Action Presentation Snapshot

- Added `WBRuntimeLegalActionPresentation` as a pure C++ presentation snapshot builder.
- It consumes externally supplied legal actions and `FWBPublicBoardSummary`.
- It emits stable action IDs and simple public labels for future UI.
- It preserves supplied action order and does not sort or filter actions.
- It copies public source/target card ids only from the public board summary.
- It treats duplicate action id lookup as ambiguous.
- It does not generate legal actions.
- It does not decide legality or call `WBRules` legality APIs.
- It does not own or cache `FWBGameStateData`.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Runtime Turn Interaction Model

- Added `UWBRuntimeTurnInteractionModelComponent` as a C++ runtime interaction model.
- It stores externally supplied legal actions.
- It stores a public board summary and presentation snapshot.
- It exposes read-only access to current presentation entries.
- It executes selected action ids through `WBRuntimeActionSelectionBridge` and the runtime controller facade path.
- It stores last selected action id, selection result, and execution result.
- It does not regenerate legal actions after execution; stored interaction state may be stale until explicitly refreshed.
- It does not generate legal actions.
- It does not decide legality or call `WBRules` legality APIs.
- It does not own or cache `FWBGameStateData`.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Runtime Interaction Refresh Adapter

- Added `WBRuntimeInteractionRefreshAdapter` as a C++ decision-point refresh adapter.
- It accepts externally generated legal actions and a public board summary.
- It updates the turn interaction model with the supplied legal actions and summary.
- It can update the visual controller from the same public summary.
- It uses validation-first behavior so missing required targets do not partially refresh the other target.
- It replaces stale turn interaction model legal actions and snapshots when a future runtime owner supplies fresh data.
- It does not generate actions.
- It does not decide legality or call `WBRules` legality APIs.
- It does not own or cache `FWBGameStateData`.
- It does not execute actions.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Runtime Decision-Point Coordinator

- Added `UWBRuntimeDecisionPointCoordinatorComponent` as a C++ read-only decision-point coordinator.
- It accepts externally generated legal actions and public board summaries.
- It refreshes the interaction model and visual controller through `WBRuntimeInteractionRefreshAdapter`.
- It exposes read-only current interaction status.
- It exposes current presentation snapshots and presentation entry lookup for future UI.
- It resets current status on failed refresh to avoid stale UI state.
- It can clear the current decision point and clear the turn interaction model when available.
- It does not generate actions.
- It does not decide legality or call `WBRules` legality APIs.
- It does not implement action execution directly.
- It does not own or cache `FWBGameStateData`.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Decision-Point Selected-Action Handoff

- Added C++ selected-action handoff to `UWBRuntimeDecisionPointCoordinatorComponent`.
- The coordinator accepts a selected action id, external `FWBGameStateData&`, external runtime context, and external runtime controller facade.
- It requires a current decision point.
- It delegates selected-action execution to the turn interaction model.
- It records last selected-action status and the full delegated execution result.
- It does not generate actions.
- It does not decide legality or call `WBRules` legality APIs.
- It does not call `WBEffectRunner` directly.
- It does not own or cache `FWBGameStateData`.
- It does not refresh legal actions automatically after execution.
- It does not inspect hidden deck, hand, discard, pending-choice, or marker identity data.
- It does not add input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Runtime Decision-Loop Test Harness

- Added a test-only C++ decision-loop harness under `WandboundTests`.
- The harness simulates an external rules/runtime owner by generating legal actions and public summaries in tests only.
- It verifies the refresh -> execute -> refresh loop across two decision points.
- It proves stale legal action presentation remains stale after execution until an explicit `RefreshDecisionPoint` call replaces it.
- It covers full EndTurn with deterministic MP roll, next-player refresh, visual refresh participation, and hidden data exclusion.
- Production runtime still does not generate legal actions.
- Production runtime still does not decide legality, call `WBRules`, call `WBEffectRunner` directly, own rules state, or inspect hidden state.
- No input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Decision-Point Owner Shell

- Added `UWBRuntimeDecisionPointOwnerComponent` as a production C++ owner shell.
- It accepts externally generated legal actions and public board summaries.
- It refreshes `UWBRuntimeDecisionPointCoordinatorComponent`.
- It executes selected action ids through the coordinator.
- It supports explicit post-action refresh from externally supplied post-action data.
- It does not generate actions.
- It does not decide legality.
- It does not call `WBRules` or `WBEffectRunner`.
- It does not own or cache `FWBGameStateData`.
- It does not add input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Fixture Card Definition Activation Expansion

- Added fixture-owned `FWBCardDefinition` and `FWBCardEffectDefinition` scaffolding.
- Added `WBCardActivationExpansion` to expand a requested fixture card effect plus target into `FWBCardActivationCommand`.
- It supports armor/status/damage/heal generic payload definitions.
- It can build candidates deterministically by effect order then supplied target order.
- It can drive fixture-only build and build-and-apply golden scenarios.
- It does not import Godot CardDB.
- It does not production-load external card JSON.
- It does not generate legal card activation actions.
- It does not decide card ownership, costs, timing, response windows, passives, wands, or card-specific behavior.
- It does not call `WBRules`, `WBEffectRunner`, `GenerateLegalActions`, or `ApplyCardActivationCommand` from the expansion layer.
- It does not add input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work.

## Milestone - Card Activation Candidate Generation

- Added `FWBCardActivationCandidate`, `FWBCardActivationCandidateSource`, and `FWBCardActivationCandidateGenerationResult`.
- Added `WBCardActivationCandidateGenerator`.
- Fixture-owned definitions can now produce deterministic activation candidates from externally supplied target refs.
- Each candidate wraps an `FWBCardActivationCommand` for explicit later application.
- Candidate order is source order, activated-effect order, then target order.
- Dynamic invalid sources/targets are skipped; malformed fixture/card input fails clearly.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- `WBActionCodec` remains unchanged and does not encode activation candidate ids.
- No Godot CardDB import, production card loading, real activation legal actions, card zones, costs, response windows, UI target selection, passives, wands, or card-specific behavior were added.
- No input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Legal Action Family

- Added `FWBCardActivationLegalAction`, `FWBCardActivationLegalActionSet`, and `FWBCardActivationLegalActionGenerationResult`.
- Added `WBCardActivationLegalActionGenerator`.
- Added activation-only presentation snapshot helpers.
- Fixture-owned activation candidates can now be converted into a separate activation legal action family.
- Activation action ids use candidate ids and are not `WBActionCodec` ids.
- Public labels use candidate labels or `Activate` and reject raw internal operation/schema/hook names.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- Applying an activation still requires explicit `WBEffectRunner::ApplyCardActivationCommand`.
- No Godot CardDB import, production card loading, `EWBActionType::Activate`, `FWBAction` activation, UI target selection, response windows, negation, passives, wands, or card-specific behavior were added.
- No input, UI widgets, camera behavior, animation, VFX, audio, assets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Source Gates

- Added `FWBCardActivationSourceGateDefinition`.
- Added `FWBCardActivationSourceGateContext`.
- Added pure C++ `WBCardActivationSourceGate::Evaluate`.
- Added fixture-only activation usage keys on `FWBGameStateData`.
- Source gates check fixture source zones, normal-turn priority timing, source unit ownership, Stunned/Frozen policy, externally satisfied cost flags, and once-per-turn usage.
- Candidate generation filters activated effects through source gates before creating candidates.
- Legacy default candidate filtering remains compatible with old fixtures.
- Usage keys clear during turn-start resource setup.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No Godot CardDB import, production card zones, real cost payment, activation `FWBAction`, response windows, negation, passives, wands, card-specific behavior, UI, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Usage Marking

- Added `FWBCardActivationUsageCommit`.
- `FWBCardActivationCommand` can now carry optional usage-mark metadata.
- Activation expansion populates usage commit metadata for once-per-turn source gates.
- Candidate generation passes source-gate context into expansion.
- Activation candidates and separate activation legal actions preserve usage commit metadata.
- `WBRules::CanApplyCardActivationCommand` validates usage commits but does not mutate usage.
- `WBEffectRunner::ApplyCardActivationCommand` marks usage only after successful effect request resolution.
- Failed activation does not mark usage.
- Invalid usage commit fails atomically without mutating original state.
- Already-used activation commands fail with `once_per_turn_already_used`.
- Usage resets through existing turn-start resource setup.
- `card_activation_usage_marked` trace is safe and does not serialize usage keys.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No Godot CardDB import, production card zones, real costs/RL/RR payment, activation `FWBAction`, response windows, negation, passives, wands, card-specific behavior, UI, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Cost Gates

- Added fixture-driven `FWBCardActivationCostGateDefinition`.
- Added fixture-driven `FWBCardActivationCostGateContext`.
- Source gates can now require externally supplied RR/RL affordability before activation candidates are generated.
- Candidate generation filters unaffordable activations through the existing source-gate path.
- Added validation for missing affordability, unaffordable results, RR requirement mismatches, negative cost data, and supplied available RL lower than supplied RR.
- Existing `bRequiresCostsSatisfiedExternally` remains in place; when both simple and detailed cost gates are present, both must pass.
- Candidate generation does not pay costs or mutate `RLUsed`; later fixture payment scaffolding lets explicit activation application pay only through command-owned payment metadata.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No Godot CardDB import, production card zones, real RR/RL payment, overflow, wand destruction, activation `FWBAction`, response windows, negation, passives, wands, card-specific behavior, UI, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Affordability Query

- Added `FWBCardActivationAffordabilityRequest`.
- Added `FWBCardActivationAffordabilityResult`.
- Added `IWBCardActivationAffordabilityProvider`.
- Added deterministic `FWBFixedCardActivationAffordabilityProvider` for fixture/test use.
- Added `WBCardActivationAffordability::QueryFromUnitRL`.
- Available RL is computed as `max(0, RLTotal - RLUsed)` while current Unreal state still uses `RLTotal` as current RL.
- Affordability queries are read-only and report `bOk = true` even when the activation is not affordable.
- Affordability results can be projected into source-gate cost contexts.
- Candidate sources can be copied and prepared with effect-specific source-gate contexts for effects with different RR costs.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No payment from the affordability query itself, overflow, wand destruction, CardDB import, production zones, activation `FWBAction`, response windows, UI, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Cost Payment Scaffolding

- Added `FWBCardActivationCostPaymentCommit` to activation commands.
- Added pure C++ `WBCardActivationCostPayment`.
- `CanPayCost` validates fixture-owned RR payment without mutation.
- `PayCost` mutates only source unit `RLUsed`.
- Activation expansion populates payment commits from source-gate cost metadata.
- Candidate generation and activation legal action generation preserve payment commits.
- `WBRules::CanApplyCardActivationCommand` re-checks payment affordability.
- `WBEffectRunner::ApplyCardActivationCommand` pays on a working copy and commits payment only after successful activation resolution.
- Failed payment, failed effect validation, and failed effect resolution do not mutate original `RLUsed`.
- Payment plus usage marking remains atomic: failed activation marks neither payment nor usage.
- Added `card_activation_cost_paid` trace fields for cost amount and RL before/after values.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No overflow, wand destruction, CardDB import, production zones, activation `FWBAction`, response windows, negation, passives, wands, card-specific behavior, UI, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Cost Payment Replay Verifier

- Added test-only `FWBCardActivationCostPaymentVerifier` helpers under `WandboundTests`.
- Added fixture utility support for optional `expected.rl_state`, `expected.cost_paid_trace`, and `expected.forbidden_trace_substrings`.
- Added fixture utility validation for activation legal action `expected.cost_payment_commit`.
- Added GodotCanon replay fixtures for cost trace parity, final RL parity, failure no-payment, effect-failure rollback, payment/usage trace order, hidden metadata exclusion, legal action payment commit preservation, and the `FWBAction`/`WBActionCodec` boundary.
- Confirmed legal activation generation preserves payment commits without paying costs or mutating `RLUsed`.
- Confirmed payment remains outside core `FWBAction` and `WBActionCodec`.
- No gameplay, runtime, UI, Blueprint, `.uasset`, or `.umap` work was added.

## Milestone - Activation Legal Action Replay Verifier

- Added test-only `FWBCardActivationLegalActionReplayVerifier` helpers under `WandboundTests`.
- Selected activation action ids can be resolved from externally generated activation legal action sets.
- Missing selected activation ids fail with `activation_action_id_not_found` before mutation.
- Duplicate selected activation ids fail with `activation_action_id_ambiguous` before mutation.
- Added fixture operation `apply_card_activation_legal_action_by_id`.
- Selected activation action ids now cover cost payment trace parity and final `RLUsed` / Available RL parity.
- Selected activation action ids cover payment plus usage trace order.
- Hidden metadata exclusion is covered through selected-id replay traces.
- Activation remains separate from `FWBAction`.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No gameplay, runtime, UI, Blueprint, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Source-Zone Ownership Gates

- Added fixture-only `FWBCardActivationFixtureZoneEntry` and `FWBCardActivationFixtureZoneContext`.
- Source gates can now require fixture source-zone ownership for Hand, Equipped, and Discard activation candidates.
- Equipped fixture ownership requires the card to be equipped to the selected source unit.
- Deck activation is explicitly unsupported in this scaffold.
- Candidate generation inherits `SourceCardId` from card definitions and inherits source-level fixture zone context into effect-specific source-gate contexts where missing.
- Existing Fixture-zone behavior remains backward-compatible.
- Candidate generation remains read-only and does not mutate production `Deck`, `Hand`, or `Discard` arrays.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No production zones, CardDB import, activation `FWBAction`, UI target selection, response windows, effect negation, passives, wands, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Card Activation Board Source Parity

- Added fixture-only Board-source activation parity.
- Board-source gates now require the source unit's visible `CardId` to match the supplied or inherited `SourceCardId`.
- `SourceCardId` can inherit from `FWBCardDefinition::CardId`.
- Effect-specific source-gate contexts inherit Board source card id, unit, player, source zone, and fixture context as needed.
- Board source parity uses the visible board unit as source of truth and does not require `FixtureZoneContext`.
- Hand, Equipped, Discard, Deck unsupported, and Fixture legacy source-zone behavior remain unchanged.
- Candidate generation filters explicit Board fixture-ownership sources through the source gate.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No production zones, CardDB import, activation `FWBAction`, UI target selection, response windows, effect negation, passives, wands, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Board-Source Activation Legal Action Coverage

- Added fixture-only Board-source activation legal action coverage.
- Board-source activation candidates convert through the existing separate activation legal action family.
- Cost payment commits are preserved from candidate generation into legal actions.
- Once-per-turn usage commits are preserved from candidate generation into legal actions.
- Selected activation action id replay is verified for Board-source legal actions.
- Card-id mismatch produces no selectable Board-source activation legal action.
- Activation remains separate from `FWBAction`.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- Fixture trace coverage confirms hidden fixture zone, usage, hand, deck, and discard metadata stay out of replay traces.
- No production zones, CardDB import, activation `FWBAction`, activation codec ids, UI target selection, response windows, effect negation, passives, wands, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Board-Source Activation Presentation Snapshot

- Added fixture-only Board-source activation legal action presentation snapshot coverage.
- Entries use activation action ids and clean public labels.
- Source and target public CardIds come from `FWBPublicBoardSummary` only.
- Missing public source/target units leave flags false and CardIds empty.
- Hidden fixture metadata, usage keys, debug activation ids, cost metadata, and fixture zone context are excluded from public presentation fields.
- Duplicate activation action ids make presentation lookup fail instead of selecting ambiguously.
- Activation remains separate from `FWBAction`.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No production zones, CardDB import, UI target selection, response windows, effect negation, passives, wands, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Activation Target Selection Presentation Snapshot

- Added fixture-only activation target-selection presentation scaffolding.
- `WBCardActivationTargetPresentation` builds read-only entries from externally supplied activation legal actions and `FWBPublicBoardSummary`.
- Target refs are classified as None, Unit, Tile, WallEdge, or Unknown.
- Public activation labels fall back to `Activate`.
- Public target labels use `Activate`, `Choose Unit`, `Choose Tile`, `Choose Wall`, or `Choose Target`.
- Source public CardIds and unit-target public CardIds come only from public board summary units.
- Missing public units leave CardIds empty and do not fail snapshot construction.
- Duplicate activation action ids make target-presentation lookup fail.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No real target picking, target legality generation, activation `FWBAction`, production zones, CardDB import, UI, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation Presentation Handoff

- Added `UWBRuntimeActivationPresentationModelComponent`.
- Runtime can now store externally supplied activation legal action sets.
- Runtime can build and store activation legal action presentation snapshots.
- Runtime can build and store activation target presentation snapshots for future UI.
- Added `WBRuntimeActivationPresentationRefreshAdapter`.
- `UWBRuntimeDecisionPointCoordinatorComponent` can optionally refresh activation presentation from external activation legal action sets and public summaries.
- `UWBRuntimeDecisionPointOwnerComponent` can delegate activation presentation refresh and explicit activation presentation clear.
- Activation presentation remains separate from normal `FWBAction` decision-point presentation.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production zones, CardDB import, activation `FWBAction`, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation Selection Resolver

- Added `WBRuntimeActivationSelectionResolver`.
- Selected activation action ids can resolve from stored runtime activation presentation state.
- Successful resolution returns the matching internal `FWBCardActivationLegalAction`.
- Successful resolution also returns public activation and target presentation entries when available.
- Missing ids fail with `activation_action_id_not_found`.
- Duplicate ids fail with `activation_action_id_ambiguous`.
- Model, coordinator, and owner convenience methods delegate to the resolver.
- The resolver does not execute activation commands.
- Activation remains separate from normal `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production zones, CardDB import, activation `FWBAction`, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation Execution Handoff Stub

- Added `WBRuntimeActivationExecutionHandoff`.
- Resolved activation selections can now produce a not-implemented execution handoff result.
- Successful handoffs preserve the internal `FWBCardActivationLegalAction` and any public activation/target presentation entries.
- Failed handoffs preserve the unresolved selection reason.
- Model, coordinator, and owner convenience methods can create handoff stubs from selected activation action ids.
- The stub does not execute activation commands.
- The stub does not mutate state.
- The stub does not inspect `FWBGameStateData`.
- Activation remains separate from normal `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production zones, CardDB import, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation Execution Integration

- Added `WBRuntimeActivationExecutionBridge`.
- Runtime can now execute a resolved activation handoff by delegating to `WBEffectRunner::ApplyCardActivationCommand`.
- Added `FWBRuntimeActivationExecutionResult` for bridge status, attempted execution state, selected activation id, copied handoff payload, and core activation command result.
- Added model, coordinator, and owner convenience methods for executing selected activation action ids from externally refreshed activation presentation state.
- The owner stores the latest activation execution result and clears it through the existing owner clear path.
- Cost payment, effect mutation, usage marking, trace emission, and rollback remain owned by `WandboundCore`.
- Activation execution does not regenerate legal actions, activation legal actions, public summaries, or visual presentation.
- Activation remains separate from normal `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production zones, CardDB import, activation codec ids, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Post-Activation Refresh Sequencing

- Added `WBRuntimePostActivationRefreshSequencer`.
- Activation execution can now be followed by explicit external-data refresh for normal decision-point presentation and activation presentation.
- Added `FWBRuntimePostActivationRefreshOptions`, `FWBRuntimePostActivationRefreshInput`, and `FWBRuntimePostActivationRefreshResult`.
- Added `UWBRuntimeDecisionPointOwnerComponent::ExecuteActivationActionIdAndRefreshFromExternalData`.
- Runtime does not generate legal actions.
- Runtime does not generate activation action sets.
- Runtime does not build public summaries from state.
- Runtime does not call `WBEffectRunner` directly from the sequencer.
- Activation remains separate from `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production zones, CardDB import, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation Decision-Session Facade

- Added `UWBRuntimeActivationDecisionSessionFacadeComponent`.
- The facade consumes external normal legal actions, activation legal actions, and public summaries.
- It sequences session refresh, activation selection resolution, handoff creation, activation execution, and post-action refresh.
- It does not generate legal actions.
- It does not generate activation action sets.
- It does not build public summaries from state.
- It does not call `WBEffectRunner` directly.
- Activation remains separate from `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production zones, CardDB import, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation Session Test Harness

- Added test-only `FWBActivationSessionTestHarness` under `WandboundTests`.
- The harness simulates an external rules owner by generating normal legal actions, activation candidates/actions, and public summaries only in automation tests.
- It validates external-data refresh, activation execution, post-action external refresh, and a second decision session.
- Coverage includes once-per-turn refresh, RR-cost affordability refresh, stale-state explicit refresh, failure-refresh policy, and hidden metadata exclusion.
- Production runtime still does not generate legal actions, activation candidates, activation action sets, or public summaries from state.
- Activation remains separate from `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production CardDB, production zones, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation Data Provider Interface

- Added `IWBRuntimeActivationDataProvider`.
- Provider results carry externally supplied normal legal actions, activation legal actions, and public board summaries.
- Added `WBRuntimeActivationDataProviderAdapter`.
- The adapter can refresh the activation decision-session facade from provider output.
- The adapter can execute a selected activation and refresh from provider-supplied post-activation output.
- Added fixed test-only provider coverage under `WandboundTests`.
- Runtime still does not generate legal actions, activation candidates, activation action sets, or public summaries from state.
- Runtime still does not own or cache rules state through the provider shell.
- Activation remains separate from `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production zones, CardDB import, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation Data Provider Contract

- Added test-only provider contract verifier coverage under `WandboundTests`.
- Future providers must return normal legal actions, activation legal actions, and public summaries for current-decision and post-activation requests.
- Deterministic ordering and hidden-information exclusion are covered.
- Provider failure policy is covered for refresh and execute-and-refresh adapter flows.
- Runtime still does not generate normal legal actions, activation candidates, activation action sets, or public summaries from state.
- Runtime still does not own or cache rules state through the provider shell.
- Activation remains separate from `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No production provider, CardDB import, production zones, UI target picking, response windows, UI widgets, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Runtime Activation CardDB/Zone Provider Planning

- Added planning-only provider documents.
- Defined future provider inputs and outputs.
- Defined zone ownership and hidden-information boundaries.
- Defined CardDB import boundaries.
- Defined fixture strategy and provider contract extension categories.
- Recommended a staged implementation sequence before production provider work.
- No production provider was added.
- No CardDB import was added.
- No production zones were added.
- No UI or response windows were added.
- No source code was changed.

## Milestone - Unreal CardDB Schema Documentation

- Added Unreal-owned CardDB schema planning docs.
- Documented the supported payload mapping for armor, status, damage, and heal effects.
- Documented source gate, cost gate, usage gate, target binding, public label, and hidden-information policy.
- Documented fail-closed diagnostics for unsupported card kinds, timings, target requirements, payload types, and payload operations.
- Activation remains separate from `FWBAction`.
- Existing `WBActionCodec` output remains unchanged.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- No importer was implemented.
- No production CardDB loader was added.
- No production zones were added.
- No UI, target picking, or response windows were added.
- No source code was changed.

## Milestone - Test-Only CardDB Schema Fixture Validation

- Added Unreal-owned CardDB schema fixtures under `Reference/GodotCanon/CardDBSchemaFixtures/`.
- Added a test-only schema fixture validator under `WandboundTests`.
- Supported payloads validate for damage, heal, status, and armor.
- Valid fixtures can map into `FWBCardDefinition` for validation only.
- Unsupported payloads fail closed.
- Unsupported operations, target requirements, timings, invalid RR costs, and invalid status ids fail closed.
- Bad player-facing labels and hidden-information policy violations fail closed.
- Source guards confirm the validator is not included by `WandboundCore` or `WandboundRuntime`.
- No production importer was implemented.
- No production CardDB loader was added.
- No production zones were added.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Extended Test-Only CardDB Schema Fixture Validation

- Added additional negative CardDB schema fixtures.
- Added unsupported card kind and unsupported source zone coverage.
- Added malformed JSON coverage.
- Added missing, malformed, and empty payload array coverage.
- Added malformed activated effects, source gate, and cost gate coverage.
- Added numeric validation coverage for payload amount and status duration fields.
- Existing valid fixtures still validate.
- Existing invalid fixtures still fail with stable diagnostics.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Strict Schema Validation

- Added explicit strict unknown-field validation options to the test-only CardDB schema fixture validator.
- Default validator behavior remains non-strict.
- Added unknown-field diagnostics for top-level/card, stats, effect, source gate, cost gate, payload, and metadata objects.
- Documented and tested the metadata allow-list.
- Added strict-valid, strict-invalid, and non-strict compatibility fixtures.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Bundle Schema Validation

- Added test-only bundle validation for a future production-shaped `cards[]` wrapper.
- Bundle validation supports `bundle_schema_version`, `carddb_version`, optional source/migration/metadata fields, and `cards`.
- Each `cards[]` entry validates through the existing root-card fixture validator.
- Duplicate card ids fail closed.
- Strict bundle unknown-field rejection was added for bundle top-level and metadata fields.
- Valid bundle cards map into `FWBCardDefinition` values for validation only.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Bundle Diagnostic Reporting

- Added stable bundle diagnostic context for card index, safe card id, effect id, and JSON path.
- Multi-card invalid bundle coverage was added.
- Duplicate card id diagnostics report the second duplicate index.
- Bundle-level diagnostics appear before card-level diagnostics when detected first.
- Hidden-token diagnostic safety is covered.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Bundle Cross-Reference Diagnostics

- Added test-only `references` shape validation for card, activated-effect, and payload fixture objects.
- Added bundle-level resolution for referenced card ids and effect ids.
- Added `missing_card_reference`, `missing_effect_reference`, `reference_malformed`, and `unknown_reference_field` diagnostics.
- Root-card validation checks reference shape only; bundle validation resolves references.
- Reference metadata remains authoring/test metadata and does not map into `FWBCardDefinition`.
- Duplicate card ids preserve duplicate diagnostics and skip reference resolution because the bundle index is ambiguous.
- Hidden-token diagnostic safety is covered for missing references.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Bundle Dependency Diagnostics

- Added test-only dependency order calculation from existing fixture `references` fields.
- Added `DependencyOrderCardIds` to the test-only bundle validation result.
- Added `dependency_self_reference` and `dependency_cycle_detected` diagnostics.
- Stable dependency ordering and original `cards[]` tie-breaks are covered by fixtures.
- Missing references and duplicate card ids skip dependency ordering and leave the order empty.
- Cycle diagnostics report deterministic authoring context without full cycle lists.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Bundle Dependency Export

- Added test-only JSON export for bundle validation results.
- Expected exports snapshot dependency order and diagnostic context.
- Export JSON includes only safe context fields and omits diagnostic messages and full source JSON.
- Hidden-token export safety is covered.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Bundle-Level Export Diagnostics

- Added expected export snapshots for malformed bundle-level diagnostics.
- Added export coverage for missing/unsupported bundle schema version, missing CardDB version, missing/malformed/empty `cards`, duplicate card ids, strict unknown fields, non-strict unknown-field compatibility, mixed bundle/card diagnostics, and hidden-token-safe missing references.
- Export snapshots remain test-only planning artifacts.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Dependency Ordering Edge Cases

- Added importer-ready dependency ordering edge-case fixtures.
- Covered duplicate references, mixed reference levels, repeated paths, disconnected components, independent card input order, and effect/payload references to the same dependency.
- Invalid bundles skip dependency order when validation diagnostics already exist before dependency ordering.
- Added expected export snapshots for the new edge cases.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Bundle Version Metadata Diagnostics

- Added test-only `carddb_version` and `source_version` policy diagnostics.
- Added `migration_notes` validation for type, max length, and hidden-token safety.
- Added bundle metadata object/value type validation and hidden-token safety.
- Added expected export snapshots for version and metadata diagnostics.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Source Version Compatibility

- Added an opt-in test-only source-version compatibility matrix.
- Added target source version validation for future importer planning.
- Added direct support and source-to-target transition support fixtures.
- Unsupported source versions fail closed with `source_version_unsupported`.
- Unsupported transitions fail closed with `source_version_transition_unsupported`.
- Missing required source versions fail closed with `source_version_missing`.
- Malformed compatibility matrix options fail closed with `source_version_compatibility_matrix_malformed`.
- No migration implementation was added.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Importer Readiness

- Added a validation-only importer-readiness helper under `WandboundTests`.
- Compatible bundles can produce ordered validated `FWBCardDefinition` values for test inspection.
- Unsupported source versions and unsupported transitions fail closed before readiness.
- Invalid bundles, missing references, dependency cycles, duplicate card ids, and invalid payloads are not ready.
- Added expected readiness export snapshots.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No effects are executed.
- No activation candidates/actions are generated by production code.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Importer Diagnostic Summary

- Added a test-only importer diagnostic summary helper under `WandboundTests`.
- Readiness failures can be grouped by stable reason.
- Schema diagnostics can be grouped by stable diagnostic code.
- Affected bundle and affected card-context counts are covered.
- Added expected summary export snapshots.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No effects are executed.
- No activation candidates/actions are generated by production code.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

## Milestone - Test-Only CardDB Importer Batch Readiness

- Added a test-only importer batch-readiness helper under `WandboundTests`.
- Named bundle sets can be evaluated with per-entry validation options.
- Batch output preserves per-bundle readiness entries in input order.
- Batch output includes grouped diagnostic summary arrays from the existing summary helper.
- Added expected batch readiness export snapshots for mixed, all-ready, all-not-ready, source-version mix, hidden-token-safe, and empty batches.
- No production importer was implemented.
- No production loader was added.
- No production zones were added.
- No Core or Runtime source files were changed.
- No runtime activation behavior was changed.
- No effects are executed.
- No activation candidates/actions are generated by production code.
- No `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` behavior was changed.
- No UI, target picking, response windows, Blueprints, `.uasset`, or `.umap` work was added.

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
