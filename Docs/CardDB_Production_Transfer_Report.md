# CardDB Production Transfer Report

## Scope

This audit compares the runtime-loaded Godot CardDB source set under `Reference/GodotProject/godotcanon/scripts/data/CardDB` with the validated Unreal production CardDB snapshot format. Godot files were read only. No canonical Godot definition was copied into the synthetic Unreal fixture.

## Result

| Classification | Count |
|---|---:|
| Deferred | 26 |
| UnsupportedEffect | 218 |

- Canonical definitions inspected: 244
- Canonical definitions transferred: 0
- Synthetic Unreal fixture definitions: 7
- Unsupported effect/passive/directive types: 97

No canonical definition is reported as production-ready. Definitions without unsupported behavior remain `Deferred` because they have not been normalized into an explicit production manifest.

## Material Mismatches

- The canonical Unreal rules kernel treats `AR` as attack range. The task wording also described base Armor; this pass does not invent a conflicting base-Armor definition field.
- Godot Characters and NPCs generally rely on implicit movement and attack patterns. Production Unreal requires explicit deterministic patterns.
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

The JSON companion contains one deterministically sorted entry per definition with ID, display name, canonical category, source path, Unreal classification, field mismatches, unsupported effect types, and corrective action. `effects_react.json` is used only as a parseable mirror; provenance points to the runtime-loaded `effects_react.gd` source.
