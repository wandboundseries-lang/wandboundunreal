# Godot Canon Reference

This folder contains tracked reference notes, indices, and golden scenario specs for porting Wandbound behavior from Godot to Unreal.

The full Godot project copy lives at `../GodotProject/godotcanon/`. Treat that project as read-only source material. Unreal code must not import, compile, load, or depend on Godot files at runtime, and the Godot copy must stay outside Unreal asset paths such as `Content/`.

Codex may inspect the Godot files for parity while implementing deterministic Unreal C++ rules. If Godot code and `Wandbound_Rules_Bible_v2.txt` conflict, stop and ask the user before choosing behavior.

New Unreal mechanics should be backed by golden scenario tests before presentation, UI, camera, animation, VFX, audio, or Blueprint work. Preserve the canonical boundary: Rules validate legality, EffectRunner mutates state, and UI selects only.

Suggested read order:

1. `../GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
2. `../GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`
3. `GodotSourceIndex.md`
4. `GoldenScenarios/`
