# Repo Hygiene Report

Date of check: 2026-06-16 (America/New_York)

## Summary

This pass verified the repository baseline for generated Unreal, Visual Studio, cache, and local machine files. No gameplay, rules behavior, assets, Blueprints, or Godot reference files were changed.

## Tracked Generated Files

Targeted checks found no tracked generated files under:

- `.vs/`
- `Binaries/`
- `Intermediate/`
- `Saved/`
- `DerivedDataCache/`

Targeted pattern checks found no tracked:

- `*.pdb`
- `*.obj`
- `*.pch`
- `*.ilk`
- `*.log`
- `Saved/Logs/`
- `Saved/Config/WindowsEditor/`
- common IDE user files
- Unreal autosaves/backups

## Removed From Git Tracking

No generated files were tracked, so no `git rm --cached` cleanup was required.

`.gitignore.txt` was not present and was not tracked.

## Final .gitignore Policy

The root `.gitignore` excludes generated Unreal and local machine output:

- `.vs/`
- `Binaries/`
- `DerivedDataCache/`
- `Intermediate/`
- `Saved/`
- `.vscode/`
- `.idea/`
- `Autosaves/`
- `Backup/`
- build/log/cache patterns such as `*.obj`, `*.pdb`, `*.ilk`, `*.log`, `*.tmp`, and `*.pch`

The root `.gitignore` keeps real project material trackable:

- `Config/`
- `Content/`
- `Source/`
- `Plugins/`
- `Docs/`
- `Reference/`
- `AGENTS.md`
- `*.uproject`
- `*.sln`
- `.vsconfig`

`Reference/GodotProject/` remains ignored so the full copied Godot project is not committed. `Reference/GodotCanon/` remains trackable.

## Suspicious Tracked Files Intentionally Kept

No suspicious generated files were found tracked by the targeted checks.

## Manual Action Needed

No manual action is required from the developer for this pass.

If generated files are accidentally committed later, remove them from tracking only with `git rm --cached` and leave local developer files on disk unless deletion is explicitly requested.
