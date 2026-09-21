$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Assert-Contract {
    param(
        [bool] $Condition,
        [string] $Message
    )

    if (-not $Condition) { throw "UI contract validation failed: $Message" }
}

$jsonFiles = Get-ChildItem -LiteralPath $root -Recurse -File | Where-Object {
    $_.Extension -in '.maxpat', '.json' -and
    $_.FullName -notmatch '[\\/]vst3[\\/]build[^\\/]*[\\/]'
}
foreach ($file in $jsonFiles) {
    Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json | Out-Null
    Write-Host "OK JSON: $($file.FullName)"
}
$required = @(
    'LICENSING.md'
    'README.zh-CN.md'
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
    'vst3/WebUI/index.html'
    'vst3/WebUI/styles.css'
    'vst3/WebUI/app.js'
    'vst3/WebUI/ui-spec.json'
    'vst3/Tests/GUIIntegrationTests.cpp'
    'vst3/DSP_OPTIMIZATION_PLAN.md'
    'vst3/WEBVIEW_AUV3.md'
    '.github/workflows/macos-package.yml'
    'scripts/package-auv3.sh'
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $path))) { throw "Missing: $path" }
}

$uiSpecPath = Join-Path $root 'vst3/WebUI/ui-spec.json'
$uiSpec = Get-Content -LiteralPath $uiSpecPath -Raw | ConvertFrom-Json
Assert-Contract ($uiSpec.schemaVersion -eq 2) 'ui-spec schemaVersion must be 2.'

$appJsPath = Join-Path $root 'vst3/WebUI/app.js'
$appJs = Get-Content -LiteralPath $appJsPath -Raw
$parametersCppPath = Join-Path $root 'vst3/Source/Parameters.cpp'
$parametersCpp = Get-Content -LiteralPath $parametersCppPath -Raw
$macosWorkflow = Get-Content -LiteralPath (Join-Path $root '.github/workflows/macos-package.yml') -Raw
$auv3PackageScript = Get-Content -LiteralPath (Join-Path $root 'scripts/package-auv3.sh') -Raw
$modeNameBlock = [regex]::Match($appJs, '(?s)const modeNames = \[(.*?)\];')
$modeBlock = [regex]::Match($appJs, '(?s)const modes = \[(.*?)\];\s*const skewForCentre')
$profileBlock = [regex]::Match($appJs, '(?s)const waterfallVisualProfiles = \{(.*?)\};\s*const waterfallColourTables')
$cppModeNameBlock = [regex]::Match($parametersCpp, '(?s)juce::StringArray\s+getModeNames\(\)\s*\{\s*return\s*\{(.*?)\};\s*\}')
Assert-Contract ($modeNameBlock.Success -and $modeBlock.Success -and $profileBlock.Success -and $cppModeNameBlock.Success) 'C++ or app.js UI metadata blocks could not be read.'
$appModeNames = @([regex]::Matches($modeNameBlock.Groups[1].Value, '"([^"\\]*(?:\\.[^"\\]*)*)"') | ForEach-Object { $_.Groups[1].Value })
$cppModeNames = @([regex]::Matches($cppModeNameBlock.Groups[1].Value, '"([^"\\]*(?:\\.[^"\\]*)*)"') | ForEach-Object { $_.Groups[1].Value })
$appModeDescriptions = @([regex]::Matches($modeBlock.Groups[1].Value, 'description:\s*"([^"\\]*(?:\\.[^"\\]*)*)"') | ForEach-Object { $_.Groups[1].Value })
$appProfileDescriptions = @([regex]::Matches($profileBlock.Groups[1].Value, 'description:\s*"([^"\\]*(?:\\.[^"\\]*)*)"') | ForEach-Object { $_.Groups[1].Value })

$expectedModeIds = @(
    'off', 'detune', 'smear', 'spread', 'harmonics', 'subharm',
    'gate', 'robotize', 'shift', 'mirror', 'peakFollow', 'peakOctUp',
    'peakOctDown', 'peakHmxUp', 'peakHmxDown', 'harmSweep', 'shepard',
    'shepardWide', 'comb', 'pitchBlend', 'pitchShift', 'phaseTwist',
    'glitch', 'pitchMap'
)
$expectedModeLabels = @(
    'Off', 'Bend', 'Smear', 'Spread', 'Harmonics', 'Subharm',
    'Gate', 'Zero Phase', 'Shift', 'Mirror', 'Peak Push', 'Peak x2',
    'Peak /2', 'Peak Expand', 'Peak Compress', 'Harm Sweep',
    'Oct Stack', 'Wide Oct Stack', 'Comb', 'Pitch Blend',
    'Spectral Scale', 'Phase Ripple', 'Glitch', 'Pitch Map'
)
$modes = @($uiSpec.modes)
Assert-Contract ($modes.Count -eq $expectedModeIds.Count) 'ui-spec must contain exactly 24 modes.'
Assert-Contract ($appModeNames.Count -eq $modes.Count) 'app.js modeNames must contain exactly 24 labels.'
Assert-Contract ($cppModeNames.Count -eq $modes.Count) 'C++ getModeNames must contain exactly 24 labels.'
Assert-Contract ($appModeDescriptions.Count -eq $modes.Count) 'app.js modes must contain exactly 24 descriptions.'

for ($index = 0; $index -lt $expectedModeIds.Count; $index++) {
    $mode = $modes[$index]
    Assert-Contract ($mode.value -eq $index) "mode at index $index must use value $index."
    Assert-Contract ($mode.id -eq $expectedModeIds[$index]) "mode value $index must use id '$($expectedModeIds[$index])'."
    Assert-Contract ($mode.label -eq $expectedModeLabels[$index]) "mode '$($mode.id)' must use display label '$($expectedModeLabels[$index])'."
    Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $mode.label)) "mode '$($mode.id)' must have a label."
    Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $mode.description)) "mode '$($mode.id)' must have a description."
    Assert-Contract ($mode.PSObject.Properties.Name -contains 'enabledControls') "mode '$($mode.id)' must declare enabledControls."
    Assert-Contract ($appModeNames[$index] -eq $mode.label) "app.js label for mode '$($mode.id)' differs from ui-spec."
    Assert-Contract ($cppModeNames[$index] -eq $mode.label) "C++ label for mode '$($mode.id)' differs from ui-spec."
    Assert-Contract ($appModeDescriptions[$index] -eq $mode.description) "app.js description for mode '$($mode.id)' differs from ui-spec."
}

Assert-Contract ($modes[1].labelOverrides.shiftHz -eq 'BEND' -and $modes[1].labelOverrides.pivotHz -eq 'CENTER') 'Bend controls must use BEND and CENTER.'
Assert-Contract ($modes[10].labelOverrides.pivotHz -eq 'ANCHOR' -and $modes[10].labelOverrides.amount -eq 'REPEL') 'Peak Push controls must use ANCHOR and REPEL.'
Assert-Contract ($modes[11].labelOverrides.amount -eq 'BLEND' -and $modes[12].labelOverrides.amount -eq 'BLEND') 'Fixed peak-distance modes must not claim octave controls.'
Assert-Contract ($modes[13].labelOverrides.scale -eq 'SPAN' -and $modes[14].labelOverrides.scale -eq 'SPAN') 'Variable peak-distance modes must not use HMX terminology.'
Assert-Contract ($modes[16].labelOverrides.amount -eq 'OCTAVE POSITION' -and $modes[17].labelOverrides.amount -eq 'OCTAVE POSITION') 'Octave Stack controls must use OCTAVE POSITION.'
Assert-Contract ($modes[21].labelOverrides.pivotHz -eq 'RIPPLE SCALE' -and $modes[21].labelOverrides.amount -eq 'ROTATION') 'Phase Ripple controls must use RIPPLE SCALE and ROTATION.'

$expectedParameterIds = @(
    'mode', 'shiftHz', 'scale', 'pivotHz', 'amount', 'widthQ', 'mix',
    'outputGainDb', 'quality', 'analyzerView', 'bypass', 'freeze',
    'pitchRoot', 'pitchScale'
)
$parameterControls = @($uiSpec.controls | Where-Object { $expectedParameterIds -contains $_.id })
Assert-Contract ($parameterControls.Count -eq 14) 'ui-spec must represent all 14 unique plugin parameters.'
foreach ($parameterId in $expectedParameterIds) {
    Assert-Contract (@($parameterControls | Where-Object { $_.id -eq $parameterId }).Count -eq 1) "plugin parameter '$parameterId' must be represented exactly once."
}

$pitchRootControl = @($parameterControls | Where-Object { $_.id -eq 'pitchRoot' })[0]
$pitchScaleControl = @($parameterControls | Where-Object { $_.id -eq 'pitchScale' })[0]
Assert-Contract ($pitchRootControl.automatable -eq $true -and @($pitchRootControl.choices).Count -eq 12) 'pitchRoot must be an automatable 12-note choice.'
Assert-Contract ($pitchScaleControl.automatable -eq $true -and @($pitchScaleControl.choices).Count -eq 2) 'pitchScale must be an automatable Major/Minor choice.'
Assert-Contract (@($pitchRootControl.visibleForModes).Count -eq 1 -and $pitchRootControl.visibleForModes[0] -eq 23) 'pitchRoot selector must be visible only for Pitch Map.'
Assert-Contract (@($pitchScaleControl.visibleForModes).Count -eq 1 -and $pitchScaleControl.visibleForModes[0] -eq 23) 'pitchScale selector must be visible only for Pitch Map.'

$glitchMode = $modes[22]
$pitchMapMode = $modes[23]
Assert-Contract ($glitchMode.id -eq 'glitch' -and $glitchMode.labelOverrides.pivotHz -eq 'BAND CENTER' -and $glitchMode.labelOverrides.widthQ -eq 'BAND Q') 'Glitch selection controls must identify band centre and Q.'
Assert-Contract (@($pitchMapMode.enabledControls) -contains 'pitchRoot' -and @($pitchMapMode.enabledControls) -contains 'pitchScale') 'Pitch Map must enable both key selectors.'

$modeControls = @($uiSpec.controls | Where-Object { $_.id -eq 'mode' })
Assert-Contract ($modeControls.Count -eq 1) 'controls must contain exactly one mode control.'
$modeHelp = $modeControls[0].effectHelp
Assert-Contract ($null -ne $modeHelp) 'mode control must define effectHelp.'
Assert-Contract ($modeHelp.descriptionSource -eq 'modes[].description') 'mode effectHelp must read modes[].description.'
Assert-Contract ($modeHelp.descriptionElement -eq 'modeEffectDescription') 'mode effectHelp must target modeEffectDescription.'
Assert-Contract ($modeHelp.ariaDescribedBy -eq 'modeEffectDescription') 'mode effectHelp aria relation is missing.'
Assert-Contract (@($modeHelp.updateTriggers).Count -ge 6) 'mode effectHelp must cover local input, automation and restore updates.'

foreach ($controlId in @('shiftHz', 'scale', 'pivotHz', 'amount', 'widthQ')) {
    $tooltip = $modeHelp.defaultControlTooltips.$controlId
    Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $tooltip)) "default tooltip for '$controlId' is missing."
}

$expectedProfileIds = @('vision', 'pulse', 'phosphor', 'xray')
$waterfall = $uiSpec.analyzer.waterfall
$profiles = @($waterfall.visualProfiles)
Assert-Contract ($profiles.Count -eq $expectedProfileIds.Count) 'waterfall must contain exactly four visual profiles.'
Assert-Contract ($appProfileDescriptions.Count -eq $profiles.Count) 'app.js waterfallVisualProfiles must contain four descriptions.'

for ($profileIndex = 0; $profileIndex -lt $expectedProfileIds.Count; $profileIndex++) {
    $profile = $profiles[$profileIndex]
    $expectedId = $expectedProfileIds[$profileIndex]
    Assert-Contract ($profile.id -eq $expectedId) "visual profile at index $profileIndex must use id '$expectedId'."
    Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $profile.label)) "visual profile '$expectedId' must have a label."
    Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $profile.description)) "visual profile '$expectedId' must have a description."
    Assert-Contract ($appProfileDescriptions[$profileIndex] -eq $profile.description) "app.js description for visual profile '$expectedId' differs from ui-spec."
    Assert-Contract ([double] $profile.floorDb -lt [double] $profile.ceilingDb) "visual profile '$expectedId' must have floorDb below ceilingDb."
    Assert-Contract ([double] $profile.gamma -gt 0.0) "visual profile '$expectedId' must have positive gamma."
    Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $profile.gridAccent)) "visual profile '$expectedId' must define gridAccent."
    Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $profile.newestEdgeAccent)) "visual profile '$expectedId' must define newestEdgeAccent."

    $stops = @($profile.stops)
    Assert-Contract ($stops.Count -ge 2) "visual profile '$expectedId' must contain at least two colour stops."
    $previousPosition = -1.0
    for ($stopIndex = 0; $stopIndex -lt $stops.Count; $stopIndex++) {
        $stop = @($stops[$stopIndex])
        Assert-Contract ($stop.Count -eq 2) "visual profile '$expectedId' stop $stopIndex must contain position and RGB."
        $position = [double] $stop[0]
        $rgb = @($stop[1])
        Assert-Contract ($position -ge 0.0 -and $position -le 1.0) "visual profile '$expectedId' stop $stopIndex position must be within 0..1."
        Assert-Contract ($position -gt $previousPosition) "visual profile '$expectedId' stops must be strictly increasing."
        Assert-Contract ($rgb.Count -eq 3) "visual profile '$expectedId' stop $stopIndex must contain three RGB channels."
        foreach ($channel in $rgb) {
            $channelValue = [double] $channel
            Assert-Contract ($channelValue -ge 0 -and $channelValue -le 255 -and $channelValue -eq [math]::Floor($channelValue)) "visual profile '$expectedId' RGB channels must be integers within 0..255."
        }
        $previousPosition = $position
    }
    Assert-Contract ([double] $stops[0][0] -eq 0.0) "visual profile '$expectedId' must start at 0."
    Assert-Contract ([double] $stops[$stops.Count - 1][0] -eq 1.0) "visual profile '$expectedId' must end at 1."
}

Assert-Contract ($waterfall.visualProfileContract.default -eq 'vision') 'default visual profile must be vision.'
Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $waterfall.visualProfileContract.historyMutation)) 'visual profile history-mutation rule is missing.'
Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $waterfall.visualProfileContract.processingIsolation)) 'visual profile processing-isolation rule is missing.'
Assert-Contract ($appJs -match 'const elapsed = clamp\(now - lastWaterfallAnimationTime, 0, 250\);') 'waterfall clock must tolerate low-frame-rate intervals.'
Assert-Contract ($appJs -match 'const advance = Math\.min\(32, due\);') 'waterfall clock must cap catch-up work without discarding pending columns.'
Assert-Contract ($appJs -match 'waterfallAdvanceAccumulator -= advance;') 'waterfall clock must retain columns that were not advanced this frame.'

$visualControls = @($waterfall.localControls | Where-Object { $_.id -eq 'waterfallVisual' })
Assert-Contract ($visualControls.Count -eq 1) 'waterfall localControls must contain waterfallVisual.'
$visualControl = $visualControls[0]
Assert-Contract ($visualControl.label -eq 'LOOK') 'waterfallVisual label must be LOOK.'
Assert-Contract ($visualControl.choicesSource -eq 'visualProfiles') 'waterfallVisual must use visualProfiles choices.'
Assert-Contract ($visualControl.hostParameter -eq $false) 'waterfallVisual must remain a local non-host control.'
Assert-Contract ($expectedProfileIds -contains $visualControl.default) 'waterfallVisual default must reference a visual profile.'
Assert-Contract (-not [string]::IsNullOrWhiteSpace([string] $visualControl.persistence)) 'waterfallVisual persistence rule is missing.'

$html = Get-Content -LiteralPath (Join-Path $root 'vst3/WebUI/index.html') -Raw
$editorCpp = Get-Content -LiteralPath (Join-Path $root 'vst3/Source/PluginEditor.cpp') -Raw
$cmake = Get-Content -LiteralPath (Join-Path $root 'vst3/CMakeLists.txt') -Raw
$licensing = Get-Content -LiteralPath (Join-Path $root 'LICENSING.md') -Raw
$webUiRuntimeTest = Join-Path $root 'tests/validate-webui.py'
Assert-Contract (Test-Path -LiteralPath $webUiRuntimeTest -PathType Leaf) 'WebUI runtime validation script is missing.'
Assert-Contract ($html -match 'styles\.css\?v=8' -and $html -match 'app\.js\?v=8') 'WebUI asset cache version must be v8.'
Assert-Contract ($editorCpp -match 'index\.html\?v=8') 'C++ WebUI root cache version must be v8.'
Assert-Contract ($html -match 'id="axisModule"' -and $html -match 'id="pitchMapModule"' -and $html -match 'id="pitchRoot"' -and $html -match 'id="pitchScale"') 'Pitch Map selector module is incomplete.'
Assert-Contract ($appJs -match 'axisModule\.hidden = pitchMapActive' -and $appJs -match 'pitchMapModule\.hidden = !pitchMapActive') 'Pitch Map must replace AXIS in the fixed parameter-strip slot.'
Assert-Contract ($html -match 'id="glitchSelection"' -and $html -match 'id="pitchMapReadout"') 'Mode-specific analyzer overlays are missing.'
Assert-Contract ($appJs -match 'const bandwidth = clamp\(centre / clamp\(state\.widthQ, 0\.05, 8\), 4 \* binWidth, nyquist\);') 'Glitch selection overlay does not match the DSP band formula.'
Assert-Contract ($uiSpec.analyzer.waterfall.modeOverlays.Count -eq 2) 'ui-spec must define Glitch and Pitch Map analyzer overlays.'
Assert-Contract ($uiSpec.analyzer.waterfall.modeOverlays[0].dataIsolation -match 'does not.*mutate analyzer history') 'Glitch overlay must remain isolated from waterfall data.'
$surfaceVisibilityEvents = @($uiSpec.nativeBridge.cppToFrontend | Where-Object { $_.event -eq 'surfaceVisibility' })
Assert-Contract ($surfaceVisibilityEvents.Count -eq 1) 'native bridge must define exactly one surfaceVisibility event.'
Assert-Contract ($appJs -match 'backend\.addEventListener\("surfaceVisibility"' -and $appJs -match 'function stopAnimationLoop\(\)') 'WebUI native visibility animation lifecycle is missing.'
Assert-Contract ($editorCpp -match 'emitEventIfBrowserIsVisible\("surfaceVisibility"') 'C++ must emit native surface visibility changes.'
Assert-Contract ($editorCpp -match 'void OpenFADFlipShiftAudioProcessorEditor::parentHierarchyChanged\(\)' -and $editorCpp -match 'timerCallback\(\)[\s\S]*syncNativeSurfaceState\(\);') 'Editor visibility must be resynchronised after host attachment and from the timer fallback.'
Assert-Contract ($editorCpp -match 'audioProcessor\.setAnalyzerConsumerActive\(true\)' -and $editorCpp -match 'audioProcessor\.setAnalyzerConsumerActive\(false\)') 'Analyzer production must span the editor lifetime.'
Assert-Contract ($editorCpp -notmatch '!frontendReady\s*\|\|\s*!lastSurfaceVisible' -and $editorCpp -notmatch 'setAnalyzerConsumerActive\(visible\)') 'Analyzer delivery must not be gated by host-sensitive surface visibility.'
Assert-Contract ($editorCpp -match 'const auto visible = isVisible\(\);' -and $editorCpp -notmatch 'const auto visible = isShowing\(\);') 'Native surface hints must not use the parent-peer-sensitive isShowing state.'
Assert-Contract ($cmake -match 'juce_add_console_app\(FlipShiftGUIIntegrationTests' -and $cmake -match 'add_test\(NAME FlipShiftGUIIntegrationTests') 'Windows native WebView analyzer integration test is missing.'
$knobTrail = $uiSpec.designTokens.motion.interactionFeedback
Assert-Contract ($knobTrail.particlePoolMaximum -eq 24) 'knob trail must use a fixed 24-particle pool.'
Assert-Contract ($knobTrail.frameRateMaximumHz -eq 30) 'knob trail must remain capped at 30 FPS.'
Assert-Contract ($appJs -match 'Array\.from\(\{ length: 24 \}') 'app.js knob trail pool size differs from ui-spec.'
Assert-Contract ($appJs -match 'now - knobTrailLastFrameTime < 1000 / 30') 'app.js knob trail frame cap is missing.'
Assert-Contract ($appJs -match 'reducedMotionQuery\.matches.*document\.hidden') 'app.js knob trail reduced-motion and visibility guard is missing.'
Assert-Contract ($appJs -notmatch 'setLocalParameter\([^\r\n]+emitKnobTrail') 'knob trail must not be attached to generic parameter-state rendering.'

$about = $uiSpec.about
Assert-Contract ($about.trigger -eq 'aboutOpen' -and $about.dialog -eq 'aboutDialog') 'About trigger or dialog contract is missing.'
Assert-Contract ($html -match 'id="aboutOpen"' -and $html -match 'id="aboutDialog"') 'About HTML controls are missing.'
Assert-Contract (@($about.features).Count -ge 4) 'About must list the main product functions.'
Assert-Contract ($about.license.status -eq 'notYetDeclared' -and $about.license.shortName -eq 'NOT YET DECLARED') 'About must not claim an undeclared FlipShift license.'
Assert-Contract ($about.license.upstream.shortName -eq 'GNU AGPLv3') 'About must identify GNU AGPLv3 only as the upstream openFAD license.'
Assert-Contract ($about.license.frameworkDependency.version -eq '8.0.12' -and $about.license.frameworkDependency.status -eq 'routeNotYetDeclared') 'About must record the undeclared JUCE 8.0.12 licensing route.'
Assert-Contract ($html -match 'NOT YET DECLARED' -and $html -match 'Upstream openFAD uses GNU AGPLv3' -and $html -match 'JUCE 8') 'About HTML license scope is inaccurate.'
Assert-Contract ($html -match 'VST3/WebView2 and AUv3/WKWebView build paths') 'About host integration must cover both configured WebView backends.'
$projectLicenseCandidates = @('LICENSE', 'LICENSE.md', 'LICENSE.txt', 'COPYING', 'COPYING.md') | Where-Object {
    Test-Path -LiteralPath (Join-Path $root $_)
}
Assert-Contract ($projectLicenseCandidates.Count -eq 0) 'ui-spec says the project license is undeclared, but a root license file now exists.'
Assert-Contract ($cmake -match 'GIT_TAG\s+8\.0\.12') 'JUCE FetchContent fallback must be pinned to 8.0.12.'
Assert-Contract ($cmake -match 'set\(OPENFAD_PLUGIN_FORMATS AUv3 Standalone\)') 'iOS formats must be limited to AUv3 and Standalone.'
Assert-Contract ($cmake -match 'set_target_properties\(OpenFADFlipShift OpenFADFlipShift_AUv3 PROPERTIES') 'App Extension-safe compilation must cover shared code and the AUv3 wrapper.'
Assert-Contract ($cmake -match 'XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY YES') 'AUv3 App Extension-safe API enforcement is missing.'
Assert-Contract ($macosWorkflow -match 'runs-on:\s*macos-15') 'AUv3 workflow must run on a macOS runner.'
Assert-Contract ($macosWorkflow -match '-DOPENFAD_BUILD_AUV3=ON') 'macOS workflow must enable AUv3 configuration.'
Assert-Contract ($macosWorkflow -match '--target OpenFADFlipShift_Standalone OpenFADFlipShift_AUv3') 'macOS workflow must explicitly build the AUv3 container and extension targets.'
Assert-Contract ($macosWorkflow -match 'CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO') 'Unsigned AUv3 CI builds must disable Xcode signing requirements explicitly.'
Assert-Contract ($macosWorkflow -match 'bash scripts/package-auv3\.sh') 'macOS workflow must run the AUv3 packaging script.'
Assert-Contract ($macosWorkflow -match 'path:\s*dist/auv3/\*') 'macOS workflow must upload the AUv3 package artifact.'
Assert-Contract ($auv3PackageScript -match "find_bundle 'openFAD FlipShift\.app'" -and $auv3PackageScript -match "find_bundle 'openFAD FlipShift\.appex'") 'AUv3 packaging must require both the standalone app and extension bundles.'
Assert-Contract ($auv3PackageScript -match 'lipo.*-verify_arch arm64 x86_64') 'AUv3 packaging must verify universal arm64/x86_64 binaries.'
Assert-Contract ($auv3PackageScript -match 'plutil -lint') 'AUv3 packaging must validate bundle Info.plist files.'
Assert-Contract ($licensing -match 'does not contain a `LICENSE`' -and $licensing -match 'JUCE 8\.0\.12') 'LICENSING.md does not match the current project and JUCE status.'
$aboutUrls = @($about.project.url, $about.developer.url, $about.source, $about.license.upstream.url)
foreach ($url in $aboutUrls) {
    Assert-Contract ($html.Contains($url)) "About HTML is missing URL '$url'."
    Assert-Contract ($editorCpp.Contains($url)) "native external-link allow list is missing '$url'."
}
Assert-Contract ($appJs -match 'backend\.emitEvent\("openExternal"') 'About native openExternal event is missing.'
Assert-Contract ($editorCpp -match 'withEventListener\("openExternal"') 'C++ openExternal listener is missing.'
Assert-Contract ($editorCpp -match '#if JUCE_IOS && defined\(JucePlugin_Build_AUv3\) && JucePlugin_Build_AUv3') 'AUv3 external-link guard is missing.'

Write-Host 'OK UI contract: 24 described modes, 14 parameters, isolated mode overlays, four waterfall LOOK profiles, pooled knob trails and About.'
Write-Host 'VectorShift source validation passed.'
