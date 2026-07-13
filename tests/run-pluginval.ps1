param(
    [string]$PluginPath = '',
    [string]$PluginvalPath = '',
    [int]$Strictness = 10,
    [int]$TimeoutMs = 60000,
    [int]$Repeat = 1,
    [string]$RandomSeed = '',
    [switch]$Randomise
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$newPluginval = 'D:\pluginval\pluginval-develop\build-juce8\pluginval_artefacts\Release\pluginval.exe'
$localPluginval = 'D:\pluginval\pluginval.exe'
$pluginvalCommand = Get-Command pluginval.exe -ErrorAction SilentlyContinue

if (-not [string]::IsNullOrWhiteSpace($PluginvalPath)) {
    if (-not (Test-Path -LiteralPath $PluginvalPath)) {
        throw "pluginval was not found at $PluginvalPath"
    }
    $pluginvalExe = $PluginvalPath
} elseif (Test-Path -LiteralPath $newPluginval) {
    $pluginvalExe = $newPluginval
} elseif (Test-Path -LiteralPath $localPluginval) {
    $pluginvalExe = $localPluginval
} elseif ($pluginvalCommand) {
    $pluginvalExe = $pluginvalCommand.Source
} else {
    throw 'pluginval was not found in PATH or at D:\pluginval\pluginval.exe.'
}

if ([string]::IsNullOrWhiteSpace($PluginPath)) {
    $buildDir = Join-Path $root 'vst3/build'
    $candidate = Get-ChildItem -LiteralPath $buildDir -Recurse -Filter 'openFAD FlipShift.vst3' -ErrorAction SilentlyContinue |
        Where-Object { $_.PSIsContainer } |
        Select-Object -First 1
    if (-not $candidate) {
        throw 'Could not find openFAD FlipShift.vst3 under vst3/build. Pass -PluginPath explicitly.'
    }
    $PluginPath = $candidate.FullName
}

Write-Host "Using pluginval: $pluginvalExe"
Write-Host "Running pluginval strictness $Strictness on $PluginPath"
$argumentLine = "--strictness-level $Strictness --timeout-ms $TimeoutMs --validate `"$PluginPath`""
if ($Repeat -gt 1) { $argumentLine += " --repeat $Repeat" }
if (-not [string]::IsNullOrWhiteSpace($RandomSeed)) { $argumentLine += " --random-seed $RandomSeed" }
if ($Randomise) { $argumentLine += ' --randomise' }
$process = Start-Process -FilePath $pluginvalExe -ArgumentList $argumentLine -NoNewWindow -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "pluginval failed with exit code $($process.ExitCode)" }
Write-Host 'pluginval completed.'
