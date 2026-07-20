param(
    [ValidateSet('User', 'System')]
    [string]$Scope = 'System',
    [string]$PluginPath = ''
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($PluginPath)) {
    $PluginPath = Join-Path $root 'vst3\build\OpenFADFlipShift_artefacts\Release\VST3\openFAD FlipShift.vst3'
}

if (-not (Test-Path -LiteralPath $PluginPath -PathType Container)) {
    throw "Release VST3 bundle was not found at $PluginPath. Build it or pass -PluginPath explicitly."
}

$plugin = Get-Item -LiteralPath (Resolve-Path -LiteralPath $PluginPath).Path
$sourceBinary = Get-ChildItem -LiteralPath $plugin.FullName -Recurse -File -Filter 'openFAD FlipShift.vst3' |
    Select-Object -First 1
if (-not $sourceBinary) { throw "VST3 binary was not found inside $($plugin.FullName)" }

$destinationRoot = if ($Scope -eq 'System') {
    'C:\Program Files\Common Files\VST3'
} else {
    Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'
}

New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
$installedPath = Join-Path $destinationRoot $plugin.Name
$destinationRootFull = [IO.Path]::GetFullPath($destinationRoot).TrimEnd('\') + '\'
$installedPathFull = [IO.Path]::GetFullPath($installedPath)
if (-not $installedPathFull.StartsWith($destinationRootFull, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to replace an install path outside $destinationRoot"
}

if (Test-Path -LiteralPath $installedPath) {
    Remove-Item -LiteralPath $installedPath -Recurse -Force
}
Copy-Item -LiteralPath $plugin.FullName -Destination $destinationRoot -Recurse -Force

$installedBinary = Get-ChildItem -LiteralPath $installedPath -Recurse -File -Filter 'openFAD FlipShift.vst3' |
    Select-Object -First 1
if (-not $installedBinary) { throw "Installed VST3 binary was not found inside $installedPath" }

$sourceHash = (Get-FileHash -LiteralPath $sourceBinary.FullName -Algorithm SHA256).Hash
$installedHash = (Get-FileHash -LiteralPath $installedBinary.FullName -Algorithm SHA256).Hash
if ($sourceHash -ne $installedHash) {
    throw "Installed VST3 hash mismatch. Source: $sourceHash Installed: $installedHash"
}

Write-Host "Installed openFAD FlipShift for $Scope VST3 scanning: $installedPath"
Write-Host "Verified SHA-256: $installedHash"
