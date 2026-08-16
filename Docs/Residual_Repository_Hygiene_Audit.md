# Residual Repository Hygiene Audit

Date: 2026-08-15

## Baseline

- Branch: `main`
- `HEAD`: `fe346cd803b371e0aebcda3b14a132706bf00d0b`
- `origin/main`: `fe346cd803b371e0aebcda3b14a132706bf00d0b`
- Baseline commit: `Add Wandbound 3D card models`
- Repository visibility supplied by the owner: public
- Staged files before and after remediation: 0

## Android FileServer Owner Decision

The owner selected **disable Android FileServer for now**. This decision applies only to Unreal Engine's auxiliary Android file-transfer service. It does not disable Android platform support or alter gameplay, replay, save, CardDB, content, packaging identity, SDK, signing, or unrelated Android settings.

The historical `SecurityToken` value was nonblank authentication material committed in a public repository. The value is intentionally omitted from this audit and its JSON companion. It first appeared in commit `cf4f6849e24da4f2e7f10435090fe5ca34b9c61c` and must be treated as exposed.

## Verified UE 5.7 Disable Semantics

Installed Unreal Engine 5.7 source establishes the following behavior for `/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings`:

1. `bEnablePlugin=False` sets the Android packaging layer's authoritative `bEnabled` value to false.
2. Disabled packaging sets both embedded AFS and standalone AFS project creation paths to false.
3. The Android manifest receives no AFS service, activity, permissions, or network metadata.
4. Embedded AFS Java sources are removed from generated Android sources, and AFS runtime methods are generated as false-returning stubs.
5. Consequently neither USB nor network access through Android FileServer can start.
6. A blank `SecurityToken` cannot create an unauthenticated service because the service, generated components, and start paths are disabled.
7. Leaving the key absent is less deterministic: the settings constructor uses `[AUTO]`, and `PostInitProperties` generates a GUID and attempts to update `DefaultEngine.ini`. An explicit blank value prevents that regeneration.
8. Android platform support and non-AFS runtime systems remain unaffected because the setting gates only the AndroidFileServer plugin's packaging additions.

## Security Configuration Remediation

Before editing, `Config/DefaultEngine.ini` was copied outside the repository to `%LOCALAPPDATA%\Temp\WandboundAndroidSecurity_20260815_192304\`.

Only these keys changed in the Android FileServer settings section:

| Key | Prior state | Current state |
| --- | --- | --- |
| `bEnablePlugin` | enabled | disabled |
| `bAllowNetworkConnection` | enabled | disabled |
| `SecurityToken` | nonblank | blank/non-secret |

Redacted validation confirmed the old credential is absent, no new credential or `[AUTO]` sentinel was generated, and all unrelated `DefaultEngine.ini` content is exact after normalizing those three expected lines.

## Historical Exposure and Revocation

Disabling the service and removing its reusable credential prevents future builds from activating that credential through Android FileServer. Already-distributed builds cannot be changed retroactively, and Git history still contains the old value. History rewriting is not required for practical revocation in future builds because the service is disabled and the value is no longer active there. A coordinated history purge may still be considered later if repository policy requires reducing historical discoverability.

If Android FileServer is re-enabled later, it requires a newly generated private/local credential workflow that does not commit reusable authentication material. The historical value must not be reused.

## DefaultEditor Cleanup

The 55,531-byte generated Advanced Preview Scene profile dump was copied to the same external backup directory. `Config/DefaultEditor.ini` was then written to the exact committed `HEAD` representation, which is a zero-byte file. It is now clean relative to `HEAD`.

Classification: `LOCAL_EDITOR_STATE`. No ignore-policy or index change was made.

## Local Transcript Cleanup

The untracked file named exactly `h origin main` was a 5,245-byte UTF-8 local command transcript, not source, config, content, or an Unreal asset. No production reference was found. It was copied to the external backup directory and only that exact file was deleted under the owner's explicit authorization.

Classification: `LOCAL_EDITOR_STATE`; backup complete; deletion complete.

## Historical Audits

`Docs/CardDB_Unreal_Importer_Manifest_Suite_Audit.md` remains labeled **Historical / Superseded** and points to `Docs/CardDB_Unreal_Importer_Manifest_Suite_Report.md` and commit `7625719c2c02a57e998a872d9f369d4078e94f8f`.

`Docs/Production_Activation_Target_Selection_Bridge_Audit.md` remains labeled **Historical / Superseded** and points to `Docs/Production_Activation_Target_Selection_Bridge_Report.md` and commit `51b04876120a102dc7028c6390d003c3a1e46cae`.

Their historical bodies remain unchanged and are not current implementation or rules authority.

## Intentional Untracked Assets

`Content/Maps/NewProjectTest.umap` remains untracked and unchanged. Classification: `LEGACY_DEV_ASSET`. It is not selected by startup, packaging, smoke, runtime, or automation paths. Manual Unreal Editor inspection remains optional before any later archival or deletion decision.

`Plugins/meshy/Content/Materials/M_MeshyPBR.uasset` remains untracked and unchanged. Classification: `MESHY_PLUGIN_VENDOR_UNRESOLVED`. No local redistribution grant or sufficient provenance was found, so it remains outside source control and has no new LFS rule.

## LFS and Gameplay Integrity

All 58 `Content/MeshyImports/**` Wandbound card-model assets remain tracked through Git LFS and clean. No files changed under gameplay source, CardDB data, replay data, tests, or the committed card-model directory. No Unreal binary asset was modified.

## Final Classification

| Path | Classification | Final state | Commit disposition |
| --- | --- | --- | --- |
| `Config/DefaultEngine.ini` | `SECURITY_REMEDIATION` | tracked, modified | `SECURITY_CONFIG` |
| `Config/DefaultEditor.ini` | `LOCAL_EDITOR_STATE` | tracked, clean | none |
| `Docs/CardDB_Unreal_Importer_Manifest_Suite_Audit.md` | `HISTORICAL_DOC` | untracked | `DOCUMENTATION_HYGIENE` |
| `Docs/Production_Activation_Target_Selection_Bridge_Audit.md` | `HISTORICAL_DOC` | untracked | `DOCUMENTATION_HYGIENE` |
| `Docs/Residual_Repository_Hygiene_Audit.md` | `AUDIT_DOC` | untracked | `DOCUMENTATION_HYGIENE` |
| `Docs/Residual_Repository_Hygiene_Audit.json` | `AUDIT_DOC` | untracked | `DOCUMENTATION_HYGIENE` |
| `Content/Maps/NewProjectTest.umap` | `LEGACY_DEV_ASSET` | untracked, unchanged | excluded |
| `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset` | `MESHY_PLUGIN_VENDOR_UNRESOLVED` | untracked, unchanged | excluded |
| `h origin main` | `LOCAL_EDITOR_STATE` | externally backed up, removed | none |

Unknown path count: 0.

## Commit Manifests

### SECURITY_CONFIG

- `Config/DefaultEngine.ini`

The tracked file was clean before this pass and contains only the three redacted Android FileServer changes listed above. It is whole-file safe to stage as a dedicated future commit after review.

### DOCUMENTATION_HYGIENE

- `Docs/CardDB_Unreal_Importer_Manifest_Suite_Audit.md`
- `Docs/Production_Activation_Target_Selection_Bridge_Audit.md`
- `Docs/Residual_Repository_Hygiene_Audit.md`
- `Docs/Residual_Repository_Hygiene_Audit.json`

These four untracked documentation files are whole-file safe and contain no credential value. No other path belongs in either commit.

## Validation Boundary

This pass performed configuration and documentation validation only, as directed. It did not run the full gameplay automation suite. No file was staged, committed, pushed, reset, restored, checked out, cleaned, or migrated.
