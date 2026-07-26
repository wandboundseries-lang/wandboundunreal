# Character Source Bundles

Create one folder per approved character:

```text
SourceAssets/Characters/<character_id>/
  character_manifest.json
  model/
  textures/
  animations/
  previews/
  notes/
```

Only the manifest and one `.fbx`, `.glb`, or `.gltf` model are mandatory. Raw source files stay here; imported Unreal assets go to `Content/Wandbound/Characters/<character_id>/`.

Create a template:

```powershell
.\Scripts\Assets\NewWandboundCharacterBundle.ps1 -CharacterId "example_guardian" -DisplayName "Example Guardian" -CardDefinitionId "CHAR_EXAMPLE_GUARDIAN"
```

Validate it:

```powershell
.\Scripts\Assets\ImportWandboundCharacter.ps1 -CharacterId "example_guardian"
```

The pipeline never stages, commits, deletes, downloads, converts, or executes files from a bundle.
