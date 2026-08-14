# Godot Canon Reference

This folder contains tracked reference notes, indices, and golden scenario specs for porting Wandbound behavior from Godot to Unreal.

The full Godot project copy lives at `../GodotProject/godotcanon/`. Treat that project as read-only source material. Unreal code must not import, compile, load, or depend on Godot files at runtime, and the Godot copy must stay outside Unreal asset paths such as `Content/`.

Codex may inspect the Godot files for parity while implementing deterministic Unreal C++ rules. Product-owner-approved repository addenda supersede the Rules Bible only for the conflicts they explicitly register. If Godot code and the still-authoritative canon conflict, stop and ask the user before choosing behavior.

New Unreal mechanics should be backed by golden scenario tests before presentation, UI, camera, animation, VFX, audio, or Blueprint work. Preserve the canonical boundary: Rules validate legality, EffectRunner mutates state, and UI selects only.

Suggested read order:

1. `../GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
2. `../GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`

Explicit repository authority overrides are indexed in
`../../Docs/Repository_Canon_Reconciliation_Audit.md`. In particular,
`../../Docs/Wandbound_Game_Start_and_Turn_One_Addendum_v1.md` supersedes the
older sequential Hero setup and blanket first-player Turn 1 attack ban.
3. `GodotSourceIndex.md`
4. `GoldenScenarios/`
