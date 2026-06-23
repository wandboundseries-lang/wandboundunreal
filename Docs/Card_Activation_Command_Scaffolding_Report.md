# Card Activation Command Scaffolding Report

## Scope

This pass adds deterministic card activation command scaffolding for Unreal-owned fixtures. It bridges external activation data into the existing generic `FWBEffectRequest` dispatcher.

Implemented:

- `FWBCardActivationSource`
- `FWBCardActivationCommand`
- `FWBCardActivationCommandResult`
- `WBRules::CanApplyCardActivationCommand`
- `WBEffectRunner::ApplyCardActivationCommand`
- fixture operation `apply_card_activation_command`
- parent trace kind `card_activation_resolved`
- fixture-driven card activation command scenarios

Not implemented:

- Godot CardDB import
- JSON card database loading
- legal action generation for card activation
- card ownership checks
- costs
- activation timing windows
- response windows
- negation
- UI target selection
- card-specific behavior
- passives
- wands
- Blueprints, assets, `.uasset`, or `.umap` work

## Command Shape

`FWBCardActivationSource` carries:

- player id
- optional source unit id
- source card id
- source effect id

`SourceCardId`, `SourceEffectId`, and `DebugActivationId` are fixture/debug metadata. They are not used for CardDB lookup and are not serialized into traces or public runtime summaries.

`FWBCardActivationCommand` carries:

- activation source metadata
- one externally supplied `FWBEffectRequest`
- debug activation id

The effect request remains the only behavior payload.

## Definition Expansion Follow-Up

`WBCardActivationExpansion` now builds `FWBCardActivationCommand` values from fixture-owned `FWBCardDefinition` data. That layer does not import Godot CardDB, generate legal actions, check ownership/cost/timing, or call rules/effect runner code. It only expands a requested fixture effect id and target into the command shape described here.

## Validation

`WBRules::CanApplyCardActivationCommand` checks:

- game is not over
- source player id is valid and has player state
- explicit source unit exists when provided
- explicit source unit is not defeated or removed
- explicit source unit belongs to the source player
- effect request source player matches the command source or is fillable
- effect request source unit matches the command source or is fillable
- payloads are non-empty
- the filled request passes `CanApplyEffectRequest`

Source-less/global fixture effects are allowed when `SourceUnitId == -1` and `PlayerId` is valid.

Validation intentionally does not check card ownership, hand/deck location, resource costs, response timing, CardDB definitions, or target-selection UI.

## Runner Behavior

`WBEffectRunner::ApplyCardActivationCommand`:

- validates first
- fills missing effect-request source fields from the command source
- applies the request through `ApplyEffectRequest` on a working state copy
- commits only after the effect request succeeds
- returns no success traces on validation or effect-request failure

Successful trace order:

1. `card_activation_resolved`
2. `effect_request_resolved`
3. child effect traces in payload order

## Hidden Information

`card_activation_resolved` includes only:

- player id
- source unit id
- target unit id
- ok flag

It excludes source card id, source effect id, debug activation id, deck contents, hand contents, discard contents, and pending private choice data.

## Fixtures

Added GodotCanon fixtures:

- `card_activation_armor_effect_success.json`
- `card_activation_status_effect_success.json`
- `card_activation_damage_effect_success.json`
- `card_activation_heal_effect_success.json`
- `card_activation_mixed_payload_atomic_success.json`
- `card_activation_mixed_payload_atomic_failure.json`
- `card_activation_missing_source_unit_fails.json`
- `card_activation_removed_source_unit_fails.json`
- `card_activation_missing_target_fails.json`
- `card_activation_source_metadata_not_public_trace.json`

## Tests

Added `WBCardActivationCommandScaffoldingTests.cpp`, covering:

- source validation success
- missing source failure
- removed source failure
- source-less/global fixture activation
- source field filling
- source mismatch failure
- armor/status/damage/heal activation dispatch
- mixed payload atomic success
- mixed payload atomic rollback
- parent trace ordering
- hidden source metadata exclusion
- legal action generation unchanged
- `WBActionCodec` unchanged
- fixture-driven scenario coverage
- runtime serialization hidden-data coverage

## Validation

Build:

```text
Result: Succeeded
Total execution time: 43.59 seconds
```

Wandbound automation:

```text
succeeded=517
succeededWithWarnings=0
failed=0
notRun=0
```

`git diff --check` reported no whitespace errors, only LF-to-CRLF working-copy notices.

Exact final errors: none.

## Remaining Work

- CardDB importer
- card activation legal action generation
- target selection
- costs
- timing/priority integration
- response windows
- effect negation
- passives
- wands
- card-specific effects
- UI and runtime activation presentation
