# Production Activation Execution Handoff Audit

Date: 2026-06-28

## Scope

This pass adds a narrow C++ runtime handoff adapter for the first production activation vertical slice:

provider entry -> selected target option -> target-selection bridge -> existing execution bridge -> provider refresh

It does not add UI, response windows, activation `FWBAction`, `WBActionCodec` support, `WBRules::GenerateLegalActions` changes, Godot CardDB import, or new effect semantics.

## Current Activation Command Execution Path

Core execution already exists in `WBEffectRunner::ApplyCardActivationCommand`.

That path:

- validates with `WBRules::CanApplyCardActivationCommand`
- fills missing effect source fields from the command source
- pays activation costs on a working state copy when requested
- applies existing generic payloads through `ApplyEffectRequest`
- marks once-per-turn usage on success
- commits the working state only after the command succeeds
- emits activation, cost, effect, and usage traces

Supported generic payloads remain the existing `armor_effect`, `status_effect`, `damage_effect`, and `heal_effect` behavior. This pass must not add payload types or card-specific effects.

## Current Runtime Execution Handoff Behavior

`WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff` is the runtime execution boundary. It receives mutable `FWBGameStateData&` and an `FWBRuntimeActivationExecutionHandoffResult`, validates that the handoff is resolved and contains a non-empty activation command, then delegates to `WBEffectRunner::ApplyCardActivationCommand`.

The production handoff adapter should call this bridge instead of calling `WBEffectRunner` directly. That keeps direct mutation inside the existing execution integration.

## Current Post-Activation Refresh Behavior

`WBRuntimePostActivationRefreshSequencer` can execute a selected activation id through an owner component and refresh presentation from caller-supplied post-action data. It intentionally does not compute new legal actions or activation data.

For this production pass, the adapter can refresh production activation entries directly by creating a fresh `FWBProductionActivationDataProvider` after successful execution and querying it against the updated state, repository, and viewer. This keeps runtime/provider state externally owned while giving tests a complete provider -> handoff -> refresh path.

## Current Provider Data Shape

`FWBProductionActivationDataProvider` is read-only. It accepts:

- const `FWBGameStateData*`
- const `FWBCardDefinitionRepository*`
- viewer player id

It emits `FWBRuntimeActivationDataProviderResult` containing an activation action set and public board summary. Provider actions include safe source card/effect ids, public labels, source unit ids, target requirements, unit target options, and a target-deferred command shell.

The provider does not include effect payloads in public action commands. Execution therefore needs to rebuild the internal command from the repository instead of using the public/scrubbed command as-is.

## Current Target-Selection Bridge Result Shape

`FWBProductionActivationTargetSelectionBridge` consumes provider activation entries and a selected provider target option. It returns:

- stable result code and reason
- a copied, target-bound command shell
- safe source card id
- safe effect id
- selected unit id

The bridge output is deliberately public/safe. It scrubs debug activation id and usage key and does not read game state, repository data, hidden zones, or Godot CardDB data.

## Internal Metadata Rehydration Plan

The production handoff adapter should:

1. query the configured provider for current activation entries
2. use the target-selection bridge to validate and bind the selected target
3. match the successful request back to the current provider entry by safe action id/source card/effect/source instance keys
4. look up the card definition in `FWBCardDefinitionRepository`
5. rebuild the internal `FWBCardActivationCommand` with `WBCardActivationExpansion::BuildActivationCommand`
6. pass the rebuilt command through `WBRuntimeActivationExecutionBridge`

The handoff result must not expose rebuilt debug activation ids, usage keys, raw fixture data, or hidden zone identities. If metadata cannot be resolved, the adapter should fail closed with `internal_execution_metadata_missing` or `command_rebuild_failed`.

## Chosen Adapter Location And API

The narrow adapter belongs in `Source/WandboundRuntime` because it coordinates runtime provider data, target selection, existing runtime execution integration, and post-action provider refresh.

Planned files:

- `Source/WandboundRuntime/Public/WBProductionActivationExecutionHandoff.h`
- `Source/WandboundRuntime/Private/WBProductionActivationExecutionHandoff.cpp`

The API should accept mutable game state, const repository, viewer id, const provider, and a target-selection request. It should return a safe result with status, reason, source card/effect id, selected target unit id, execution-applied flag, refreshed provider activation entries, and sanitized diagnostic codes.

## Unit-Target Behavior

Board-source and own-hand unit-target activations are supported when:

- provider data emits a matching action entry
- the selected unit target exactly matches one provider target option
- repository lookup succeeds
- command expansion succeeds
- existing runtime execution bridge accepts the command

Successful damage/heal/status/armor behavior remains whatever the existing core activation command path already supports.

## No-Target Behavior

The target-selection bridge supports `None` target requirements by returning a command shell with no selected target. Current `WBRules::CanApplyEffectRequest` still requires a target unit for existing generic payloads, so no-target execution may return an execution failure if the underlying effect payload needs a unit target.

This pass documents that behavior and does not invent new no-target effect semantics.

## Tile And Wall Behavior

Tile and wall-edge target requirements remain unsupported for production handoff. The target-selection bridge fails them closed as `unsupported_target_requirement`, and the handoff adapter should surface that without mutation.

## Failure Behavior

Validation failures before execution must not mutate game state and do not require provider refresh.

Execution failures should return `execution_rejected` when the existing execution bridge refuses to attempt execution, or `execution_failed` when execution is attempted and the core command path fails.

Provider refresh failures after successful execution should return `provider_refresh_failed`.

## Hidden-Info Policy

Safe output may contain visible board unit ids, visible board card ids already present in target options, own-hand source instance ids if already in provider action ids, source card id, effect id, and public labels.

Handoff output, diagnostics, refreshed entries, and test-visible strings must not contain:

- opponent hand card or instance ids
- deck card or instance ids
- hidden marker internal card ids
- raw source JSON
- raw schema labels
- debug activation ids
- usage keys

Diagnostics should be copied as stable code strings only.

## Boundaries Preserved

This pass does not add UI because selection is supplied as a C++ request and target picking remains future presentation work.

This pass does not add response windows because current activation execution is a direct command execution path with no priority/pass layer changes.

This pass does not create `FWBAction` because production activations still live in the separate `FWBCardActivationLegalAction` surface.

This pass does not change `WBActionCodec` because there are still no activation action ids in normal replay/action codec output.

This pass does not change `WBRules::GenerateLegalActions` because activation entries are supplied by the production activation provider, not the normal action generator.

## Risks And Open Questions

- No-target effects remain limited by current generic effect target validation.
- Tile/wall selection needs target option enumeration and geometry policy before handoff support.
- The adapter cannot currently clone a custom provider config, so post-action refresh uses a fresh default production provider configuration.
- Played-card movement, discard, equip, summon, response windows, and card lifecycle remain future passes.
- Public labels and hidden-zone policies should continue to be tested as production provider coverage expands.
