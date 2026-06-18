# Status Effect Request Scaffolding Report

## Scope

This pass adds deterministic generic status effect scaffolding for Unreal-owned fixture data. Status effects can now run standalone through `WBEffectRunner::ApplyStatusEffect` or as `status_effect` payloads inside `FWBEffectRequest`.

Implemented status operations:

- `apply_status`
- `remove_status`
- `set_status_duration`
- `add_status_duration`
- `reduce_status_duration`
- `cleanse_status`
- `cleanse_all_statuses`

Not implemented:

- Godot CardDB import
- JSON card database loading
- real card activation timing
- target selection UI
- player legal action generation for effects
- response windows
- effect negation
- passives
- wands
- card-specific status behavior
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work

## Canonical Status Names

Known public status ids are canonicalized to stable Unreal names:

- `burn` -> `Burn`
- `poison` -> `Poison`
- `root` / `rooted` -> `Rooted`
- `stun` / `stunned` -> `Stunned`
- `frozen` -> `Frozen`

Unknown non-empty status ids are left unchanged for future fixture-owned scaffolding. Empty status ids fail for single-status operations.

## Duration Behavior

`Duration == 0` means permanent/untimed in this Unreal pass. Positive durations are stored in `FWBUnitState::StatusTurnsRemaining`.

Operation behavior:

- `ApplyStatus` adds or refreshes the canonical status using the incoming duration.
- `SetStatusDuration` requires an active status and replaces its duration.
- `AddStatusDuration` requires an active status; untimed statuses receive the incoming positive duration.
- `ReduceStatusDuration` requires an active status; timed statuses expire at zero, while untimed statuses remain unchanged.
- Negative duration fails without mutation.

Godot uses `-1` for some untimed paths; this pass intentionally keeps Unreal's existing `Duration == 0` convention per the migration request.

## Cleanse Behavior

`RemoveStatus` and `CleanseStatus` remove one active status. Missing status is a successful no-op, matching Godot `remove_status`.

`CleanseAllStatuses` removes every active status from the unit, clears duration metadata, and reports deterministic sorted public status ids in `RemovedStatuses`.

## Effect Request Payload Support

`EWBGenericEffectOp` now supports:

- `armor_effect`
- `status_effect`

`FWBGenericEffectPayload` now carries `FWBStatusEffectRequest StatusEffect`.

`WBRules::CanApplyEffectRequest` validates status payload target, target mismatch, removed target, unknown status operation, negative duration, and required status ids before dispatch.

`WBEffectRunner::ApplyEffectRequest` fills a missing status payload target from the request target and dispatches through `WBEffectRunner::ApplyStatusEffect`.

## Atomicity

Multi-payload effect requests still use a working state copy. If any payload fails, earlier payload mutations are discarded and no success traces are returned.

Mixed armor/status payload fixtures cover both atomic success and atomic failure.

## Trace Behavior

Successful standalone status effects emit:

1. `status_modified`
2. one `status_removed` trace for each removed status, when applicable

Successful effect requests emit:

1. `effect_request_resolved`
2. child payload traces such as `armor_modified` and `status_modified`

Added trace fields:

- `status_effect_operation`
- `removed_statuses`

Existing `status_tick`, `status_expired`, and `status_removed` traces remain compatible.

## Hidden Information

Status effect traces and public summaries include only public status names, target unit ids, owner ids, and duration deltas. Runtime serialization still excludes deck, hand, discard, source card id, source effect id, private choices, and pending attack internals.

## Fixtures And Tests

Added standalone status effect fixtures under `Reference/GodotCanon/GoldenScenarios/`.

Added effect request fixtures for:

- status apply
- status cleanse-all
- mixed armor/status atomic success
- mixed armor/status atomic failure
- runtime public serialization after status effect request

Added automation coverage in:

- `WBStatusEffectScaffoldingTests.cpp`
- `WBEffectRequestScaffoldingTests.cpp`

## Remaining Work

- Godot CardDB importer
- card activation commands
- target selection
- real card ownership/timing validation
- response windows
- effect negation
- passives
- wands
- card-specific status effects
- terrain/wall/card status interactions
