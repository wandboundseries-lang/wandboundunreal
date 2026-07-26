# Wandbound Character Import Report

- Character: `pipeline_validation_fixture` (Pipeline Validation Fixture)
- Public definition: `CHAR_PIPELINE_VALIDATION_FIXTURE`
- Approved: `yes` by `automation`
- Result: `success` (`validation_succeeded`)
- Source: `model/pipeline_validation_fixture.fbx` (fbx, static)
- Inventory hash: `cd98e2d3ee32e6f1b57d967ca9022ef6d7f9be258f6f58160cca5fdb3dc3ef27`
- Reimport state: `NeverImported`
- Destination: `/Game/Wandbound/Characters/pipeline_validation_fixture`
- Primary mesh: `/Game/Wandbound/Characters/pipeline_validation_fixture/Meshes/SM_pipeline_validation_fixture`
- Skeleton: ``
- Physics asset: ``
- Transform: scale 1, rotation (0, 0, 0), offset (0, 0, 0)
- Preview: optional; NullRHI produces a structured request instead of failing import
- Presentation: candidate only; the starter asset is not modified automatically
- Fallback: missing animation roles retain Wandbound transform/pulse fallbacks

## Source Inventory

| Path | Role | Bytes | SHA-256 | Git/LFS |
| --- | --- | ---: | --- | --- |
| `character_manifest.json` | `manifest` | 1250 | `a50a5177d7de59284f655bbdb18c1ded39aff5be0c4f826d595772be7e21d3d6` | `Untracked` |
| `model/pipeline_validation_fixture.fbx` | `primary_model` | 105 | `fa5fcdfc56d4f48ef6a860c858ba2ef96c8d1d25f5c58b45d956dc76452655ec` | `Untracked` |
| `notes/README.md` | `notes` | 285 | `84aacd03b938a6d47a1b587839d51518abd6f86b9be1c634e00556d6459213ca` | `Untracked` |

## Diagnostics

- **warning** `card_definition_repository_unavailable`: No production CardDB repository is loaded; the public definition ID is syntax-checked only. Validate the mapping again when a production CardDB bundle is available.
- **warning** `production_ready_blocked_binary_not_lfs`: A required binary source file is not tracked through Git LFS. Apply the narrow reported LFS rule before production approval.

## Git LFS Recommendations

`SourceAssets/Characters/pipeline_validation_fixture/model/* filter=lfs diff=lfs merge=lfs -text`

`SourceAssets/Characters/pipeline_validation_fixture/textures/* filter=lfs diff=lfs merge=lfs -text`

`SourceAssets/Characters/pipeline_validation_fixture/animations/* filter=lfs diff=lfs merge=lfs -text`

`SourceAssets/Characters/pipeline_validation_fixture/previews/* filter=lfs diff=lfs merge=lfs -text`

`Content/Wandbound/Characters/pipeline_validation_fixture/**/*.uasset filter=lfs diff=lfs merge=lfs -text`
