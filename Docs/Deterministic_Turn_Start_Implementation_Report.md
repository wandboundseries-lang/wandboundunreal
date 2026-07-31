# Wandbound Deterministic Turn-Start Implementation Report

Date: 2026-07-30

## Result

The authoritative Core match coordinator now completes turn start in this
order:

1. Draw
2. Roll MP
3. Reset attacks and wall resources
4. Resolve start-of-turn statuses and deaths
5. Collect and resolve post-status start-of-turn effects

The sequence pauses inside Core when a trigger order or target choice is
required. Normal match actions are not exposed until turn start completes.

- Editor build: succeeded in 12.79 seconds on the final build.
- Game build: succeeded in 211.20 seconds.
- Focused new turn-start tests: 54 succeeded, 0 failed.
- Migrated affected groups: 599 succeeded, 0 failed.
- Initial Wandbound count: 1,724 succeeded.
- Final Wandbound count: 1,778 succeeded, 0 failed, 0 not run.
- Fresh BuildCookRun: succeeded in 194.04 seconds.
- Packaged development smoke: passed, exit 0.
- Packaged production turn start: passed, exit 0.
- Repeated startup comparison: byte-identical.
- Replay verification: 2 focused replay tests succeeded, 0 failed.
- `git diff --check`: passed; line-ending notices only.
- Exact final errors: none.

Interim validation issues were resolved:

- The first full run found 12 legacy tests and fixtures that still expected
  status processing before resource reset. Their expectations were migrated to
  the July 30 canonical order.
- The first direct packaging invocation was blocked by the local PowerShell
  execution policy. The same repository script succeeded with a process-local
  execution-policy bypass.
- The first packaged production probe omitted `-WandboundProductionData` and
  its bundle/match-spec paths, so it remained in development mode and hit the
  180-second guard. That process was stopped. The corrected documented
  production invocation passed twice.

## Baseline

- Baseline commit: `b3536b0 Add Active Format v1 and deterministic game start`.
- Branch: `main`.
- `HEAD` and `origin/main`: both
  `b3536b0ec86e43bc7e98e4f765aca7f698591b29`.
- Staged files: none.
- LFS-staged files: none.
- Initial Wandbound automation: 1,724 succeeded.
- Production bundle digest:
  `87d2644aeb479e84a3e96967fd57901ac52aa7e283fd5cfee142d35e8659f00c`.
- Match-spec file SHA-256:
  `5fd3ba8d78af9681e3acdc4c4b58e7c2934aacbcfed83cd3504a8a37e23b03da`.

The committed production startup reached `production_started`, selected Player
1 first, completed setup, and exposed a playable decision. It did not expose
the new turn-start milestone fields.

Pre-existing tracked changes were preserved in:

- `Config/DefaultEditor.ini`
- `Docs/CardDB_Unreal_Bundle_Schema_Validation_Report.md`
- `Source/WandboundCore/Private/WBCardActivationAffordability.cpp`
- `Source/WandboundCore/Private/WBCardActivationCostPayment.cpp`
- `Source/WandboundCore/Private/WBGameStateData.cpp`
- `Source/WandboundCore/Private/WBPublicBoardSummary.cpp`
- `Source/WandboundCore/Private/WBResonanceLoad.cpp`
- `Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp`
- `Source/WandboundCore/Public/WBEquipExecution.h`
- `Source/WandboundCore/Public/WBPublicBoardSummary.h`
- the nine pre-existing dirty equip, resonance, and runtime test files shown by
  final `git status`

Pre-existing untracked work was preserved under `Content/Maps/`,
`Content/MeshyImports/`, `Plugins/meshy/Content/`, two audit documents,
`MaxHP`, `RLTotal`, and the existing stray terminal-named files.

## Previous Turn-Start Behavior

Production match initialization previously:

1. resolved start-turn statuses;
2. emitted a synthetic first-turn draw-skip trace without calling the normal
   turn-start draw operation;
3. rolled MP;
4. applied combined MP/resource setup;
5. exposed normal actions.

Later coordinator transitions previously:

1. resolved start-turn statuses;
2. drew;
3. rolled MP and reset resources together;
4. exposed normal actions.

The lower-level deterministic transition helper also processed statuses before
resource setup and does not own draw or card-trigger orchestration. Runtime and
presentation did not directly perform these mutations, but the Core logic was
duplicated and ordered inconsistently.

## Authoritative Sequence

`WBTurnStartSequence` is the single full turn-start state machine used by
`WBMatchCoordinator` for initial startup and later match turns. Its phases are:

```text
Draw
MPRoll
ResourceReset
StatusResolution
EffectCollection
EffectResolution
Complete
```

`WBMatchCoordinator` owns phase progression, the deterministic RNG draw, pause
state, action validation, replay trace accumulation, and the transition to
normal actions. Runtime only reads the coordinator and serializes public-safe
milestones.

Sequence execution uses working state and sequence copies. A failed phase or
invalid submitted choice does not partially commit state.

## Draw

- `WBCardLifecycle::ApplyTurnStartDraw` remains the authoritative operation.
- The selected first player skips only Turn 1.
- The second player's first turn and the first player's Turn 2 draw normally.
- A skipped draw does not consume the deck and emits
  `turn_start_draw_skipped`, not a false card-drawn trace.
- Normal draw traces use `normal_turn_start_draw`; the skip uses
  `first_player_turn_one_draw_skipped`.
- Opening-hand reasons remain distinct.
- Empty-deck behavior is unchanged.
- Public turn-start traces contain count and reason only. They do not contain
  Card ID, card-instance ID, deck order, or hand contents.

## MP Generation

The coordinator calls its existing seeded `RollD6` stream exactly once before
starting the MP phase. `ApplyTurnStartMPRollForPlayer` preserves the existing
1-6 validation and existing replacement semantics for `LastMPRoll` and
`RemainingMP`.

The rolled value is stored in `FWBTurnStartSequenceState` and emitted as
`turn_start_mp_rolled`. Pausing for a trigger choice stores the already
completed sequence state, so refresh, observation, and resume cannot reroll or
reapply MP. Identical seeds reproduce identical results.

## Resource Reset

MP application and resource reset are now separate operations so their ordering
is explicit.

- Active, on-board units reset through the existing
  `ResetActionResourcesForPlayer` path.
- Existing modified attack limits remain authoritative.
- Defeated, removed, inactive, and opponent units are not reset.
- The active player's existing wall-placement allowance is restored.
- The opponent's wall allowance is unchanged.
- Existing per-turn activation usage keys are cleared in this phase.
- No unrelated once-per-match or setup-only flags were added.
- Resume after a Phase 5 decision does not repeat the reset.

Typed traces are `turn_start_attacks_reset` and
`turn_start_wall_restored`.

## Status Resolution

Status processing starts only after resource reset. Existing Poison behavior,
duration changes, expiration, death resolution, equipment/RL cleanup, board
removal, and win checks remain owned by the established effect/death paths.

On-board units are now visited in ascending Unit ID order, avoiding container
order as an authority source. The sequence records phase start/completion and
safe defeated-unit IDs. If status processing ends the match, the sequence enters
`Terminal`, does not collect triggers, and does not expose normal actions.

## Start-of-Turn Effects

Definitions may declare typed `AtStartOfYourTurn` or `AtStartOfEachTurn`
triggers. Collection occurs from the post-status authoritative state.

Sources must still be on board, undefeated, unchanged in identity, and not
`Negated`. A source removed by status/death cleanup is never queued. Repository
validation rejects missing/duplicate trigger IDs, missing effects, unsupported
target requirements, and malformed target/effect combinations.

Pending triggers are ordered by active-player control, controller, Unit ID, and
stable trigger ID. One legal choice resolves automatically. Multiple legal
choices pause the sequence and expose stable IDs:

```text
turn_start_trigger:p{controller}:u{source}:{trigger}
turn_start_trigger:p{controller}:u{source}:{trigger}:t{target}
```

Submitted choices are checked against the current queue. Stale, duplicate, and
unknown IDs fail closed. Each selection and resolution is recorded. Existing
generic payload execution remains in `WBEffectRunner`; trigger order is not a
React window.

## Production Integration

The Active Format production startup now completes:

```text
setup
turn_start_draw_skipped
turn_start_mp_rolled
turn_start_attacks_reset
turn_start_wall_restored
turn_start_status_phase_completed
turn_start_effects_resolved
turn_start_completed
first normal legal decision
```

The startup result does not return `production_started` until all turn-start
milestones and a playable decision are present.

Fresh package:

```text
Saved/PackagedBuilds/DeterministicTurnStart_20260730_221320
```

The packaged development smoke passed with generation 1 and revision 4. The
production probe passed with Player 1 first and active, Turn 1, completed draw
skip, MP, reset, status, effect, and playable-decision milestones.

## Startup Result

New public-safe fields are:

```text
turn_start_completed
turn_start_draw_skipped
turn_start_mp_generated
turn_start_resources_reset
turn_start_statuses_resolved
turn_start_effects_resolved
active_player
turn_number
```

No RNG state, drawn-card identity, hand contents, deck order, concealed marker
identity, or hidden effect payload is serialized.

Two corrected packaged production runs produced byte-identical JSON:

```text
SHA-256 cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004
```

## Tests

### Ordering

- `Wandbound.TurnStart.Order.DrawBeforeMP`: proves trace phase order.
- `Wandbound.TurnStart.Order.MPBeforeResourceReset`: proves MP precedes reset.
- `Wandbound.TurnStart.Order.ResourceResetBeforeStatuses`: proves reset precedes status.
- `Wandbound.TurnStart.Order.StatusesBeforeEffects`: proves post-status collection.
- `Wandbound.TurnStart.Order.CompleteSequenceDeterministic`: compares repeated runs.

### Draw

- `Wandbound.TurnStart.Draw.FirstPlayerTurnOneSkipped`: proves the sole normal skip.
- `Wandbound.TurnStart.Draw.SecondPlayerFirstTurnDraws`: proves Player 2 draws.
- `Wandbound.TurnStart.Draw.FirstPlayerTurnTwoDraws`: proves later Player 1 draw.
- `Wandbound.TurnStart.Draw.SkipDoesNotConsumeDeck`: protects deck count.
- `Wandbound.TurnStart.Draw.NoFalseDrawTrace`: excludes a false draw event.
- `Wandbound.TurnStart.Draw.PrivateIdentityProtected`: excludes identity from public trace.
- `Wandbound.TurnStart.Draw.DistinctFromOpeningHandDraw`: protects reason separation.
- `Wandbound.TurnStart.Draw.EmptyDeckBehaviorPreserved`: preserves empty-deck behavior.

### MP

- `Wandbound.TurnStart.MP.ExactlyOneRoll`: proves one application.
- `Wandbound.TurnStart.MP.AuthoritativeRNG`: proves coordinator RNG ownership.
- `Wandbound.TurnStart.MP.ReplayStable`: proves seeded replay stability.
- `Wandbound.TurnStart.MP.NoRerollAfterPausedDecision`: proves resume idempotence.
- `Wandbound.TurnStart.MP.ExistingAccumulationPreserved`: preserves existing MP semantics.

### Reset

- `Wandbound.TurnStart.Reset.AttacksResetAfterMP`: proves ordering and reset.
- `Wandbound.TurnStart.Reset.ModifiedAttackLimitPreserved`: protects modified limits.
- `Wandbound.TurnStart.Reset.InactiveUnitsIgnored`: excludes inactive units.
- `Wandbound.TurnStart.Reset.WallPlacementRestored`: restores active wall allowance.
- `Wandbound.TurnStart.Reset.OpponentWallNotRestored`: protects opponent resources.
- `Wandbound.TurnStart.Reset.NoDuplicateResetAfterResume`: proves pause idempotence.

### Status

- `Wandbound.TurnStart.Status.TickAfterResourceReset`: proves canonical order.
- `Wandbound.TurnStart.Status.DurationDecrements`: proves duration mutation.
- `Wandbound.TurnStart.Status.ExpirationCompletesBeforeEffects`: proves full expiry.
- `Wandbound.TurnStart.Status.LethalDamageRemovesUnit`: proves lethal removal.
- `Wandbound.TurnStart.Status.DeathCleanupCompletes`: proves cleanup completion.
- `Wandbound.TurnStart.Status.TerminalMatchStopsSequence`: blocks Phase 5 on game over.

### Effects

- `Wandbound.TurnStart.Effects.CollectedFromPostStatusState`: proves late collection.
- `Wandbound.TurnStart.Effects.DeadSourceDoesNotTrigger`: excludes defeated sources.
- `Wandbound.TurnStart.Effects.SurvivingSourceTriggers`: resolves valid sources.
- `Wandbound.TurnStart.Effects.NegatedSourceDoesNotTrigger`: excludes negated sources.
- `Wandbound.TurnStart.Effects.MultipleTriggersRequireOrdering`: proves pause behavior.
- `Wandbound.TurnStart.Effects.ControllerChoosesOrder`: proves authoritative submission.
- `Wandbound.TurnStart.Effects.StableActionIds`: proves deterministic IDs.
- `Wandbound.TurnStart.Effects.ReplayPreservesOrder`: proves replayed choice order.
- `Wandbound.TurnStart.Effects.RequiredTargetChoiceAllowed`: proves target decisions.

### Production

- `Wandbound.Production.TurnStart.FirstPlayerTurnOneCompletes`: proves startup completion.
- `Wandbound.Production.TurnStart.DrawSkipRecorded`: proves safe skip milestone.
- `Wandbound.Production.TurnStart.MPRecorded`: proves safe MP milestone.
- `Wandbound.Production.TurnStart.ResourcesReset`: proves reset milestone.
- `Wandbound.Production.TurnStart.StatusesResolved`: proves status milestone.
- `Wandbound.Production.TurnStart.EffectsResolved`: proves effect milestone.
- `Wandbound.Production.TurnStart.FirstDecisionAfterCompletion`: gates readiness.
- `Wandbound.Production.TurnStart.StartupResultPublicSafe`: checks hidden tokens.
- `Wandbound.Production.TurnStart.RepeatedResultByteIdentical`: checks serialization.

### Authority

- `Wandbound.Authority.TurnStart.CoreOwnsSequence`: guards Core ownership.
- `Wandbound.Authority.TurnStart.RuntimeCannotReroll`: guards runtime authority.
- `Wandbound.Authority.TurnStart.PresentationCannotMutate`: guards presentation.
- `Wandbound.Authority.TurnStart.NoGodotChanges`: guards reference source.
- `Wandbound.Authority.TurnStart.NoMeshyChanges`: guards Meshy content.
- `Wandbound.Authority.TurnStart.NoModelImports`: guards model assets.

The following existing tests were updated only to expect resources before
start-status processing:

- `Wandbound.Core.RuntimeResultSerialization.FixtureScenarios`
- `Wandbound.Core.RuntimeTurnResolution.FixtureScenarios`
- `Wandbound.Core.RuntimeTurnResultEnvelope.FixtureScenarios`
- `Wandbound.Core.SelectedAction.EndTurnFullTransition`
- `Wandbound.Core.SelectedAction.FixtureScenarios`
- `Wandbound.Core.TurnCommandReplay.FullTransitionFixture`
- `Wandbound.Core.TurnController.FixtureScenarios`
- `Wandbound.Core.TurnController.FullTransition`
- `Wandbound.Core.TurnTransition.BurnThenPoisonOrder`
- `Wandbound.Core.TurnTransition.ExpirationOrder`
- `Wandbound.Core.TurnTransition.FixtureScenarios`
- `Wandbound.Core.TurnTransition.ValidSequence`

## Changed Files

| Path | Baseline / overlap | Purpose and impact |
| --- | --- | --- |
| `Source/WandboundCore/Public/WBTurnStartSequence.h` | New | Sequence phases, durable pause state, trigger instances, and result contract. |
| `Source/WandboundCore/Private/WBTurnStartSequence.cpp` | New | Transactional five-phase execution, choices, safe traces, and completion gating. |
| `Source/WandboundCore/Public/WBCardDefinition.h` | Clean / none | Typed start-turn trigger definitions. |
| `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp` | Clean / none | Fail-closed trigger validation. |
| `Source/WandboundCore/Public/WBGameStateData.h` | Clean / none | Split MP and reset state operations. |
| `Source/WandboundCore/Private/WBGameStateData.cpp` | Dirty / yes | Added only split turn-start methods; pre-existing RL hunks were preserved. |
| `Source/WandboundCore/Public/WBEffectRunner.h` | Clean / none | Split effect-runner entry points. |
| `Source/WandboundCore/Private/WBEffectRunner.cpp` | Clean / none | MP/reset traces, deterministic status ordering, corrected compatibility order. |
| `Source/WandboundCore/Public/WBMatchCoordinator.h` | Clean / none | Sequence ownership, query access, and trigger action family. |
| `Source/WandboundCore/Private/WBMatchCoordinator.cpp` | Clean / none | Startup/later-turn integration, RNG ownership, pause/resume, action gating. |
| `Source/WandboundRuntime/Public/WBProductionStartupResult.h` | Clean / none | Public-safe milestone fields. |
| `Source/WandboundRuntime/Private/WBProductionStartupResult.cpp` | Clean / none | Completion gating and deterministic JSON serialization. |
| `Source/WandboundTests/Private/WBTurnStartSequenceTests.cpp` | New | All 54 requested focused tests. |
| `Source/WandboundTests/Private/WBSelectedActionExecutorTests.cpp` | Clean / none | New canonical legacy trace expectation. |
| `Source/WandboundTests/Private/WBTurnControllerCommandTests.cpp` | Clean / none | New canonical legacy trace expectation. |
| `Source/WandboundTests/Private/WBTurnTransitionOrchestrationTests.cpp` | Clean / none | New ordering and expiration assertions. |
| `Reference/GodotCanon/GoldenScenarios/replay_turn_command_full_transition_burn_poison_setup.json` | Clean / none | Canonical resource-before-status trace order. |
| `Reference/GodotCanon/GoldenScenarios/runtime_result_end_turn_full_transition_roll_4.json` | Clean / none | Canonical resource-before-status trace order. |
| `Reference/GodotCanon/GoldenScenarios/runtime_result_serialization_full_transition.json` | Clean / none | Canonical resource-before-status trace order. |
| `Reference/GodotCanon/GoldenScenarios/runtime_selected_end_turn_full_transition_roll_4.json` | Clean / none | Canonical resource-before-status trace order. |
| `Reference/GodotCanon/GoldenScenarios/selected_action_end_turn_full_transition.json` | Clean / none | Canonical resource-before-status trace order. |
| `Reference/GodotCanon/GoldenScenarios/turn_command_full_transition.json` | Clean / none | Full canonical trace event reorder. |
| `Reference/GodotCanon/GoldenScenarios/turn_transition_burn_then_poison_then_setup.json` | Clean / none | Full canonical trace and intent reorder. |
| `Reference/GodotCanon/GoldenScenarios/turn_transition_status_expiration_order.json` | Clean / none | Resource reset now precedes Poison/expiration. |
| `Docs/Deterministic_Turn_Start_Implementation_Report.md` | New | Durable audit and validation evidence. |
| `Docs/Build_Test_Report.md` | Clean / none | Appended concise final validation. |

No `Reference/GodotProject`, Meshy, model, Blueprint, map, `.uasset`, or
`.umap` file was changed by this task.

## Git Status

- Task files: the 26 paths listed above.
- Pre-existing tracked changes: preserved and unstaged.
- Pre-existing untracked files: preserved.
- Staged files: none.
- LFS-staged files: none.
- Package output: ignored under
  `Saved/PackagedBuilds/DeterministicTurnStart_20260730_221320` and
  `Saved/StagedBuilds/DeterministicTurnStart_20260730_221320`.
- No stage, commit, push, clean, reset, restore, checkout, or delete operation
  was performed.

## Remaining Risks

- `WBEffectRunner::ApplyDeterministicTurnTransition` and its
  `WBTurnController` caller remain compatibility APIs. They now preserve the
  canonical resource-before-status order, but they do not perform draw or
  start-turn trigger orchestration. Production match flow uses
  `WBMatchCoordinator` and does not rely on this narrower helper.
- Start-turn generic payloads currently support the existing Unit target
  requirement. Tile, wall, and other future target types fail closed.
- The current trigger model treats declared start-turn triggers as mandatory;
  optional "may" semantics are not modeled.
- No existing rule establishes a manual React window during start-turn effect
  resolution. This pass preserves the narrow existing behavior and does not
  create or suppress a new React timing rule.
- Production CardDB definitions currently contain no start-turn trigger data;
  the production first turn validates the empty-trigger path. Trigger
  resolution is covered with Core definition fixtures.

## Recommended Next Task

Migrate the remaining production-facing `WBTurnController` full-transition
callers onto `WBMatchCoordinator` turn transitions, then deprecate the partial
compatibility helper without changing turn-start behavior.
