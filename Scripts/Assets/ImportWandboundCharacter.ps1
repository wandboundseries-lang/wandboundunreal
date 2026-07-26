[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[a-z0-9]+(?:_[a-z0-9]+)*$')]
    [string]$CharacterId,

    [ValidateSet('Validate', 'DryRun', 'Import', 'Reimport')]
    [string]$Mode = 'Validate',

    [switch]$GeneratePreview,
    [switch]$ValidateCook,

    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
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
    $editorCommand = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
        throw "UnrealEditor-Cmd.exe was not found under EngineRoot: $EngineRoot"
    }

    $manifestRelative = "SourceAssets/Characters/$CharacterId/character_manifest.json"
    $manifestAbsolute = Join-Path $projectRoot ($manifestRelative.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $manifestAbsolute -PathType Leaf)) {
        throw "Character bundle manifest was not found: $manifestRelative"
    }

    $logDirectory = Join-Path $projectRoot 'Saved\Logs\CharacterImports'
    [System.IO.Directory]::CreateDirectory($logDirectory) | Out-Null
    $logPath = Join-Path $logDirectory "${CharacterId}_$($Mode.ToLowerInvariant()).log"

    $commandletArguments = @(
        $projectFile
        '-run=WBCharacterModelImport'
        "-Manifest=$manifestRelative"
        "-Mode=$Mode"
        '-unattended'
        '-nop4'
        '-nosplash'
        "-abslog=$logPath"
    )
    if ($GeneratePreview) {
        $commandletArguments += '-GeneratePreview'
    }
    if ($ValidateCook) {
        $commandletArguments += '-ValidateCook'
    }

    Write-Host "Wandbound character $Mode started for '$CharacterId'."
    & $editorCommand @commandletArguments
    $exitCode = $LASTEXITCODE

    $reportRoot = Join-Path $projectRoot "Docs\AssetImports\$CharacterId"
    Write-Host "Log: $logPath"
    if (Test-Path -LiteralPath $reportRoot -PathType Container) {
        Write-Host "Reports: $reportRoot"
        Get-ChildItem -LiteralPath $reportRoot -File |
            Sort-Object Name |
            ForEach-Object { Write-Host "  $($_.Name)" }
    }
    Write-Host 'Review SourceInventory.json and ImportReport.md for Git/LFS warnings before source control.'

    if ($exitCode -ne 0) {
        throw "Character pipeline failed with exit code $exitCode. See $logPath"
    }
    Write-Host "Wandbound character $Mode completed successfully."
    exit 0
}
catch {
    Write-Error $_
    exit 1
}
