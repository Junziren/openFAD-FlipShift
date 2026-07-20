param(
    [string]$JucePath = '',
    [string]$VcvarsPath = ''
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$vstRoot = Join-Path $root 'vst3'
$buildDir = Join-Path $vstRoot 'build'

if ([string]::IsNullOrWhiteSpace($JucePath) -and (Test-Path -LiteralPath 'D:\JUCE\CMakeLists.txt')) {
    $JucePath = 'D:\JUCE'
}

if ([string]::IsNullOrWhiteSpace($VcvarsPath)) {
    $candidates = @(
        'D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat'
    )
    $VcvarsPath = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

$configureArgs = @('-S', $vstRoot, '-B', $buildDir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release')
if (-not [string]::IsNullOrWhiteSpace($JucePath)) {
    $configureArgs += "-DJUCE_PATH=$JucePath"
}

function Quote-CmdArg([string]$value) {
    '"' + ($value -replace '"', '\"') + '"'
}

if (-not [string]::IsNullOrWhiteSpace($VcvarsPath)) {
    Write-Host "Using MSVC environment: $VcvarsPath"
    $configureLine = 'cmake ' + (($configureArgs | ForEach-Object { Quote-CmdArg $_ }) -join ' ')
    $buildLine = 'cmake --build ' + (Quote-CmdArg $buildDir) + ' --config Release'
    cmd /c "chcp 65001 >nul && call `"$VcvarsPath`" && set VSLANG=1033 && $configureLine && $buildLine"
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
} else {
    cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }
    cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE" }
}

ctest --test-dir $buildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE" }

Write-Host 'openFAD FlipShift VST3 build, DSP tests and native WebView integration tests completed.'
