param(
    [ValidateSet('User', 'System')]
    [string]$Scope = 'System'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'vst3/build'
$plugin = Get-ChildItem -LiteralPath $buildDir -Recurse -Filter 'openFAD FlipShift.vst3' -ErrorAction SilentlyContinue |
    Where-Object { $_.PSIsContainer } |
    Select-Object -First 1

if (-not $plugin) {
    throw 'Built VST3 bundle was not found. Run tests/build-vst3.ps1 first.'
}

$destinationRoot = if ($Scope -eq 'System') {
    'C:\Program Files\Common Files\VST3'
} else {
    Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'
}

New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
Copy-Item -LiteralPath $plugin.FullName -Destination $destinationRoot -Recurse -Force

$installedPath = Join-Path $destinationRoot $plugin.Name
Write-Host "Installed openFAD FlipShift for $Scope VST3 scanning: $installedPath"
