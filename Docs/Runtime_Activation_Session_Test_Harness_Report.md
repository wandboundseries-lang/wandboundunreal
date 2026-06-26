# Runtime Activation Session Test Harness Report

## Purpose

This pass adds a C++ automation-only activation session harness under `Source/WandboundTests/Private`. It proves the non-UI runtime activation session loop can consume external normal legal actions, activation legal action sets, and public board summaries across activation execution and later refreshes.

## Test-Only Location

`FWBActivationSessionTestHarness` lives under `WandboundTests` only. It must not be included from `Source/WandboundRuntime` or production code.

The helper simulates an external rules owner by calling:

- `WBRules::GenerateLegalActionsForPlayer`
- `WBCardActivationCandidateGenerator::GenerateCandidates`
- `WBCardActivationLegalActionGenerator::GenerateFromCandidates`
- `WBPublicBoardSummary::Build`

These calls remain test-only. Production runtime still receives precomputed data from an external owner.

## External Data Build Behavior

`BuildExternalDataForTest` returns:

- normal legal `FWBAction` data
- activation legal action sets
- public board summary data

It does not modify the state.

## One-Decision Flow

The harness refreshes the runtime activation decision-session facade from external data, selects the first activation action matching a public label, previews post-action data on a cloned state for test-only sequencing, executes the real activation through the facade/owner/sequencer path, and verifies the real state mutation plus post-refresh status.

## Two-Decision Flow

Automation coverage proves two consecutive activation decision sessions can run against the same facade:

1. refresh external data
2. execute activation
3. refresh from externally generated post-action data
4. repeat for a second activation selection

## Once-Per-Turn Refresh

The once-per-turn test uses source-gate usage marking. Before execution the activation is available. After execution and external refresh, the action disappears. After turn-start resource setup clears the per-turn usage key, externally generated activation data contains the action again.

## Cost Refresh

The RR-cost test uses source-gate affordability and cost-payment commits. Before execution the activation is affordable. Execution increases `RLUsed` through the core effect path. Rebuilt post-action external data marks the action unaffordable, and the refreshed activation presentation has no activation actions.

## Stale-State Policy

Tests prove presentation stays stale when success refresh is disabled. The facade only updates after an explicit external refresh with caller-supplied data.

Failure refresh remains opt-in. Failed activation selection does not refresh by default; with failure-refresh options enabled, supplied recovery data is reflected even though the execution result stays failed.

## Hidden Metadata

Public activation presentation, serialized trace events, and reason strings exclude internal source effect ids, usage keys, debug activation ids, and cost metadata. Internal commands may retain those fields for execution.

## Production Runtime Source Guard

The new source guard verifies `Source/WandboundRuntime` does not include the test harness and does not call:

- `WBRules::GenerateLegalActions`
- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`

Existing runtime activation execution still delegates mutation through the established bridge/effect-runner boundary.

## Action Family Boundary

Activation legal actions remain separate from `FWBAction`.

`WBRules::GenerateLegalActions` is unchanged for activation generation, and `WBActionCodec` remains unchanged.

## Fixtures

No GodotCanon JSON fixtures were added. This pass verifies transient runtime component sequencing and test harness behavior rather than canonical Godot rules behavior. The useful invariant is C++ component wiring plus source-guard coverage, so C++ automation is the lower-risk format.

## Out Of Scope

- production CardDB
- production zones
- UI widgets
- target picking
- response windows
- negation
- passives and wands
- visual animation and VFX
- activation `FWBAction` integration
- `WBActionCodec` activation ids
