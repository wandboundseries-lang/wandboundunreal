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

No `match_spec.json` is present. Canonical sources identify two Hero candidates,
but every tracked deck containing them also requires definitions that remain
unsupported. `match_status.json` records the resulting named fail-closed status.

The suite contains no models, asset package paths, hidden match state, Godot
source data, test-only metadata, unsupported effect payloads, or fabricated deck
membership.
