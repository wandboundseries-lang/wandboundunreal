# CardDB Production Transfer Report

## Scope

This audit compares the read-only Godot CardDB source set under
`Reference/GodotProject/godotcanon/scripts/data/CardDB` with the validated
Unreal production CardDB snapshot format. The first eligible canonical subset is
now normalized under `Data/CardDB/Production/InitialCanonical/`.

## Result

| Classification | Count |
|---|---:|
| Transferred | 10 |
| Deferred | 16 |
| UnsupportedEffect | 218 |

- Canonical definitions inspected: 244
- Canonical definitions transferred: 10
- Initial canonical production definitions: 10
- Unsupported effect/passive/directive types: 97
- Production digest: `b406557f2f190818fe3460621bbbdfaf84abe53623ff26aa934588aad68bedde`

All transferred definitions preserve canonical identity, display name, stats,
rules text, and provenance. Unsupported definitions remain unsupported or
deferred; no behavior was removed to make a definition eligible.

## Canonical Terminology

- `AR` is Attack Range.
- `ATK` is Attack value.
- `RL` is Resonance Limit.
- `RR` is Resonance Requirement.
- No Armor value was inferred from `AR`.
- No base unit Armor field was invented.

## Transferred Definitions

| Definition ID | Name | Type | Previous | Final |
|---|---|---|---|---|
| `char_new_1_6_18_3` | New 1-6-18-3 | Character | Deferred | Transferred |
| `char_new_2_4_20_2` | New 2-4-20-2 | Character | Deferred | Transferred |
| `char_new_3_4_16_2` | New 3-4-16-2 | Character | Deferred | Transferred |
| `char_new_rl_duelist` | Resonance Needle | Character | Deferred | Transferred |
| `char_new_rl_monolith` | Resonance Monolith | Character | Deferred | Transferred |
| `char_test_01` | Arclight Crusader | Character | Deferred | Transferred |
| `char_test_02` | Arena Brawler | Character | Deferred | Transferred |
| `char_test_03` | Black Meridian Cole | Character | Deferred | Transferred |
| `npc_generic_01` | Dungeon Crawler | NPC | Deferred | Transferred |
| `npc_human_marksman` | Human Marksman | NPC | Deferred | Transferred |

The legacy `char_test_*` IDs are canonical CardDB IDs used by tracked tutorials,
not synthetic Unreal fixture IDs. They are preserved rather than renamed.

## Remaining Deferred Candidates

Fourteen definitions require unsupported implicit behavior: Black Meridian
Anchor, five CSN Characters, four Wandwright Characters, two profiled NPCs, and
two Wands. `officer_new_bulwark` and `trap_generic_01` remain deferred because
their explicit Armor and trap-damage fields cannot be preserved by the current
production definition schema.

The complete 26-entry eligibility record and exact reasons are in
`CardDB_Initial_Canonical_Eligibility_Report.json`.

## Hero And Match Status

`char_test_01` and `char_test_03` have explicit tracked hero-choice and tutorial
placement evidence. They remain Character definitions because Hero is a match
role in the canonical rules.

No `match_spec.json` was created. Rules Bible 23.3 delegates format,
faction, and card-count restrictions to an active format document, and no
active canonical format document exists in the tracked reference. Tutorial and
AI decks cannot supply production deck size, allowed types, copy limits,
ordering, mirrored-deck legality, or first-player policy. `match_status.json`
records
`production_match_spec_blocked_by_canonical_deck_evidence`; no deck list was
invented or reduced.

The exhaustive minimum-match audit found three additional definitions whose
behavior can map to existing production paths: `char_test_healer`,
`wand_equip_mender_thread`, and `trap_generic_01`. They were not transferred
because no canonically legal minimum deck can be computed, and adding unrelated
definitions would not lift the startup block. The evidence and stable
classification of all 244 definitions are recorded in
`CardDB_Minimum_Canonical_Match_Plan.json`.

## Material Mismatches

- Canonical default movement is normalized to `orthogonal_adjacent`, and
  canonical default attack behavior is normalized to `orthogonal_line` with
  range equal to AR, only for definitions with no card-specific override.
- `char_new_rl_monolith` preserves explicit `ATK 0` and `AR 0`.
- NPC target-priority profiles remain unsupported. Only NPCs using the existing
  generic deterministic NPC behavior transferred.
- Godot Quick and React effects require response-window timing, which is intentionally outside this pass.
- Godot Wands use legacy effect structures without the explicit source, cost, usage, and target gates required by the production Unreal schema.
- Hybrid sacrifice and Hero-replacement semantics are not represented by the current production schema.

## Unsupported Effects

The complete stable list is recorded in `CardDB_Production_Transfer_Report.json`:

- `after_opponent_mp_roll_d3_reduce_mp`
- `after_own_mp_roll_d3_add_mp`
- `apply_cannot_attack`
- `apply_resolved_character_effect_negation_while_target_on_board`
- `apply_status`
- `apply_status_if_target_on_terrain`
- `apply_status_to_enemy_units_on_terrain`
- `arm_tile_status_snare`
- `ash_eater_burn_pulse`
- `attack_break_walls`
- `attack_diagonal`
- `attack_ignore_walls`
- `attack_pierce_frozen`
- `attack_range_spotter`
- `attack_through_one_wall`
- `aura_enemy_stat_penalty_in_range`
- `aura_faction_stat_bonus`
- `aura_grant_passive_allies`
- `begin_deck_search`
- `begin_tile_choice`
- `bm_crossfire_wall_bonus`
- `bonus_attack_damage_vs_status`
- `census_breaker_full_board_pulse`
- `cleanse_all`
- `copy_unit_rl_total`
- `csn_inheritance`
- `damage`
- `destroy_unit`
- `double_stats_if_only_control_officers`
- `draw_cards`
- `enemy_mud_no_mp_gain_effects`
- `gain_extra_attack_on_break_frozen`
- `global_early_wall_removal_before_round_30`
- `grant_passive`
- `heal`
- `heal_unit`
- `heal_units_by_terrain`
- `immune_to_all_effects`
- `lava_burn_immune`
- `lava_slide`
- `lifesteal_on_damage`
- `marrow_attack_declared_coin_negate`
- `marrow_effect_activation_coin_claim`
- `modify_pending_attack`
- `modify_resource`
- `modify_stat`
- `move_break_walls`
- `move_fly_over_units`
- `move_ignore_walls`
- `move_within_range`
- `negate`
- `negate_all_other_character_effects_on_field`
- `negate_all_wands_on_board_except_hand`
- `negate_enemy_unit_effects_on_terrain`
- `no_repeat_attack_target_per_turn`
- `npc_friendly_target_protection`
- `on_burn_damage_gain_stat`
- `on_deal_damage_modify_named_ally_hp`
- `on_destroy_buff_other_active_faction_units`
- `on_destroy_summon_faction_from_hand_or_deck_to_last_tile`
- `on_enter_terrain_draw_then_discard`
- `on_event`
- `on_successful_attack_apply_status`
- `on_successful_attack_reduce_target_rl`
- `on_summon_draw_one_then_copy_effects_if_faction`
- `on_summon_transfer_current_atk_from_other_friendly_units`
- `once_per_turn_move_1_when_attacked`
- `once_per_turn_pay_1_hp_per_other_damaged_friendly_then_heal_each_1`
- `prevent`
- `redirect_attack_to_adjacent_friendly`
- `restore_armor`
- `return_equipped_wand_to_hand`
- `return_unit_to_hand`
- `return_wand_from_discard`
- `reveal_current_controller_hand_while_active`
- `rl_unchangeable`
- `rule`
- `sacrifice_self_summon_named_character_from_hand_or_deck`
- `set_resource`
- `set_terrain`
- `set_terrain_area`
- `stat_bonus_per_faction_unit`
- `status_immunity`
- `summon_choose_enemy_negate_effects_tether`
- `summon_named_character_from_hand_adjacent_to_self`
- `summon_self_from_hand_when_other_officer_destroyed_by_battle`
- `swap_units`
- `terrain_immunity`
- `terrain_polarity_stats`
- `terrain_traverse_free`
- `transfer_all_friendly_rl_to_self_on_summon`
- `traprunner_prevent_trap_damage`
- `wall_move`
- `wall_place`
- `wall_remove`
- `when_attacked_coin_redirect_or_take_extra_damage`
- `ww_start_turn_temporary_rl`

## Provenance

The JSON companion contains one deterministically sorted entry per definition
with ID, display name, canonical category, source path, Unreal classification,
field mapping, eligibility, unsupported effect types, and corrective action.
`effects_react.json` is used only as a parseable mirror; provenance points to
the runtime-loaded `effects_react.gd` source.
