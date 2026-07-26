[CmdletBinding()]
param(
	[string]$ProjectPath,
	[string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.7",
	[string]$ArchiveDirectory,
	[string]$StagingDirectory,
	[switch]$CleanBuild,
	[switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($ProjectPath)) { $ProjectPath = Join-Path $projectRoot "WandboundUE.uproject" }
if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) { $ArchiveDirectory = Join-Path $projectRoot "Saved\PackagedBuilds\WandboundLocalPlay" }
if ([string]::IsNullOrWhiteSpace($StagingDirectory)) { $StagingDirectory = Join-Path $projectRoot "Saved\StagedBuilds\WandboundLocalPlay" }
$ProjectPath = [System.IO.Path]::GetFullPath($ProjectPath)
$ArchiveDirectory = [System.IO.Path]::GetFullPath($ArchiveDirectory)
$StagingDirectory = [System.IO.Path]::GetFullPath($StagingDirectory)
$uat = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) { throw "Project file not found: $ProjectPath" }
if (-not (Test-Path -LiteralPath $uat -PathType Leaf)) { throw "RunUAT.bat not found: $uat" }
if ($ValidateOnly) {
	Write-Output "Package inputs valid: project=$ProjectPath engine=$EngineRoot"
	return
}

New-Item -ItemType Directory -Force -Path $ArchiveDirectory, $StagingDirectory | Out-Null
$logDirectory = Join-Path $projectRoot "Saved\Logs"
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$logPath = Join-Path $logDirectory "PackageWandboundLocalPlay.log"
$uatArgs = @(
	"BuildCookRun",
	"-project=$ProjectPath",
	"-targetplatform=Win64",
	"-clientconfig=Development",
	"-build",
	"-cook",
	"-stage",
	"-package",
	"-archive",
	"-archivedirectory=$ArchiveDirectory",
	"-stagingdirectory=$StagingDirectory",
	"-map=/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev",
	"-AdditionalCookerOptions=-DisablePlugins=DatasmithFBXImporter,DatasmithContent -Package=/Game/Wandbound/Presentation/DA_WandboundStarterPresentation",
	"-pak",
	"-SkipCookingEditorContent",
	"-unattended",
	"-utf8output"
)
if ($CleanBuild) { $uatArgs += "-clean" }

& $uat @uatArgs 2>&1 | Tee-Object -FilePath $logPath
$uatExitCode = $LASTEXITCODE
if ($uatExitCode -ne 0) { throw "BuildCookRun failed with exit code $uatExitCode. Log: $logPath" }

$executable = Get-ChildItem -LiteralPath $ArchiveDirectory -Recurse -File -Filter "WandboundUE.exe" |
	Sort-Object { $_.FullName.Length } |
	Select-Object -First 1
if ($null -eq $executable) { throw "Packaged executable was not found under: $ArchiveDirectory" }
Write-Output "Package archive: $ArchiveDirectory"
Write-Output "Package log: $logPath"
Write-Output "Packaged executable: $($executable.FullName)"
