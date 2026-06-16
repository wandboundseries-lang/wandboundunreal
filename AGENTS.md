# Wandbound Unreal Codex Guide

## Project Purpose

This is the Unreal Engine migration/prototype of Wandbound. The Godot project remains the behavior reference until parity is proven. Unreal work must be rules-kernel-first: prove deterministic rules, state, effects, actions, and replay in headless C++ before building presentation.

## Non-Negotiable Architecture

- Rules/state/effects/action generation belong in C++.
- UI, camera, animation, VFX, audio, and Blueprints are presentation.
- Actors display state; Actors do not own rules truth.
- Rules validate legality.
- EffectRunner mutates state.
- UI selects only.
- View/camera/UI never affect rules.
- Do not implement core rules in Blueprint.

## Asset Safety

- Do not edit .uasset or .umap files unless explicitly requested.
- Do not create or modify Blueprints unless explicitly requested.
- Prefer C++ and data files for Codex work.
- If asset changes are needed, create an Editor Utility/Python/C++ commandlet or give clear manual instructions.

## Godot Reference Access

- Codex should consult `Reference/GodotCanon/` and `Reference/GodotProject/` before implementing rules, actions, effects, combat, state, or replay.
- Godot files are read-only reference material.
- Do not edit Godot reference files unless explicitly requested.
- Do not import, compile, or runtime-load Godot files from Unreal.
- Do not copy Godot architecture blindly; port behavior into deterministic Unreal C++ rules modules.
- Preserve canonical rule boundary: Rules validate legality, EffectRunner mutates state, UI selects only.

## Git Safety

- Always inspect git status before work.
- Keep changes scoped to the requested pass.
- Do not rewrite history.
- Do not delete assets without explicit instruction.
- Return files changed and test output.

## Build and Test Commands

Project root:

```powershell
C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE
```

Detected Unreal paths:

```powershell
C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe
C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat
C:\Program Files\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe
C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe
```

Generate project files if needed:

```powershell
& 'C:\Program Files\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe' /projectfiles 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject'
```

Fallback project-file generation through UBT:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' -ProjectFiles -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -Game -Engine -Progress
```

Build editor target:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Run automation tests:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Project; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Project'
```

Run a specific Wandbound test group once it exists:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Generated file tracking policy:

- Generated Unreal/Visual Studio folders must not be tracked:
  - `.vs/`
  - `Binaries/`
  - `Intermediate/`
  - `Saved/`
  - `DerivedDataCache/`
- Generated logs, caches, object files, PDBs, PCH files, and local user settings must not be tracked.
- Codex may remove these from Git tracking with `git rm --cached`, but must not delete the local developer's working files unless explicitly requested.
- Real source/config/docs/reference files must remain tracked.

## Future Module Plan

- WandboundCore: pure rules/state/actions/effects/replay
- WandboundRuntime: board actors, camera, visual runtime bridges
- WandboundUI: UMG/widget bridge code if needed later
- WandboundTests: automation tests

Proposed future layout:

```text
Source/WandboundCore/
Source/WandboundRuntime/
Source/WandboundTests/
```

Create module folders/build files only after explicit confirmation, then update the .uproject module list in the same pass.

## Coding Standards

- Prefer deterministic pure C++ for rules.
- Avoid UObject dependencies in core state structs unless needed for Unreal reflection.
- Keep core state serializable.
- Add tests with each mechanic.
- Do not add hidden-information leaks.
- Do not add gameplay behavior without tests.

## Reporting Format

Codex should return:

- files changed
- build result
- test result
- exact errors
- risks/unknowns
- next recommended prompt
