# Wandbound Character Model Pipeline

This pipeline imports character models that you own into Wandbound. It never generates, downloads, or substitutes a model. The raw files remain separate from Unreal assets, and validation is safe to run before any import.

## Quick Start

1. Open PowerShell in the Wandbound project folder.
2. Create a bundle:

   ```powershell
   .\Scripts\Assets\NewWandboundCharacterBundle.ps1 `
     -CharacterId "example_guardian" `
     -DisplayName "Example Guardian" `
     -CardDefinitionId "CHAR_EXAMPLE_GUARDIAN"
   ```

3. Put exactly one model in `SourceAssets/Characters/example_guardian/model/`.
4. Put optional texture images in `textures/`, animation FBX files in `animations/`, and front/side/back images in `previews/`.
5. Edit `character_manifest.json` so every path and setting describes the supplied files.
6. Review the exact bundle. Set `approved_for_import` to `true`, fill `approved_by`, and describe the approval.
7. Validate without importing:

   ```powershell
   .\Scripts\Assets\ImportWandboundCharacter.ps1 -CharacterId "example_guardian"
   ```

8. Inspect `Docs/AssetImports/example_guardian/ImportReport.md`.
9. Preview the planned Unreal packages:

   ```powershell
   .\Scripts\Assets\ImportWandboundCharacter.ps1 -CharacterId "example_guardian" -Mode DryRun
   ```

10. Import after validation:

    ```powershell
    .\Scripts\Assets\ImportWandboundCharacter.ps1 -CharacterId "example_guardian" -Mode Import -GeneratePreview -ValidateCook
    ```

11. After changing a source file or import setting:

    ```powershell
    .\Scripts\Assets\ImportWandboundCharacter.ps1 -CharacterId "example_guardian" -Mode Reimport -GeneratePreview -ValidateCook
    ```

Validation and dry run do not create Unreal assets. Import and reimport write only the character's deterministic destination packages and text reports.

## Bundle Layout

```text
SourceAssets/Characters/<character_id>/
  character_manifest.json       required
  model/
    <one model>.fbx|glb|gltf    required
  textures/                     optional
  animations/                   optional; FBX clips
  previews/
    front.png                   optional
    side.png                    optional
    back.png                    optional
  notes/
    README.md                   optional
```

Do not put raw model files under `Content/`. Do not put `.uasset` files under `SourceAssets/`. The importer never reads a model from the existing Meshy folders; copying a file into a reviewed bundle is an explicit future user action.

## Static Or Skeletal

A **static** model has no animation skeleton. It can still move, turn, pulse, and receive other Wandbound transform fallbacks. Use:

```json
"model_type": "static",
"import_animations": false,
"create_physics_asset": false,
"skeleton_policy": "none",
"generate_collision": true
```

A **skeletal** model has bones. It may create its own skeleton or reuse an explicitly named compatible skeleton. It may have semantic animation clips. Wandbound does not assume characters share a skeleton and does not silently retarget animations.

## Supported Formats

The installed Unreal Engine 5.7 source was inspected for this project.

| Format | Static | Skeletal | Animations | Import system | Limitations |
| --- | ---: | ---: | ---: | --- | --- |
| FBX | Yes | Yes | Yes | `UFbxFactory` and `UFbxImportUI` | Preferred deterministic route. External clips must be FBX. Skeleton compatibility is validated; no automatic retargeting. |
| GLB | Yes | Engine Interchange intake | Embedded engine intake | Built-in Interchange glTF translator | The translator advertises meshes, materials, and animations. Skeletal results depend on source conformance and require report/manual review. |
| glTF | Yes | Engine Interchange intake | Embedded engine intake | Built-in Interchange glTF translator | External `.bin` and image dependencies must remain in the bundle. The current task uses Interchange auto-detection and validates the resulting class. |

No plugin setting is changed by the pipeline. FBX remains operational even if an Interchange configuration is unavailable.

## Complete Example Manifest

This example is synthetic and does not identify a production character.

```json
{
  "schema_version": 1,
  "character_id": "example_guardian",
  "display_name": "Example Guardian",
  "card_definition_id": "CHAR_EXAMPLE_GUARDIAN",
  "approval": {
    "approved_for_import": true,
    "approved_by": "user",
    "approval_note": "Reviewed this exact source bundle"
  },
  "source": {
    "model": "model/example_guardian.fbx",
    "model_format": "fbx",
    "model_type": "skeletal",
    "textures_directory": "textures",
    "animations_directory": "animations",
    "textures": [
      {
        "role": "base_color",
        "path": "textures/example_guardian_base_color.png",
        "required": true
      },
      {
        "role": "normal",
        "path": "textures/example_guardian_normal.png",
        "required": false
      },
      {
        "role": "orm",
        "path": "textures/example_guardian_orm.png",
        "required": false
      }
    ],
    "animations": [
      {
        "role": "idle",
        "path": "animations/example_guardian_idle.fbx",
        "required": false
      },
      {
        "role": "attack",
        "path": "animations/example_guardian_attack.fbx",
        "required": false
      }
    ]
  },
  "presentation": {
    "role": "player_unit",
    "scale": 1.0,
    "rotation": [0.0, 0.0, 0.0],
    "offset": [0.0, 0.0, 0.0],
    "facing_axis": "positive_x"
  },
  "import": {
    "import_materials": true,
    "import_textures": true,
    "import_animations": true,
    "create_physics_asset": true,
    "generate_collision": false,
    "skeleton_policy": "create",
    "normal_policy": "import_normals_and_tangents"
  },
  "previews": {
    "front": "previews/front.png",
    "side": "previews/side.png",
    "back": "previews/back.png"
  },
  "tags": ["owned_source", "reviewed"],
  "notes": "Synthetic documentation example."
}
```

## Manifest Fields

| Field | Required | Type | Meaning | Validation |
| --- | ---: | --- | --- | --- |
| `schema_version` | Yes | integer | Manifest format version | Must be `1` |
| `character_id` | Yes | string | Stable public asset identity | Lowercase letters/digits, single underscores; must match folder |
| `display_name` | Yes | string | Human-readable name | Nonempty |
| `card_definition_id` | Yes | string | Public definition mapping | Uppercase public ID syntax; warning when CardDB is unavailable |
| `approval.approved_for_import` | Yes | boolean | Explicit user approval boundary | Must be `true` for validation/import |
| `approval.approved_by` | Yes | string | Who reviewed the bundle | Nonempty |
| `approval.approval_note` | No | string | Review context | Text only |
| `source.model` | Yes | path | Primary model inside the bundle | Relative, supported extension, exists, no traversal |
| `source.model_format` | Yes | enum | `fbx`, `glb`, or `gltf` | Must match extension |
| `source.model_type` | Yes | enum | `static` or `skeletal` | Must agree with import settings |
| `source.textures_directory` | No | path | Texture folder | Bundle-relative |
| `source.animations_directory` | No | path | Animation folder | Bundle-relative |
| `source.textures[]` | No | array | Semantic texture declarations | Unique role, supported local image path |
| `source.animations[]` | No | array | Semantic animation declarations | Unique role, local FBX path |
| `presentation.role` | Yes | enum | `player_hero`, `player_unit`, or `neutral_npc` | Public presentation category only |
| `presentation.scale` | Yes | number | Runtime display scale | Finite, greater than 0, at most 1000 |
| `presentation.rotation` | Yes | number[3] | Euler rotation fallback | Three finite values |
| `presentation.offset` | Yes | number[3] | Display offset fallback | Three finite values |
| `presentation.facing_axis` | Yes | enum | Authored forward axis | Positive/negative X or Y |
| `import.import_materials` | Yes | boolean | Let model import create materials | Recorded in settings digest |
| `import.import_textures` | Yes | boolean | Let model import include textures | Recorded in settings digest |
| `import.import_animations` | Yes | boolean | Import skeletal animation | Rejected for static models |
| `import.create_physics_asset` | Yes | boolean | Create skeletal physics asset | Rejected for static models |
| `import.generate_collision` | Yes | boolean | Generate static collision | Deterministic import policy |
| `import.skeleton_policy` | Yes | enum | `none`, `create`, or `reuse` | `none` for static; create/reuse for skeletal |
| `import.existing_skeleton_package` | Conditional | package | Skeleton to reuse | Required and validated for `reuse` |
| `import.normal_policy` | Yes | enum | Compute/import normals/tangents | Must be a supported policy |
| `previews.front/side/back` | No | path | User review images | Local image path; missing is warning |
| `tags` | No | string[] | Search terms | Sorted in parsed data |
| `notes` | No | string | Public asset notes | Must not contain private gameplay fields |

## Deterministic Unreal Destinations

For the synthetic `example_guardian` skeletal bundle:

```text
/Game/Wandbound/Characters/example_guardian/Meshes/SK_example_guardian
/Game/Wandbound/Characters/example_guardian/Skeleton/SKEL_example_guardian
/Game/Wandbound/Characters/example_guardian/Physics/PHYS_example_guardian
/Game/Wandbound/Characters/example_guardian/Textures/T_example_guardian_base_color
/Game/Wandbound/Characters/example_guardian/Animations/A_example_guardian_idle
```

A static primary mesh uses `Meshes/SM_<character_id>` and has no Skeleton or Physics package. Names contain no timestamps, machine paths, or random suffixes.

## Import And Reimport

The receipt compares SHA-256 source hashes and a normalized settings digest. File timestamps are audit information only.

| State | Meaning | Next action |
| --- | --- | --- |
| `NeverImported` | No receipt exists | Use Import after review |
| `UpToDate` | Hashes, settings, and destinations match | No import needed |
| `SourceChanged` | Model, texture, preview, or animation bytes changed | Review and use Reimport |
| `ManifestChanged` | Manifest identity/content changed | Review manifest and use Reimport |
| `SettingsChanged` | Normalized import settings changed | Review expected result and use Reimport |
| `DestinationMissing` | A recorded Unreal destination is absent | Reimport |
| `DependencyMissing` | Receipt package set no longer matches | Review report; reimport |
| `ReimportRequired` | A combined condition requires work | Review diagnostics; reimport |
| `ImportFailed` | Previous receipt recorded failure | Fix exact error and retry |

Reimport targets the same package paths. Stale assets are reported, not deleted.

## Reports

Each run writes under `Docs/AssetImports/<character_id>/`:

- `SourceInventory.json`: sorted bundle files, sizes, SHA-256 hashes, roles, Git/LFS status.
- `ImportReceipt.json`: hashes, settings digest, destination packages, engine/importer versions, result.
- `ImportReport.json`: machine-readable identity, source, destination, transform, status, and diagnostics.
- `ImportReport.md`: the same review information in plain language.
- `PresentationProfileCandidate.json`: model and fallback bindings for later approval.
- `PreviewResult.json`: deterministic preview policy, render status, and output path.
- `CookVerification.json`: exact package-list readiness; it does not run a broad cook.

`Data/CharacterModels/CharacterModelCatalog.json` is the stable searchable index. Unreal `.uasset` files remain binary and are not needed to understand the catalog.

## Preview Behavior

User-supplied front, side, and back images are hashed like other source files. `-GeneratePreview` requests a fixed whole-character Unreal preview. A render-capable editor can render the imported asset; a `-NullRHI` process records `unsupported_under_null_rhi` without failing the import. The preview request never includes gameplay or private-zone data.

## Materials And Textures

Supported declared image extensions are PNG, JPG/JPEG, TGA, BMP, and EXR. Roles include base color, normal, ORM, roughness, metallic, opacity, emissive, and additional. Each role maps to one deterministic Texture package.

The importer may preserve materials embedded in a model and may import external declared textures. It does not broadly rewrite materials or perform artistic tuning. Missing required textures fail validation; missing optional textures warn. Unconnected textures and failed generated materials remain report/review work, while Wandbound presentation keeps its material fallback.

## Animations

External clips are FBX and use the roles `idle`, `move`, `attack`, `hit`, `summon`, `death`, and `activation`. Roles cannot repeat. Missing optional clips do not fail model import; the presentation candidate lists fallback requirements. Animation duration never changes gameplay timing, deterministic event order, or the sequence controller's duration policy.

## Git LFS

Git LFS stores large binary files without making normal Git history enormous. The auditor is read-only: it never stages files, edits `.gitattributes`, or runs `git lfs track`.

Review narrow rules for each approved character:

```gitattributes
SourceAssets/Characters/<character_id>/model/* filter=lfs diff=lfs merge=lfs -text
SourceAssets/Characters/<character_id>/textures/* filter=lfs diff=lfs merge=lfs -text
SourceAssets/Characters/<character_id>/animations/* filter=lfs diff=lfs merge=lfs -text
SourceAssets/Characters/<character_id>/previews/* filter=lfs diff=lfs merge=lfs -text
Content/Wandbound/Characters/<character_id>/**/*.uasset filter=lfs diff=lfs merge=lfs -text
```

Manifests, reports, schemas, receipts, notes, and the catalog remain ordinary Git text. Do not run broad tracking commands for all `Content` or all source files. Ask for review of the exact narrow rules first.

## Common Problems

**The model is missing:** Check that `source.model` matches the exact file inside `model/`, including its extension.

**The scale is wrong:** Validate first, then adjust `presentation.scale`. Reimport after reviewing the changed settings digest. Do not rescale gameplay coordinates.

**Textures are missing:** Put the images inside the bundle, declare their exact paths and roles, and decide whether each is required. Inspect the model's own relative texture references too.

**A skeleton does not match:** Use `skeleton_policy: "create"` for an independent character skeleton, or provide an exact compatible Unreal skeleton package for `reuse`. This pass never silently retargets.

**A GLB imports as the wrong class:** Export a conforming GLB with the intended skin/mesh data or use FBX. The report rejects a primary object whose Unreal class does not match `model_type`.

**A preview was not generated:** `-NullRHI` cannot render. The import remains valid, and `PreviewRequest.json` explains the status. Run preview generation in a render-capable editor environment.

**Git/LFS says untracked:** The pipeline does not change source control. Send `ImportReport.md`, `SourceInventory.json`, and the preview images for review before adding narrow LFS rules.

## Safety Guarantees

- Character IDs and public definition IDs use strict syntax.
- Source paths must be relative and remain inside the bundle.
- Absolute paths, drive paths, remote URLs, traversal, executable extensions, and invalid Unreal package names are rejected.
- The pipeline never executes bundle files or invokes converters.
- It never downloads assets or changes plugins.
- Generated reports use repository-relative source paths.
- Private gameplay, concealed marker, credential, deck, hand, and hidden-zone fields are rejected.
- Git commands are read-only.
- Import destinations are character-scoped and deterministic.
- Raw source files are excluded from cook package lists.
- Editor, redirector-like, Godot, and Meshy package paths fail cook-list validation.
- The editor importer has no runtime dependency from WandboundCore or WandboundRuntime.
- The starter presentation asset is not modified automatically.
- Gameplay rules, state, replay traces, action order, and CardDB definitions are untouched.
