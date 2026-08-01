# Turn Transition Authority Migration Audit

## Scope

This audit covers tracked C++ production, replay, compatibility, and test-fixture entry points that can submit EndTurn, execute a complete turn transition, or perform one of its focused mutations. Production reachability was traced from player input and packaged smoke entry points rather than inferred from test use.

## Authority Boundary

`WBMatchCoordinator::SubmitActionId` is the sole production authority for the complete transition:

1. validate the submitted EndTurn action;
2. resolve end-turn statuses and cleanup;
3. advance the turn through the focused EndTurn effect;
4. resolve the NPC phase and terminal state;
5. consume coordinator-owned deterministic RNG;
6. begin the durable `WBTurnStartSequence`;
7. pause and resume through coordinator action submission;
8. expose normal legal actions only after completion.

`WBTurnController`, `WBSelectedActionExecutor` full-transition mode, and the raw-state branch of `WBRuntimeTurnResolutionAdapter` remain compatibility-only. Their legacy deterministic transition delegates to `WBMatchCoordinator::ApplyLegacyCompatibilityTurnTransition`; this preserves old fixture behavior but does not implement the production NPC, draw, trigger-decision, or coordinator phase flow.

## Caller Inventory

| Caller | Category | Authority | Production | Replay | Tests | Migration |
| --- | --- | --- | --- | --- | --- | --- |
| `AWBRuntimePlayerController::ForwardEndTurn` | ProductionRuntime | Runtime host command | Yes | No | Yes | AlreadyCoordinatorOwned |
| `UWBRuntimeMatchHUDWidget::RequestEndTurn` | ProductionRuntime | Runtime host command | Yes | No | Yes | AlreadyCoordinatorOwned |
| `UWBRuntimeMatchHostComponent::EndTurn` | ProductionRuntime | Coordinator legal action | Yes | Yes | Yes | AlreadyCoordinatorOwned |
| `UWBRuntimeMatchHostComponent::SubmitLegalActionAtRevision` | ProductionRuntime | `WBMatchCoordinator::SubmitActionId` | Yes | Yes | Yes | AlreadyCoordinatorOwned |
| `WBRuntimeLocalPlaySmoke::Run` | ProductionRuntime | Runtime host EndTurn | Yes | No | Yes | AlreadyCoordinatorOwned |
| `WBProductionRuntimeBootstrap::Build` | ProductionBootstrap | Initialization request only | Yes | Yes | Yes | AlreadyCoordinatorOwned |
| `WBMatchCoordinator::SubmitActionId` | AuthoritativeCore | Coordinator | Yes | Yes | Yes | AlreadyCoordinatorOwned |
| `WBMatchCoordinator::ApplyTurnTransition` | AuthoritativeCore | Coordinator private orchestration | Yes | Yes | Yes | AlreadyCoordinatorOwned |
| `WBTurnStartSequence::Begin/SubmitChoice` | AuthoritativeCore | Durable focused sequence | Yes | Yes | Yes | LowLevelPrimitiveOnly |
| `WBNPCPhaseResolution::ResolvePhase` | AuthoritativeCore | Focused NPC phase | Yes | Yes | Yes | LowLevelPrimitiveOnly |
| `WBEffectRunner::ApplyEndTurn` | AuthoritativeCore | Focused player advance mutation | Yes | Yes | Yes | LowLevelPrimitiveOnly |
| turn-start `WBEffectRunner` methods | AuthoritativeCore | Focused resource/status mutations | Yes | Yes | Yes | LowLevelPrimitiveOnly |
| `FWBGameStateData` turn methods | AuthoritativeCore | Focused state primitives | Yes | Yes | Yes | LowLevelPrimitiveOnly |
| `WBRules::CanApplyDeterministicTurnTransition` | LegacyCompatibility | Compatibility validation | No | No | Yes | CompatibilityOnly |
| `WBRuntimeTurnResolutionAdapter` coordinator branch | LegacyCompatibility | Injected coordinator | No active production owner | Yes | Yes | AlreadyCoordinatorOwned |
| `WBRuntimeTurnResolutionAdapter` raw-state branch | LegacyCompatibility | Compatibility helper | No | Legacy fixtures | Yes | CompatibilityOnly |
| `WBSelectedActionExecutor` full-transition mode | LegacyCompatibility | `WBTurnController` adapter | No | Legacy fixtures | Yes | CompatibilityOnly |
| `WBTurnController::ApplyTurnCommand` full mode | LegacyCompatibility | Coordinator compatibility helper | No | Legacy fixtures | Yes | SafeToDeprecate |
| `WBEffectRunner::ApplyDeterministicTurnTransition` | LegacyCompatibility | Coordinator compatibility helper | No | Legacy fixtures | Yes | SafeToDeprecate |
| `WBReplayVerifier::Verify` | Replay | Low-level action replay | No | Yes | Yes | UnsafeToRemove |
| `WBReplayFixtureTestUtils` turn helpers | GoldenFixture | Legacy fixture compatibility | No | Golden fixtures | Yes | TestOnly |
| runtime decision-point/controller facade shells | Unused | External-state shell chain | No active production owner | No | Yes | UnusedConfirmed |

The machine-readable companion contains the required field set and is the source used by authority audit automation.

## Previous Bypass

The raw-state runtime adapter previously selected an MP roll, configured `WBSelectedActionExecutor`, invoked `WBTurnController`, and reached an independent full-transition algorithm in `WBEffectRunner`. That chain could advance the player and run a partial resource/status sequence, but could not represent NPC resolution, turn-start draw, trigger pause/resume, or coordinator terminal phases.

The active runtime host and production bootstrap were already coordinator-owned at baseline. They required no behavioral rewrite.

## Final Classification

- Production full-transition callers: coordinator-owned.
- Production focused mutations: private coordinator composition only.
- Replay of complete match decisions: coordinator action submission.
- Generic low-level action replay: retained and explicitly not a complete match-transition path.
- Legacy raw-state full transition: compatibility-only, source-deprecated, and delegated.
- Unknown full-transition callers: none.

