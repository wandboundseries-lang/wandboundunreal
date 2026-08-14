# Asset Source-Control Audit

Date: 2026-08-14

## Verified Baseline

- `HEAD`: `8f6359ecaf8c41e701ce20af6161dc2640da35ed`
- `origin/main`: `8f6359ecaf8c41e701ce20af6161dc2640da35ed`
- Branch: `main`
- Staged paths at audit start: none
- Git LFS: `git-lfs/3.7.1`, functional

## Ownership Clarification

`Content/MeshyImports/**` contains intentional production 3D models for Wandbound cards. It is not generated junk and must not be deleted, ignored, moved, renamed, reimported, or modified by repository cleanup. The directory is assigned to `WANDBOUND_3D_CARD_MODELS` and prepared for Git LFS without staging any asset.

`Plugins/meshy/Content/**` is a separate ownership boundary. Its single material belongs to the Meshy editor plugin and is not one of the card models.

## 3D Card Model Inventory

- Files: 58
- Bytes: 1,058,484,438 (1.058 GB decimal; approximately 1,009.45 MiB)
- `.uasset`: 35 files, 536,968,352 bytes
- `.fbx`: 6 files, 358,343,368 bytes
- `.jpg`: 17 files, 163,172,718 bytes
- `.umap`: 0
- Other source interchange/image formats: none found

Directory summary:

| Directory | Files | Bytes |
| --- | ---: | ---: |
| `Content/MeshyImports` | 6 | 358,343,368 |
| `Content/MeshyImports/Arcane Gentleman_Import0003.fbm` | 3 | 27,367,461 |
| `Content/MeshyImports/Gilded Gambler_Import0002.fbm` | 3 | 28,269,880 |
| `Content/MeshyImports/Golden Tome Keeper_Import0002.fbm` | 3 | 28,035,105 |
| `Content/MeshyImports/Import_20260706_194044` | 7 | 141,493,579 |
| `Content/MeshyImports/Import_20260706_194452` | 7 | 97,597,736 |
| `Content/MeshyImports/Import_20260706_194947` | 7 | 89,971,907 |
| `Content/MeshyImports/Import_20260706_195821` | 7 | 104,491,795 |
| `Content/MeshyImports/Import_20260706_195903` | 7 | 103,413,335 |
| `Content/MeshyImports/Meshy_Model_Import0004.fbm` | 3 | 32,018,561 |
| `Content/MeshyImports/Pink Velvet Harlequin_Import0001.fbm` | 2 | 16,174,110 |
| `Content/MeshyImports/Stonebound Warrior_Import0005.fbm` | 3 | 31,307,601 |

Largest 20 files:

| Path | Bytes |
| --- | ---: |
| `Content/MeshyImports/Pink Velvet Harlequin_Import0001.fbx` | 93,359,180 |
| `Content/MeshyImports/Import_20260706_194044/Pink_Velvet_Harlequin_Import0001.uasset` | 70,091,212 |
| `Content/MeshyImports/Meshy_Model_Import0004.fbx` | 58,923,916 |
| `Content/MeshyImports/Stonebound Warrior_Import0005.fbx` | 58,718,556 |
| `Content/MeshyImports/Golden Tome Keeper_Import0002.fbx` | 56,296,764 |
| `Content/MeshyImports/Arcane Gentleman_Import0003.fbx` | 47,899,260 |
| `Content/MeshyImports/Gilded Gambler_Import0002.fbx` | 43,145,692 |
| `Content/MeshyImports/Import_20260706_195903/Stonebound_Warrior_Import0005_Normal.uasset` | 35,182,380 |
| `Content/MeshyImports/Import_20260706_195821/Meshy_Model_Import0004_BaseColor.uasset` | 35,051,236 |
| `Content/MeshyImports/Import_20260706_195903/Stonebound_Warrior_Import0005_BaseColor.uasset` | 34,866,875 |
| `Content/MeshyImports/Import_20260706_195821/Meshy_Model_Import0004_Normal.uasset` | 34,794,538 |
| `Content/MeshyImports/Import_20260706_194044/Pink_Velvet_Harlequin_Import0001_Normal.uasset` | 34,374,093 |
| `Content/MeshyImports/Import_20260706_194044/Pink_Velvet_Harlequin_Import0001_BaseColor.uasset` | 34,197,850 |
| `Content/MeshyImports/Import_20260706_194452/Golden_Tome_Keeper_Import0002_Normal.uasset` | 34,020,719 |
| `Content/MeshyImports/Import_20260706_194947/Arcane_Gentleman_Import0003_Normal.uasset` | 32,954,536 |
| `Content/MeshyImports/Import_20260706_194947/Arcane_Gentleman_Import0003_BaseColor.uasset` | 31,484,027 |
| `Content/MeshyImports/Import_20260706_194452/Golden_Tome_Keeper_Import0002_BaseColor.uasset` | 31,322,714 |
| `Content/MeshyImports/Import_20260706_195903/Stonebound_Warrior_Import0005.uasset` | 30,989,695 |
| `Content/MeshyImports/Import_20260706_195821/Meshy_Model_Import0004.uasset` | 30,793,583 |
| `Content/MeshyImports/Import_20260706_194452/Golden_Tome_Keeper_Import0002.uasset` | 30,748,900 |

Text-source inspection found the Meshy plugin's default import destination `/Game/MeshyImports`. Binary dependency relationships were not inferred by parsing `.uasset` payloads; no Unreal binary was opened or changed by this audit.

## DefaultEditor.ini

`HEAD` contains an empty file. The working copy contains the `[/Script/AdvancedPreviewScene.SharedProfiles]` section and three engine-default shared profiles: `Epic Headquarters`, `Grey Wireframe`, and `Grey Ambient`.

Unreal Engine source identifies `USharedProfiles` as `config=Editor, defaultconfig`; `UAssetViewerSettings::Save` writes shared profiles through `TryUpdateDefaultConfigFile`. These settings affect editor asset-preview lighting, floors, grids, environments, and post-processing. They do not control gameplay, packaging, runtime maps, or production rendering.

Classification: `GENERATED_EDITOR_STATE`, represented in the final tree as `LOCAL_EDITOR_CONFIG`. No repository source refers to these profile keys, and no Meshy-specific profile exists. The likely relationship to import work is only that opening an asset preview caused Unreal Editor to serialize engine defaults. Recommendation: leave the file uncommitted and decide separately whether the team intentionally wants shared preview profiles.

## Android File Server Security

- Section: `/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings`
- Key: `SecurityToken`
- Working-copy value state: nonblank
- `HEAD` value state: nonblank
- First key-introducing commit: `cf4f6849e24da4f2e7f10435090fe5ca34b9c61c` (`Add Unreal project setup and WandboundCore baseline`)
- Token value: intentionally omitted everywhere in this audit

Unreal Engine 5.7 defines the settings class as `Config=Engine, DefaultConfig`, exposes the key as an optional security token required to start Android FileServer, initializes it to `[AUTO]`, generates a GUID on first initialization, and writes that generated value to default config. Packaging reads the value and uses it to verify Android FileServer access; a blank value disables token checking.

This is therefore generated project configuration with credential semantics, not harmless editor decoration and not inherently machine-bound. The committed value should be treated as exposed to every repository reader. Remediation belongs in a separate `SECURITY_CONFIG_REMEDIATION` change: determine whether Android FileServer is needed; rotate/regenerate the current credential; remove the concrete value from committed defaults or inject it through a non-versioned/CI-controlled config path; and only then assess history cleanup based on repository exposure and secret-scanning results. Do not blank it without also reviewing whether the plugin/network path remains enabled, because blank explicitly disables verification.

## Stale Documents

### CardDB Unreal Importer Manifest Suite Audit

`Docs/CardDB_Unreal_Importer_Manifest_Suite_Audit.md` describes the pre-implementation design for test-only manifest-suite aggregation. Commit `7625719` implemented that suite and added `Docs/CardDB_Unreal_Importer_Manifest_Suite_Report.md`, fixtures, helpers, and tests. Statements framed as proposed/future suite work are obsolete, while the boundary and rationale retain historical/debug value. No current source or test links to the audit filename.

Classification: `KEEP_AS_HISTORICAL` / `HISTORICAL_DOC`. Recommendation: add a superseded header pointing to the implementation report before a separate documentation commit; do not present it as current planning.

### Production Activation Target Selection Bridge Audit

`Docs/Production_Activation_Target_Selection_Bridge_Audit.md` describes the planned runtime bridge. Commit `51b0487` implemented `FWBProductionActivationTargetSelectionBridge`, its tests, and `Docs/Production_Activation_Target_Selection_Bridge_Report.md`; later execution-handoff work consumes it. Proposed/future language is obsolete, but the design rationale remains useful. No current source or test links to the audit filename.

Classification: `KEEP_AS_HISTORICAL` / `HISTORICAL_DOC`. Recommendation: add a superseded header pointing to the implementation report before a separate documentation commit.

## Map Classification

`Content/Maps/NewProjectTest.umap` is an untracked 20,614-byte Unreal map. It has no text references in project config, startup maps, runtime code, automation, scripts, or documentation other than the prior hygiene inventory. `Config/DefaultEngine.ini` still names the engine OpenWorld template as `GameDefaultMap`. The tracked and explicitly packaged/smoke-tested development map is `Content/Wandbound/Maps/Wandbound_LocalPlay_Dev.umap`.

Classification: `DEV_TEST_MAP` / `INTENTIONAL_MAP`. There is insufficient evidence to call it a Meshy import test or production map. Keep it untracked until the user confirms its purpose. If retained, give it its own exact LFS rule and separate commit; it is deliberately outside the new MeshyImports rule.

## Meshy Plugin Content

The plugin has 13 tracked files: its descriptor, editor module source/build files, two resource icons, and one source icon. The descriptor sets `CanContainContent=true` and `Installed=true`.

The only untracked plugin-content file is `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset` (9,461 bytes). Tracked `MeshyBridge.cpp` hard-codes `/meshy/Materials/M_MeshyPBR.M_MeshyPBR` and loads it when building channel-correct imported materials. If absent, the plugin logs a warning and retains imported materials, so the plugin starts but loses its intended PBR material reconstruction path. No license or redistribution terms were found in the vendored plugin tree.

Classification: `MESHY_PLUGIN_VENDOR_CONTENT`. Keep it untouched and untracked until plugin ownership/redistribution is confirmed. If this installed plugin is intentionally vendored, commit the material with the plugin in a separate plugin-content commit and add an exact LFS rule if desired; otherwise restore it through the documented plugin dependency/install mechanism.

## Git LFS Policy

Before this pass, only these exact paths used LFS:

- `Content/Wandbound/Maps/Wandbound_LocalPlay_Dev.umap`
- `Content/Wandbound/Presentation/DA_WandboundStarterPresentation.uasset`

This pass adds the narrow rule:

```gitattributes
Content/MeshyImports/** filter=lfs diff=lfs merge=lfs -text
```

The directory rule covers Unreal assets, FBX sources, and textures under the intentional card-model hierarchy without changing repository-global `.uasset`, `.umap`, or image policy. `Content/Maps/NewProjectTest.umap` and `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset` remain outside the rule.

The expected first LFS upload is approximately 1,058,484,438 bytes. GitHub storage and bandwidth quotas depend on the account/repository plan and were not inferred here.

Representative `git check-attr -a` results:

| Path | filter | diff | merge | text |
| --- | --- | --- | --- | --- |
| `Content/MeshyImports/Pink Velvet Harlequin_Import0001.fbx` | `lfs` | `lfs` | `lfs` | unset |
| `Content/MeshyImports/Import_20260706_194044/Pink_Velvet_Harlequin_Import0001.uasset` | `lfs` | `lfs` | `lfs` | unset |
| `Content/MeshyImports/Arcane Gentleman_Import0003.fbm/Image_0.jpg` | `lfs` | `lfs` | `lfs` | unset |
| `Content/Maps/NewProjectTest.umap` | unspecified | unspecified | unspecified | unspecified |
| `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset` | unspecified | unspecified | unspecified | unspecified |

The two pre-existing exact LFS paths retain `filter=lfs`, `diff=lfs`, `merge=lfs`, and unset text. A path-aware `git hash-object --path` clean-filter probe generated a valid three-line LFS pointer whose filtered Git hash differed from the raw input hash. The probe used only in-memory input and Git plumbing: no probe file, index entry, real-asset modification, staging, upload, or history migration occurred.

## Recommended Commit Groups

1. LFS configuration and audit:
   - `.gitattributes`
   - `Docs/Asset_Source_Control_Audit.md`
   - `Docs/Asset_Source_Control_Audit.json`
2. Wandbound 3D card models, only after confirming GitHub LFS capacity:
   - `Content/MeshyImports/**` (58 files)
3. Map, only after user confirms intent and an exact map LFS rule is added:
   - `Content/Maps/NewProjectTest.umap`
4. Meshy plugin support content, only after plugin ownership/redistribution is confirmed:
   - `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`
5. Historical docs, after adding superseded headers:
   - `Docs/CardDB_Unreal_Importer_Manifest_Suite_Audit.md`
   - `Docs/Production_Activation_Target_Selection_Bridge_Audit.md`
6. Security remediation, separately and without exposing the credential:
   - `Config/DefaultEngine.ini` or an approved non-versioned/CI configuration mechanism

Do not include `Config/DefaultEditor.ini` in these commits unless the team explicitly adopts shared asset-preview profiles.

## Final Classification

| Path/group | Classification | Tracking | Size | LFS after pass | Disposition |
| --- | --- | --- | ---: | --- | --- |
| `.gitattributes` | `LFS_CONFIGURATION` | tracked, modified | text | no | first hygiene commit |
| `Config/DefaultEditor.ini` | `LOCAL_EDITOR_CONFIG` | tracked, modified | 55,531 bytes | no | leave uncommitted |
| `Config/DefaultEngine.ini` token finding | `SECURITY_CONFIG_REMEDIATION` | tracked, clean | n/a | no | separate remediation; do not expose value |
| `Content/MeshyImports/**` | `WANDBOUND_3D_CARD_MODELS` | untracked | 1,058,484,438 bytes | yes | separate LFS asset commit |
| `Content/Maps/NewProjectTest.umap` | `INTENTIONAL_MAP` | untracked | 20,614 bytes | no | user intent decision; separate commit if retained |
| `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset` | `MESHY_PLUGIN_VENDOR_CONTENT` | untracked | 9,461 bytes | no | ownership/license decision; separate commit if vendored |
| two superseded audit docs | `HISTORICAL_DOC` | untracked | 9,498 bytes total | no | add superseded headers, then separate doc commit |
| `Docs/Asset_Source_Control_Audit.md` and `.json` | `AUDIT_DOC` | untracked | text | no | first hygiene commit |

Unknown classifications: 0.

No gameplay source/data file was modified. No file was staged, committed, pushed, deleted, migrated, or uploaded. No Unreal binary was changed.
