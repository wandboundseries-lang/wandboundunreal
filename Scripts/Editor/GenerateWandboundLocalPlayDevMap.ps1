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
$editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) { throw "Project file not found: $ProjectPath" }
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "UnrealEditor-Cmd.exe not found: $editor" }

& $editor $ProjectPath -run=WandboundLocalPlayMapGenerator -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput
if ($LASTEXITCODE -ne 0) { throw "Development map generation failed with exit code $LASTEXITCODE" }

$mapFile = Join-Path $projectRoot "Content\Wandbound\Maps\Wandbound_LocalPlay_Dev.umap"
if (-not (Test-Path -LiteralPath $mapFile -PathType Leaf)) { throw "Generated map was not found: $mapFile" }
Write-Output "Development map ready: $mapFile"
