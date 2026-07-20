# openFAD FlipShift / VectorShift

[中文工作说明](README.zh-CN.md)

This repository now contains two related tracks:

- `patchers/` and `code/`: the original VectorShift Max for Live spectral
  prototype targeting Ableton Live 12 and Max 9.
- `vst3/`: the openFAD FlipShift JUCE/CMake VST3/AUv3 implementation.

openFAD FlipShift is a spectral creative frequency shifter built around the
idea of flipping, shifting, scaling and re-mapping frequency content.
The developer and VST3 vendor name is `UnpureBloom`.

## VST3 implementation

The VST3 project lives in `vst3/` and uses JUCE through CMake. Pass
`-DJUCE_PATH=C:\path\to\JUCE` when you want to use a local JUCE checkout;
otherwise CMake FetchContent pulls JUCE from the official repository.

Core v1 scope:

- 24 spectral transforms with behavior-based public names: Neutral, Pivot Bend,
  Magnitude Diffusion, Stereo Translate, Harmonic Sieve, Spectral Divider,
  Peak-Relative Gate, Phase Reset, Hz Translation, Pivot Reflection, Peak
  Repel, Peak Stretch 2x, Peak Compress 2:1, Peak Stretch, Peak Compress,
  Harmonic Scan, Octave Stack Tight, Octave Stack Wide, Magnitude Comb, Ratio
  Crossfade, Phase-Tracked Scale, Phase Ripple, Band Glitch, and Pitch Map.
- Band Glitch applies deterministic, time-varying spectral offsets only inside
  the band selected by Band Center and Band Q. Offset controls displacement;
  Density controls event probability, wet depth, and refresh cadence.
- Pitch Map redistributes spectral energy toward the nearest pitch classes for
  any of 12 roots in Major or Minor; the Minor choice uses natural-minor
  intervals. Map Depth blends the original and mapped spectra.
- STFT processing with Low, Normal and High quality FFT sizes.
- Delayed dry path matched to the spectral path for dry/wet mixing.
- Embedded JUCE 8 WebView UI with WebView2 on Windows and WKWebView on Apple;
  release builds have no frontend server or external network dependency.
- The WebView is a native plug-in rendering surface, not a browser product: the
  document root does not scroll, and native mode blocks text selection, context
  menus, drag/drop, internal navigation, and browser shortcuts.
- Real-time input/output spectrum plus a Canvas waterfall, with analyzer data
  reduced to 192 logarithmic points and transferred at 15 Hz.
- Responsive selectors, mode-aware parameter labels, disabled inactive
  controls, automation gestures, compact ANALYZE/CONTROL tabs, and a 320x280
  minimum editor size.
- Hiding the editor sends a surface hint that stops the frontend's main
  `requestAnimationFrame` loop while preserving waterfall history. The native
  analyzer consumer remains enabled for the Editor lifetime and is disabled
  only when the Editor is destroyed.
- Centre-positioned defaults and skewed parameter ranges for finer adjustment
  around 0 Hz, 1x scale, 1 kHz pivot, Q 1 and 0 dB.
- Parameter state restore through `AudioProcessorValueTreeState`.
- The first realtime-safety pass moves quality reconfiguration off the audio
  callback, uses a consistent callback-to-configuration lock order and reset
  path, preallocates spectral memory, publishes analyzer data through an SPSC
  three-buffer handoff with generations, makes Magnitude Diffusion O(N), and
  rejects non-finite parameters and samples.

Build and run DSP tests:

```powershell
powershell -ExecutionPolicy Bypass -File tests/build-vst3.ps1
```

The current Windows Release build and CTest suite pass 2/2: the DSP executable
and a real WebView2 GUI integration test. The GUI test links the same Processor,
Editor, DSP, parameter, and embedded WebUI sources, drives deterministic audio,
and inspects the actual WebView2 DOM. A representative run delivered 46
`analyzerFrame` events, 190 waterfall columns, 192 input plus 192 output points,
and -10.5 dB input/output meter readouts. This test executable does not load the
installed VST3 bundle and does not cover Ableton's VST3 wrapper, scan cache, or
Ableton-specific parent-window/editor lifecycle.

Run the four-viewport Playwright check separately; it covers 1000x650, 621x844,
390x844, and 320x280, asserts waterfall pixel changes and meter fills/readouts,
and can write viewport plus analyzer screenshots:

```powershell
python tests/validate-webui.py --screenshots-dir vst3/build/webui-qa
```

Run pluginval after a Release VST3 has been built. The script accepts
`-PluginvalPath`, prefers the local pluginval 1.0.4 build, then falls back to
`D:\pluginval\pluginval.exe` or PATH:

```powershell
powershell -ExecutionPolicy Bypass -File tests/run-pluginval.ps1
```

The current Release and system-installed bundles share SHA-256
`9B0D45FD15D651DE5CF0603C1934C4A0242D4414D8893EDA1F64989D2A584B0C`.
The installed copy passes pluginval 1.0.4 strictness level 10 with
`Repeat=3`. This is Windows VST3 evidence only; it does not validate AUv3.
The optional standalone Steinberg VST3 validator is not installed locally.

Link-time optimisation is opt-in with `-DOPENFAD_ENABLE_LTO=ON`. It is disabled
by default because MSVC 19.44 plus JUCE 8.0.12 produced reproducible heap
corruption under pluginval when `/GL` was enabled; the normal `/Ox` Release
optimisation remains enabled.

On Apple platforms `-DOPENFAD_BUILD_AUV3=ON` adds the AUv3 app extension and a
standalone container. Build/signing instructions and the device acceptance
matrix are in `vst3/WEBVIEW_AUV3.md`. Apple/Xcode builds, signing, AUv3 hosts,
and physical iPhone/iPad testing remain unverified. The first portable DSP
optimizations have landed, but Raspberry Pi 4/5 callback benchmarks and ARM
profiling remain unverified; the remaining work is tracked in
`vst3/DSP_OPTIMIZATION_PLAN.md`.

The complete WebView handoff contract for visual references and frontend
implementation is in `vst3/WebUI/ui-spec.json`. It defines every control,
parameter range, mode-dependent label, bridge event, responsive rule and UI
acceptance criterion.

Install the rebuilt bundle into the standard system VST3 directory:

```powershell
powershell -ExecutionPolicy Bypass -File tests/install-vst3.ps1 -Scope System
```

Ableton Live testing uses the clips in `tests/daw-audio/` and the checklist in
`tests/DAW_ACCEPTANCE.md`.

## VST3 UI previews

- `previews/flipshift-compact.png`
- `previews/flipshift-analyzer.png`
- `previews/flipshift-transform-map.png`
- `previews/flipshift-preview.html`: interactive browser preview.

Regenerate the static PNG previews:

```powershell
powershell -ExecutionPolicy Bypass -File previews/render-flipshift-previews.ps1
```

## Max for Live prototype

## Open the device

1. Open `VectorShift.maxproj` in Max 9, then open `patchers/VectorShift.maxpat`.
2. In Max for Live, choose **Save As** and save the device as `VectorShift.amxd`.
3. Keep the `patchers`, `javascript`, and `presets` folders beside the device while developing.

The repository stores the editable Max source. `.amxd` is generated by Max for
Live and is intentionally not fabricated as a text file.

## Signal flow

`plugin~ -> Classic/Spectral engines -> equal-power engine switch -> filter -> dry/wet -> limiter -> plugout~`

- Classic: Hilbert-transform single-sideband frequency shifting.
- Spectral: `pfft~`/`gizmo~` frequency scaling followed by single-sideband translation.
- UI: compact controls plus a JSUI logarithmic spectrum and waterfall display.

## Current v1 limits

- FFT quality is selected at device load; changing FFT size requires separate
  `pfft~` instances in a later optimization pass.
- Spectral analysis display is fed through `snapshot~` control-rate probes in
  this source build. The JSUI supports full FFT arrays through its `spectrum`
  message for integration with `analyzer~` or Jitter matrices.
- Factory presets are documented in `presets/factory-presets.json`; import them
  into Ableton presets after the `.amxd` wrapper is saved.

## Validation

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tests/validate.ps1
```

Then perform the listening checks in `tests/ACCEPTANCE.md` inside Live.

## UI previews

- `previews/industrial-compact.png`: compact device-first layout.
- `previews/industrial-analyzer.png`: spectrum and waterfall-first layout.
- `previews/industrial-modular.png`: modular signal-flow and modulation layout.
- `previews/compare.html`: editable source used to reproduce all three previews.
