# Initial Canonical Production CardDB

This directory contains the first fail-closed canonical production definition
suite. It is runtime text data owned by Unreal and has no Godot runtime
dependency.

The suite transfers only definitions whose complete canonical behavior maps to
the current deterministic Unreal runtime. Global canonical defaults are made
explicit as `orthogonal_adjacent` movement and `orthogonal_line` attacks with
range equal to AR. AR remains Attack Range.

`char_test_01` and `char_test_03` retain their canonical IDs despite the legacy
`test` token because tracked tutorial setup explicitly uses those definitions as
Heroes. They are not synthetic Unreal test-fixture definitions.

The product-owner-approved Active Format v1 and game-start addendum now supply
the format and setup authority delegated by the Rules Bible. The suite includes
their digest-pinned runtime data, `trap_generic_01`, and the first valid
`match_spec.json`. The match uses two seven-Character decks, repeated supported
Trap slots in each Setup Kit, both supported NPCs, deterministic marker
placements, seeded shuffle, and a seed-derived first player.

The suite contains no models, asset package paths, hidden match state, Godot
source data, test-only metadata, unsupported effect payloads, or fabricated deck
membership.
