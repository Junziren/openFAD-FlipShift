$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$jsonFiles = Get-ChildItem -LiteralPath $root -Recurse -File | Where-Object {
    $_.Extension -in '.maxpat', '.json'
}
foreach ($file in $jsonFiles) {
    Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json | Out-Null
    Write-Host "OK JSON: $($file.FullName)"
}
$required = @(
    'VectorShift.maxproj',
    'patchers/VectorShift.maxpat',
    'patchers/classic-engine.maxpat',
    'patchers/spectral-engine.maxpat',
    'patchers/spectral-core.maxpat',
    'patchers/spectral-core-low.maxpat',
    'patchers/spectral-core-high.maxpat',
    'patchers/modulation.maxpat',
    'patchers/jit_fft_viz.maxpat',
    'patchers/amp_pfft.maxpat',
    'code/vector-spectrum.js',
    'code/spectral-mask.js',
    'code/modulation-router.js',
    'presets/factory-presets.json'
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $path))) { throw "Missing: $path" }
}
Write-Host 'VectorShift source validation passed.'
