# CardDB Unreal Schema Examples

These are documentation examples only. They are not parser input or production data.

Corresponding test-only fixture examples now live under `Reference/GodotCanon/CardDBSchemaFixtures/`. Those fixtures validate the documented schema shape and fail-closed diagnostics without creating a production importer.

Additional invalid examples cover unsupported card kind/source zone, malformed JSON, missing/malformed/empty payload arrays, malformed activated effects/source gates/cost gates, and malformed numeric fields.

## 1. Damage Effect Card

Board source, unit target, and optional RR cost:

```json
{
  "schema_version": 1,
  "card_id": "example_spark_bolt",
  "public_name": "Spark Bolt",
  "kind": "fixture",
  "public_text": "Activate: Deal 2 damage.",
  "activated_effects": [
    {
      "effect_id": "deal_2",
      "public_label": "Deal 2 damage",
      "target_requirement": "unit",
      "source_gate": {
        "required_zone": "board",
        "timing": "normal_turn_priority",
        "requires_source_unit": true,
        "requires_source_unit_ownership": true,
        "blocked_by_stunned": true,
        "blocked_by_frozen": false,
        "cost_gate": {
          "requires_external_affordability": true,
          "required_rr": 1,
          "cost_kind": "RR"
        }
      },
      "payloads": [
        {
          "type": "damage_effect",
          "amount": 2,
          "bypass_armor": false,
          "damage_cause": "Effect",
          "target": "selected"
        }
      ]
    }
  ]
}
```

Expected mapping:

- card maps to `FWBCardDefinition`
- effect maps to `FWBCardEffectDefinition`
- cost gate maps to `FWBCardActivationCostGateDefinition`
- payload maps to `FWBDamageEffectRequest`

## 2. Heal Effect Card

Hand source, unit target, and clamp-to-Max-HP policy:

```json
{
  "schema_version": 1,
  "card_id": "example_field_mend",
  "public_name": "Field Mend",
  "kind": "fixture",
  "public_text": "Activate: Heal 2 HP.",
  "activated_effects": [
    {
      "effect_id": "heal_2",
      "public_label": "Heal 2 HP",
      "target_requirement": "unit",
      "source_gate": {
        "required_zone": "hand",
        "timing": "normal_turn_priority",
        "requires_fixture_zone_ownership": true
      },
      "payloads": [
        {
          "type": "heal_effect",
          "amount": 2,
          "target": "selected"
        }
      ]
    }
  ]
}
```

Expected policy:

- healing clamps to Max HP
- hand visibility is owned-player scoped
- opponent hand identity must not appear in public output

## 3. Status Effect Card

Apply Burn for 2 turns:

```json
{
  "schema_version": 1,
  "card_id": "example_cinder_mark",
  "public_name": "Cinder Mark",
  "kind": "fixture",
  "public_text": "Activate: Apply Burn.",
  "activated_effects": [
    {
      "effect_id": "burn_2",
      "public_label": "Apply Burn",
      "target_requirement": "unit",
      "source_gate": {
        "required_zone": "board",
        "timing": "normal_turn_priority",
        "requires_source_unit": true,
        "requires_source_unit_ownership": true
      },
      "payloads": [
        {
          "type": "status_effect",
          "operation": "apply_status",
          "status_id": "Burn",
          "turns_remaining": 2,
          "target": "selected"
        }
      ]
    }
  ]
}
```

Expected mapping:

- payload maps to `FWBStatusEffectRequest`
- status id should canonicalize through current status effect policy

## 4. Armor Effect Card

Restore Armor on a unit target:

```json
{
  "schema_version": 1,
  "card_id": "example_reforge_guard",
  "public_name": "Reforge Guard",
  "kind": "fixture",
  "public_text": "Activate: Restore Armor.",
  "activated_effects": [
    {
      "effect_id": "restore_armor",
      "public_label": "Restore Armor",
      "target_requirement": "unit",
      "source_gate": {
        "required_zone": "board",
        "timing": "normal_turn_priority",
        "requires_source_unit": true
      },
      "payloads": [
        {
          "type": "armor_effect",
          "operation": "restore_armor_to_max",
          "target": "selected"
        }
      ]
    }
  ]
}
```

Expected mapping:

- payload maps to `FWBArmorEffectRequest`

## 5. Once-Per-Turn Activation

Usage gate example:

```json
{
  "schema_version": 1,
  "card_id": "example_daily_spark",
  "public_name": "Daily Spark",
  "kind": "fixture",
  "public_text": "Activate once per turn: Deal 1 damage.",
  "activated_effects": [
    {
      "effect_id": "once_damage",
      "public_label": "Deal 1 damage",
      "target_requirement": "unit",
      "source_gate": {
        "required_zone": "board",
        "timing": "normal_turn_priority",
        "requires_source_unit": true,
        "once_per_turn": true,
        "once_per_turn_key": "daily_spark"
      },
      "payloads": [
        {
          "type": "damage_effect",
          "amount": 1,
          "target": "selected"
        }
      ]
    }
  ]
}
```

Expected policy:

- usage filters candidate generation
- usage is marked only after successful activation execution
- failed activation does not mark usage

## 6. Costed Activation

RR cost gate example:

```json
{
  "schema_version": 1,
  "card_id": "example_heavy_burst",
  "public_name": "Heavy Burst",
  "kind": "fixture",
  "public_text": "Activate: Deal 3 damage.",
  "activated_effects": [
    {
      "effect_id": "heavy_burst",
      "public_label": "Deal 3 damage",
      "target_requirement": "unit",
      "source_gate": {
        "required_zone": "board",
        "timing": "normal_turn_priority",
        "requires_source_unit": true,
        "cost_gate": {
          "requires_external_affordability": true,
          "required_rr": 2,
          "cost_kind": "RR"
        }
      },
      "payloads": [
        {
          "type": "damage_effect",
          "amount": 3,
          "target": "selected"
        }
      ]
    }
  ]
}
```

Expected policy:

- affordability checks `Available RL`
- payment mutates `RL Used` only on successful activation execution
- provider output generation does not pay costs

## 7. Unsupported Payload Example

Unsupported draw payload:

```json
{
  "schema_version": 1,
  "card_id": "example_draw",
  "public_name": "Draw Example",
  "kind": "fixture",
  "public_text": "Activate: Draw 2 cards.",
  "activated_effects": [
    {
      "effect_id": "draw_2",
      "public_label": "Activate",
      "target_requirement": "none",
      "source_gate": {
        "required_zone": "hand",
        "timing": "normal_turn_priority"
      },
      "payloads": [
        {
          "type": "draw_cards",
          "amount": 2
        }
      ]
    }
  ]
}
```

Expected fail-closed diagnostic:

```text
unsupported_payload_type
```

Reason: draw/search/deck manipulation is not represented by the current generic payload families.

## 8. Bad Player-Facing Label Example

Invalid label:

```json
{
  "effect_id": "bad_label",
  "public_label": "Run EffectRunner damage_effect",
  "target_requirement": "unit",
  "payloads": [
    {
      "type": "damage_effect",
      "amount": 2,
      "target": "selected"
    }
  ]
}
```

Expected fail-closed diagnostic:

```text
player_facing_label_contains_internal_term
```

Corrected label:

```json
{
  "effect_id": "good_label",
  "public_label": "Deal 2 damage",
  "target_requirement": "unit",
  "payloads": [
    {
      "type": "damage_effect",
      "amount": 2,
      "target": "selected"
    }
  ]
}
```

## 9. Strict Valid Metadata Example

Allowed metadata fields stay inside explicit `metadata` objects:

```json
{
  "schema_version": 1,
  "metadata": {
    "author": "WandboundTests",
    "notes": "Allowed authoring notes.",
    "source": "Unreal fixture",
    "version": "1",
    "test_only": true
  },
  "card_id": "example_strict_metadata",
  "public_name": "Spark Archive",
  "kind": "fixture",
  "public_text": "Activate: Deal 1 damage.",
  "activated_effects": [
    {
      "effect_id": "deal_1",
      "public_label": "Deal 1 damage",
      "target_requirement": "unit",
      "metadata": {
        "notes": "Allowed effect metadata.",
        "source": "Unreal fixture",
        "test_only": true
      },
      "source_gate": {
        "required_zone": "board",
        "timing": "normal_turn_priority",
        "metadata": {
          "notes": "Allowed source gate metadata.",
          "test_only": true
        }
      },
      "payloads": [
        {
          "type": "damage_effect",
          "amount": 1,
          "target": "selected",
          "metadata": {
            "notes": "Allowed payload metadata.",
            "test_only": true
          }
        }
      ]
    }
  ]
}
```

Expected strict-mode result:

```text
valid
```

## 10. Strict Invalid Unknown-Field Example

Unknown fields fail only when strict unknown-field rejection is enabled:

```json
{
  "schema_version": 1,
  "extra_authoring_field": true,
  "card_id": "example_unknown_field",
  "public_name": "Spark Bolt",
  "kind": "fixture",
  "public_text": "Activate: Deal 2 damage.",
  "activated_effects": [
    {
      "effect_id": "deal_2",
      "public_label": "Deal 2 damage",
      "target_requirement": "unit",
      "payloads": [
        {
          "type": "damage_effect",
          "amount": 2,
          "target": "selected"
        }
      ]
    }
  ]
}
```

Expected strict-mode diagnostic:

```text
unknown_top_level_field
```

Expected default non-strict result:

```text
valid
```
