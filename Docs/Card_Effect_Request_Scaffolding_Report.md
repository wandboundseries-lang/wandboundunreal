# Card Effect Request Scaffolding Report

## Scope

This pass adds deterministic card-effect request scaffolding for Unreal-owned fixture data. It creates a generic request transport and dispatcher, but only executes generic operations already implemented in Unreal.

Implemented payload operations:

- `armor_effect`
- `status_effect`
- `damage_effect`
- `heal_effect`

Not implemented:

- Godot CardDB import
- JSON card database loading
- real card activation timing
- target-selection UI
- player legal action generation for effects
- response windows
- effect negation
- passives
- wands
- card-specific effects
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work

## Request Shape

Added:

- `EWBGenericEffectOp`
- `FWBEffectSourceRef`
- `FWBEffectTargetRef`
- `FWBGenericEffectPayload`
- `FWBEffectRequest`
- `FWBEffectRequestResult`

`FWBEffectSourceRef` carries:

- player id
- source unit id
- source card id
- source effect id

`SourceCardId` and `SourceEffectId` are fixture/debug metadata only. They are not used for card lookup and are not emitted in traces or public summaries.

`FWBEffectTargetRef` carries:

- target unit id
- target tile
- target wall edge

Only target unit id is used by the current `armor_effect`, `status_effect`, `damage_effect`, and `heal_effect` payloads.

## Validation

Added:

- `WBRules::CanApplyEffectRequest`

Validation checks:

- game is not over
- optional source player id is valid
- optional source unit exists and is not removed/defeated
- payload array is non-empty
- payload operation is known
- armor payload request target exists
- armor payload target matches request target if set
- target unit exists and is not removed/defeated
- armor operation and amount are valid
- status operation, duration, and required status id are valid
- damage target exists, matches the request target when supplied, and amount is non-negative
- heal target exists, matches the request target when supplied, and amount is non-negative

This is generic request validation, not player legal action validation. It does not validate hand/deck/card ownership, activation timing, response timing, or UI target selection.

## Dispatcher

Added:

- `WBEffectRunner::ApplyEffectRequest`

The dispatcher validates the request, copies `FWBGameStateData`, applies payloads sequentially to the copy, and assigns the original state only after every payload succeeds.

Atomic behavior:

- validation failure: no mutation, no success traces
- payload failure: no mutation, no success traces
- all payloads succeed: state is assigned from the working copy and traces are returned

`armor_effect` payloads fill a missing `FWBArmorEffectRequest::TargetUnitId` from the request target, then route to `WBEffectRunner::ApplyArmorEffect`.

`status_effect` payloads fill a missing `FWBStatusEffectRequest::TargetUnitId` from the request target, then route to `WBEffectRunner::ApplyStatusEffect`.

`damage_effect` payloads fill missing target/source ids from the request shell, then route to `WBEffectRunner::ApplyDamageEffect`.

`heal_effect` payloads fill missing target/source ids from the request shell, then route to `WBEffectRunner::ApplyHealEffect`.

## Trace Behavior

Successful effect requests emit:

1. `effect_request_resolved`
2. child payload traces in payload order, such as `armor_modified`, `status_modified`, `damage_effect_resolved`, or `heal_effect_resolved`

`effect_request_resolved` includes only safe fields:

- kind
- ok
- player id
- source unit id
- target unit id

It does not include source card id, source effect id, raw card payloads, hand indices, deck contents, or private choices.

## Fixture Operation

Added fixture support:

- `operation = apply_effect_request`

Fixture request shape:

```json
{
  "effect_request": {
    "source": {
      "player_id": 0,
      "source_unit_id": 1,
      "source_card_id": "fixture_card_armor",
      "source_effect_id": "fixture_effect_1"
    },
    "target": {
      "target_unit_id": 2
    },
    "payloads": [
      {
        "operation": "armor_effect",
        "armor_effect": {
          "operation": "add_current_armor",
          "target_unit_id": 2,
          "amount": 2,
          "source_reason": "fixture"
        }
      }
    ]
  }
}
```

Supported armor operation strings are the existing armor-effect fixture strings:

- `add_current_armor`
- `reduce_current_armor`
- `set_current_armor`
- `add_max_armor`
- `reduce_max_armor`
- `set_max_armor`
- `restore_armor_to_max`

Supported status operation strings are:

- `apply_status`
- `remove_status`
- `set_status_duration`
- `add_status_duration`
- `reduce_status_duration`
- `cleanse_status`
- `cleanse_all_statuses`

Supported damage payload fields are:

- `target_unit_id`
- `source_unit_id`
- `source_player_id`
- `amount`
- `bypass_armor`
- `damage_cause`
- `source_reason`

Supported heal payload fields are:

- `target_unit_id`
- `source_unit_id`
- `source_player_id`
- `amount`
- `source_reason`

## Fixtures

Added GodotCanon fixtures:

- `effect_request_armor_add_current.json`
- `effect_request_armor_reduce_current.json`
- `effect_request_armor_add_max_then_restore.json`
- `effect_request_multiple_payloads_atomic_success.json`
- `effect_request_multiple_payloads_atomic_failure.json`
- `effect_request_missing_target_fails_no_mutation.json`
- `effect_request_removed_target_fails_no_mutation.json`
- `effect_request_unknown_operation_fails_no_mutation.json`
- `effect_request_status_apply_burn.json`
- `effect_request_status_cleanse_all.json`
- `effect_request_armor_then_status_atomic_success.json`
- `effect_request_armor_then_status_atomic_failure.json`
- `runtime_result_serialization_after_status_effect_request.json`
- `runtime_result_serialization_after_effect_request_armor.json`
- `effect_request_damage_effect_basic.json`
- `effect_request_heal_effect_basic.json`
- `effect_request_damage_then_heal_atomic_success.json`
- `effect_request_damage_then_heal_atomic_failure.json`
- `effect_request_mixed_armor_status_damage_heal_atomic_success.json`
- `runtime_result_serialization_after_damage_heal_effect_request.json`
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

Added `WBEffectRequestScaffoldingTests.cpp`, covering:

- armor add-current request success
- armor reduce-current request success
- add-max then restore multi-payload success
- atomic failure without mutation
- missing target failure
- removed target failure
- unknown generic operation failure
- removed source unit failure when source unit id is provided
- optional source unit success
- parent trace ordering
- hidden-data exclusion from runtime serialization
- legal action generation unchanged
- `WBActionCodec` unchanged
- fixture-driven scenarios
- runtime serialization fixture
- status payload apply and cleanse coverage
- mixed armor/status atomic success and failure
- damage/heal payload dispatch
- mixed armor/status/damage/heal atomic success
- damage/heal atomic rollback on failure
- card activation command source validation
- card activation command source-field filling
- card activation command parent trace ordering
- card activation command hidden metadata exclusion
- card activation command fixture scenarios

## Hidden Information

Runtime/public serialization after an effect request or card activation command excludes:

- deck contents
- hand contents
- discard contents
- pending attack internals
- source card id
- source effect id
- debug activation id

Visible board unit card ids remain public under the existing public board summary policy.

## Remaining Work

- Godot CardDB importer
- card activation legal action generation
- target selection
- real card ownership/timing validation
- response windows
- effect negation
- passives
- wands
- card-specific armor cards
- broader generic effect payloads such as walls and terrain
