[CmdletBinding()]
param(
	[string]$ProjectPath,
	[string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.7"
)

$ErrorActionPreference = "Stop"
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
	$ProjectPath = Join-Path $projectRoot "WandboundUE.uproject"
}
$ProjectPath = [System.IO.Path]::GetFullPath($ProjectPath)
$editorCommandlet = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$assetFile = Join-Path $projectRoot "Content\Wandbound\Presentation\DA_WandboundStarterPresentation.uasset"
$logDirectory = Join-Path $projectRoot "Saved\Logs"
$logPath = Join-Path $logDirectory "GenerateWandboundStarterPresentationAssets.log"

if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) {
	throw "Project file not found: $ProjectPath"
}
if (-not (Test-Path -LiteralPath $editorCommandlet -PathType Leaf)) {
	throw "UnrealEditor-Cmd.exe not found: $editorCommandlet"
}
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

$arguments = @(
	$ProjectPath,
	"-run=WBStarterPresentationAssetGenerator",
	"-unattended",
	"-nop4",
	"-nosplash",
	"-NullRHI",
	"-log=$logPath",
	"-stdout",
	"-FullStdOutLogOutput"
)
& $editorCommandlet @arguments
$commandletExitCode = $LASTEXITCODE
if ($commandletExitCode -ne 0) {
	throw "Starter presentation generator failed with exit code $commandletExitCode. Log: $logPath"
}
if (-not (Test-Path -LiteralPath $assetFile -PathType Leaf)) {
	throw "Starter presentation asset was not generated: $assetFile"
}

$asset = Get-Item -LiteralPath $assetFile
$hash = Get-FileHash -LiteralPath $assetFile -Algorithm SHA256
$relativeAsset = "Content/Wandbound/Presentation/DA_WandboundStarterPresentation.uasset"
$trackedFiles = @(git -C $projectRoot ls-files -- $relativeAsset)
$tracked = $trackedFiles -contains $relativeAsset
$attributes = git -C $projectRoot check-attr filter diff merge text -- $relativeAsset

Write-Output "Starter presentation asset generated and validated."
Write-Output "Asset: $assetFile"
Write-Output "Bytes: $($asset.Length)"
Write-Output "SHA256: $($hash.Hash)"
Write-Output "Git tracked: $tracked"
Write-Output "Attributes:"
$attributes | ForEach-Object { Write-Output "  $_" }
Write-Output "Generator log: $logPath"
