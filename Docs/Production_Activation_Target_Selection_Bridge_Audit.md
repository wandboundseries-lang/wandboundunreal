# Production Activation Target Selection Bridge Audit

> **Historical / Superseded**
>
> This audit records the pre-implementation state and is retained for engineering history. It is not current implementation authority. See `Docs/Production_Activation_Target_Selection_Bridge_Report.md` and commit `51b04876120a102dc7028c6390d003c3a1e46cae`.

Date: 2026-06-28

## Existing Activation Command Shape

`FWBCardActivationCommand` contains:

- `FWBCardActivationSource Source`
- `FWBEffectRequest EffectRequest`
- cost payment commit data
- usage commit data
- internal `DebugActivationId`

`FWBEffectRequest` already contains `FWBEffectTargetRef Target`, including `TargetUnitId`, `TargetTile`, and `TargetWallEdge`.

No new command target-binding field is required for unit targets. A selected unit target can be bound by copying the provider-supplied command and setting `Command.EffectRequest.Target.TargetUnitId`.

## Existing Runtime Selection Resolver Behavior

`WBRuntimeActivationSelectionResolver` resolves a selected activation action id from `UWBRuntimeActivationPresentationModelComponent`.

It returns `FWBRuntimeActivationSelectionResolution`, including:

- the selected activation action id
- the matching internal `FWBCardActivationLegalAction`
- safe activation and target presentation entries when available

The resolver is read-only. It does not inspect game state, execute effects, generate actions, or compute target options.

## Existing Execution Handoff Behavior

`WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff` converts a resolved activation selection into `FWBRuntimeActivationExecutionHandoffResult`.

The handoff stores the activation action and presentation entries. It does not execute by itself.

`WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff` is the later mutating step. It validates the handoff and delegates to `WBEffectRunner::ApplyCardActivationCommand`.

## Provider Target-Option Data Shape

`FWBCardActivationLegalAction` now carries:

- `TargetRequirement`
- `TargetOptions`

`FWBCardActivationTargetOption` currently supports unit targets and includes visible board unit id, owner id, tile, public board card id, and a safe public label.

Tile and wall target options remain deferred.

## Chosen Bridge Location And API

The bridge should live in `WandboundRuntime` as a production C++ helper:

- `FWBProductionActivationTargetSelectionBridge`
- configured from externally supplied provider activation entries
- builds a bound `FWBCardActivationCommand` for one selected provider target option

This keeps runtime session components as consumers of external data. The bridge does not compute target options, execute commands, or mutate rules state.

## Unit Target Binding

For a `Unit` target requirement:

- a selected target option is required
- the selected option must be type `Unit`
- it must match an option already present on the provider activation entry
- matching checks unit id, owner id, tile, and visible board card id
- on success, the bridge copies the action command and sets `EffectRequest.Target.TargetUnitId`
- the result also reports safe source card id, effect id, and target unit id for tests

## No-Target Behavior

For a `None` target requirement:

- no selected target is required
- providing a target fails with `target_provided_for_no_target_effect`
- the copied command keeps an empty target binding

## Rejection Behavior

Stable failures:

- `bridge_not_configured`
- `activation_entry_missing`
- `target_required_but_missing`
- `target_not_allowed`
- `target_type_mismatch`
- `target_stale_or_missing`
- `target_provided_for_no_target_effect`
- `unsupported_target_requirement`
- `hidden_target_rejected`
- `command_build_failed`

Tile, wall, and unknown target requirements fail closed for this pass.

## Hidden-Info Policy

The bridge consumes provider entries only. It does not inspect hidden card zones.

Bridge output and reason strings must not contain:

- opponent hand identities
- deck identities
- hidden marker internal ids
- raw schema data
- debug activation ids
- usage keys

Safe output may include visible board unit ids/card ids and the viewer's own source instance id if it was already in provider data.

## Boundary Rationale

This pass does not add UI. It only exposes a C++ binding step that future UI can call after selecting an option.

This pass does not add response windows. It binds a target for the existing immediate activation command shape only.

This pass does not create `FWBAction`. Activation remains separate from normal legal actions.

This pass does not change `WBActionCodec`. No activation ids are added to the normal action codec.

This pass does not change `WBRules::GenerateLegalActions`. Normal rules action generation remains unchanged.

## Risks And Open Questions

- Tile and wall target selection remain future work.
- The provider command is still only as complete as the provider entry supplied to the bridge.
- A later pass must decide whether bound commands flow into the existing handoff path directly or through a higher-level runtime session method.
- Response-window target changes, retargeting, negation, and stale board validation remain future work.
