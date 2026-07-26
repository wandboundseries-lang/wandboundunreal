[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[a-z0-9]+(?:_[a-z0-9]+)*$')]
    [string]$CharacterId,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$DisplayName,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Z][A-Z0-9_]*$')]
    [string]$CardDefinitionId,

    [string]$ProjectPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try {
    if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
        $ProjectPath = Join-Path (Split-Path $PSScriptRoot -Parent) '..\WandboundUE.uproject'
    }
    $projectFile = [System.IO.Path]::GetFullPath($ProjectPath)
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
        throw "Wandbound project file was not found: $projectFile"
    }

    $projectRoot = Split-Path $projectFile -Parent
    $bundleRoot = Join-Path $projectRoot "SourceAssets\Characters\$CharacterId"
    if (Test-Path -LiteralPath $bundleRoot) {
        throw "Character bundle already exists and will not be overwritten: $bundleRoot"
    }

    foreach ($directory in @('model', 'textures', 'animations', 'previews', 'notes')) {
        [System.IO.Directory]::CreateDirectory((Join-Path $bundleRoot $directory)) | Out-Null
    }

    $manifest = [ordered]@{
        schema_version = 1
        character_id = $CharacterId
        display_name = $DisplayName
        card_definition_id = $CardDefinitionId
        approval = [ordered]@{
            approved_for_import = $false
            approved_by = ''
            approval_note = 'Review the source files, then approve this exact bundle.'
        }
        source = [ordered]@{
            model = "model/$CharacterId.fbx"
            model_format = 'fbx'
            model_type = 'skeletal'
            textures_directory = 'textures'
            animations_directory = 'animations'
            textures = @()
            animations = @()
        }
        presentation = [ordered]@{
            role = 'player_unit'
            scale = 1.0
            rotation = @(0.0, 0.0, 0.0)
            offset = @(0.0, 0.0, 0.0)
            facing_axis = 'positive_x'
        }
        import = [ordered]@{
            import_materials = $true
            import_textures = $true
            import_animations = $false
            create_physics_asset = $true
            generate_collision = $false
            skeleton_policy = 'create'
            normal_policy = 'import_normals_and_tangents'
        }
        previews = [ordered]@{}
        tags = @()
        notes = ''
    }

    $manifestPath = Join-Path $bundleRoot 'character_manifest.json'
    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        $manifestPath,
        (($manifest | ConvertTo-Json -Depth 12) + [Environment]::NewLine),
        $utf8WithoutBom)

    $readme = @"
# $DisplayName character bundle

1. Put one `.fbx`, `.glb`, or `.gltf` model in `model/`.
2. Update `source.model`, `source.model_format`, and `source.model_type` in `character_manifest.json`.
3. Put optional images in `textures/`, FBX clips in `animations/`, and review images in `previews/`.
4. Declare each texture and animation in the manifest.
5. After reviewing the exact files, set `approval.approved_for_import` to `true` and fill `approved_by`.
6. Validate from the project root with:

   ````powershell
   .\Scripts\Assets\ImportWandboundCharacter.ps1 -CharacterId "$CharacterId"
   ````

This generator creates no model and never stages files.
"@
    [System.IO.File]::WriteAllText(
        (Join-Path $bundleRoot 'notes\README.md'),
        ($readme + [Environment]::NewLine),
        $utf8WithoutBom)

    Write-Host "Created character bundle: $bundleRoot"
    Write-Host 'No model was created. Add your source files, edit the manifest, then validate.'
    exit 0
}
catch {
    Write-Error $_
    exit 1
}
