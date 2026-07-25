[CmdletBinding()]
param(
	[string]$PackageDirectory,
	[string]$ExecutablePath,
	[int]$TimeoutSeconds = 180,
	[switch]$DisableNullRHI,
	[switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
	$PackageDirectory = Join-Path $projectRoot "Saved\PackagedBuilds\WandboundLocalPlay"
}
$PackageDirectory = [System.IO.Path]::GetFullPath($PackageDirectory)
if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
	if (-not (Test-Path -LiteralPath $PackageDirectory -PathType Container)) { throw "Package directory not found: $PackageDirectory" }
	$executable = Get-ChildItem -LiteralPath $PackageDirectory -Recurse -File -Filter "WandboundUE.exe" |
		Sort-Object { $_.FullName.Length } |
		Select-Object -First 1
	if ($null -eq $executable) { throw "Packaged executable not found under: $PackageDirectory" }
	$ExecutablePath = $executable.FullName
}
$ExecutablePath = [System.IO.Path]::GetFullPath($ExecutablePath)
if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw "Packaged executable not found: $ExecutablePath" }
if ($ValidateOnly) {
	Write-Output "Smoke inputs valid: executable=$ExecutablePath"
	return
}

$previousResults = Get-ChildItem -LiteralPath $PackageDirectory -Recurse -File -Filter "result.json" -ErrorAction SilentlyContinue |
	Where-Object { $_.FullName -like "*WandboundLocalPlayPackagedSmoke*" }
foreach ($previous in $previousResults) {
	$archiveName = "result.previous.$([DateTime]::UtcNow.ToString('yyyyMMddHHmmssfff')).json"
	Move-Item -LiteralPath $previous.FullName -Destination (Join-Path $previous.DirectoryName $archiveName)
}

$logDirectory = Join-Path $projectRoot "Saved\Logs"
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$stdoutPath = Join-Path $logDirectory "WandboundLocalPlayPackagedSmoke.stdout.log"
$stderrPath = Join-Path $logDirectory "WandboundLocalPlayPackagedSmoke.stderr.log"
$arguments = @(
	"/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev",
	"-WandboundLocalPlaySmoke",
	"-unattended",
	"-nosound",
	"-log",
	"-stdout",
	"-FullStdOutLogOutput"
)
if (-not $DisableNullRHI) { $arguments += "-nullrhi" }

$process = Start-Process -FilePath $ExecutablePath -ArgumentList $arguments -PassThru -WindowStyle Hidden `
	-RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
$null = $process.Handle
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
	Stop-Process -Id $process.Id -Force
	throw "Packaged smoke timed out after $TimeoutSeconds seconds. Stdout: $stdoutPath"
}
$process.Refresh()
$processExitCode = $process.ExitCode
$resultFile = Get-ChildItem -LiteralPath $PackageDirectory -Recurse -File -Filter "result.json" -ErrorAction SilentlyContinue |
	Where-Object { $_.FullName -like "*WandboundLocalPlayPackagedSmoke*" } |
	Sort-Object LastWriteTimeUtc -Descending |
	Select-Object -First 1
if ($null -eq $resultFile) { throw "Packaged smoke result.json was not produced. Process exit code: $processExitCode" }
$result = Get-Content -LiteralPath $resultFile.FullName -Raw | ConvertFrom-Json
foreach ($requiredField in @("success", "failure_reason", "map_name", "game_mode_class", "bootstrap_state", "match_generation", "presentation_revision", "tile_count", "visible_unit_count", "visible_hero_count", "concealed_marker_count", "own_hand_count", "legal_action_count", "action_submitted", "end_turn_submitted", "game_over", "winner_player_id", "process_exit_code")) {
	if ($null -eq $result.PSObject.Properties[$requiredField]) { throw "Smoke result missing required field: $requiredField" }
}
if ($processExitCode -ne 0) { throw "Packaged process failed with exit code $processExitCode. Result reason: $($result.failure_reason)" }
if (-not $result.success -or $result.process_exit_code -ne 0) { throw "Smoke JSON reported failure: $($result.failure_reason)" }

Write-Output "Packaged smoke passed: exit=$processExitCode map=$($result.map_name) generation=$($result.match_generation) revision=$($result.presentation_revision)"
Write-Output "Smoke result: $($resultFile.FullName)"
Write-Output "Smoke stdout: $stdoutPath"
