# openFAD FlipShift implementation status

## VST3 implementation

Implemented:

- JUCE/CMake VST3 project in `vst3/` with product name `openFAD FlipShift`.
- VST3 developer/vendor metadata is `UnpureBloom`.
- Stable spectral parameter model with behavior-based public mode names; the
  original mode values 0-21 remain unchanged for state compatibility.
- STFT engine with Low/Normal/High FFT quality, dry-path latency alignment,
  sample-level smoothing for continuous parameters and stereo-safe processing.
- 24 spectral transforms: Neutral, Pivot Bend, Magnitude Diffusion, Stereo
  Translate, Harmonic Sieve, Spectral Divider, Peak-Relative Gate, Phase Reset,
  Hz Translation, Pivot Reflection, Peak Repel, Peak Stretch 2x, Peak Compress
  2:1, Peak Stretch, Peak Compress, Harmonic Scan, Octave Stack Tight, Octave
  Stack Wide, Magnitude Comb, Ratio Crossfade, Phase-Tracked Scale, Phase
  Ripple, Band Glitch and Pitch Map.
- Band Glitch uses Band Center and Band Q to constrain deterministic,
  time-varying spectral offsets. Offset controls displacement; Density controls
  event probability, wet depth and refresh cadence. Bins outside the selected
  band are preserved and stereo channels share the same event pattern.
- Pitch Map exposes 12 roots plus Major/Minor, with Minor using natural-minor
  intervals, and blends energy toward the nearest in-scale pitch classes. Its
  mapping cache is rebuilt only when the root, scale, sample rate or FFT
  configuration changes.
- JUCE 8 embedded WebView editor with WebView2 on Windows and WKWebView on
  Apple. HTML/CSS/JavaScript assets are compiled into the plugin and do not use
  a production web server or external network requests.
- WebView is restricted to a native plug-in rendering surface: no document
  scrolling, text selection, context menu, drag/drop, internal navigation, or
  browser refresh/source/zoom shortcuts in native mode.
- Canvas input/output spectrum, scrolling waterfall, pivot/destination guides,
  responsive narrow layouts down to 320x280, compact ANALYZE/CONTROL tabs,
  mode-aware controls, and host automation gestures.
- Analyzer transfer is capped at 192 logarithmic points and 15 Hz to keep UI
  serialization and rendering outside the DSP performance budget.
- Hiding the editor pauses the frontend main `requestAnimationFrame`, gestures,
  and knob particles. The analyzer consumer remains active for the complete
  Editor lifetime and is disabled only in Editor destruction.
- Quality reconfiguration is asynchronous and outside `processBlock()`. Engine
  lifecycle paths use the same callback-lock then configuration-lock order and
  provide an explicit reset path.
- Spectral memory is allocated during prepare; analyzer publication uses an
  SPSC fixed three-buffer exchange with sequence/generation validation;
  Magnitude Diffusion is O(N); continuous values and audio samples have
  finite/range guards.
- CMake and source configuration are present to expose AUv3 plus the required
  standalone container when `OPENFAD_BUILD_AUV3=ON`; Windows cannot compile,
  sign or validate that wrapper.
- Knob defaults are centred where practical, with nonlinear ranges around the
  musically useful values for Shift, Scale, Pivot, Width/Q and output gain.
- DSP test executable and PowerShell build/pluginval scripts.
- Three static UI previews plus one interactive HTML preview.

Validated locally on Windows:

- Release build and CTest pass 2/2 using
  `D:\VS2022BuildTools` and `D:\JUCE`; the build script sets
  UTF-8 console output so Ninja records MSVC header dependencies correctly.
- Hz Translation, Phase-Tracked Scale, Pivot Reflection, unity-gain
  reconstruction, every spectral mode, Freeze latching, sample-rate/quality
  setup, dry-path latency and deterministic random mode/quality stress tests
  pass.
- Band Glitch tests cover deterministic replay, stereo consistency,
  epoch-to-epoch variation and per-frame preservation outside the selected
  band. Pitch Map tests cover C/G roots in Major and Minor (natural-minor
  intervals) and verify the expected dominant-frequency mapping.
- `tests/validate-webui.py` passes at 1000x650, 621x844, 390x844 and
  320x280, checking fixed document bounds, compact tabs, native-surface guards,
  waterfall pixel/brightness changes, meter fills/readouts and control text fit;
  the verification run also writes viewport and analyzer screenshots.
- The Windows GUI integration CTest links the same Processor, Editor, parameter,
  DSP and WebUI sources, drives deterministic audio through a real WebView2 DOM,
  and passes with a representative 46 analyzer events, 190 waterfall columns,
  192+192 analyzer points and -10.5 dB input/output meters. It does not load the
  installed VST3 or cover Ableton's wrapper and plug-in cache.
- The current Release is installed at
  `C:\Program Files\Common Files\VST3\openFAD FlipShift.vst3`; the Release
  and installed bundles share SHA-256
  `9B0D45FD15D651DE5CF0603C1934C4A0242D4414D8893EDA1F64989D2A584B0C`.
- The installed copy passes pluginval 1.0.4 strictness level 10 with
  `Repeat=3`. This validates the Windows VST3 bundle, not AUv3.
- Bypass is exported as the standard host bypass parameter. Quality, Analyzer
  View and Freeze are intentionally non-automatable; Mode and sound controls
  remain automatable.
- LTO is disabled by default after `/GL` reproducibly triggered heap corruption
  with MSVC 19.44 and JUCE 8.0.12. Normal Release optimisation remains enabled.
- The optional standalone Steinberg VST3 validator is not installed, so that
  pluginval sub-test is skipped.

Remaining release checks:

- Audition and tune each spectral mode in Ableton Live 12 Suite.
- Confirm Mode/Shift/Scale/Amount/Pitch Root/Pitch Scale automation and quality
  changes by ear.
- Verify preset restore and latency compensation in Ableton Live.
- REAPER is not installed on this machine, so cross-host listening remains open.
- Build, sign, and validate AUv3 on macOS/Xcode, then test GarageBand and a
  second AUv3 host on physical iPhone/iPad hardware.
- Record Apple Silicon/Intel DSP and WKWebView profiles. None of the Apple,
  Xcode, AUv3-host, signing, or physical-device checks has been completed on
  Windows.
- Build and run the Raspberry Pi 4/5 benchmark matrix, including callback
  p50/p95/p99/max, deadline misses and ARM64/NEON comparisons. Pi performance is
  not yet verified.
- Continue the benchmark-led mode specialization, crossfade and SIMD work in
  `vst3/DSP_OPTIMIZATION_PLAN.md`; the first realtime-safety and O(N) Magnitude
  Diffusion pass is complete, but the full optimization program is not.

## Max for Live prototype implemented

- Editable Max 9 source device with stereo `plugin~` / `plugout~` I/O.
- Classic Hilbert-transform single-sideband shifter with Upper, Lower and Stereo Split directions.
- Spectral scale abstraction using `pfft~` and `gizmo~`, followed by frequency translation.
- 35 ms equal-power engine crossfade and 20 ms equal-power dry/wet smoothing.
- Stereo SVF filtering plus true per-bin Cutoff, Band, Comb, Odd/Even, Gate,
  Freeze, Random Reject and Harmonic Select processing.
- Five-shape LFO and envelope follower routed to Shift, Translate, Scale,
  Pivot or Filter through non-destructive bipolar modulation depth.
- JSUI logarithmic input/output spectrum and waterfall fed by native Jitter FFT matrices.
- Twelve factory-preset definitions and validation/listening checklists.

## Max prototype work still open

- Add three parallel FFT-size instances for runtime Low/Normal/High switching and latency reporting.
- Add a delayed dry path matched to each selected spectral quality.
- Save the source through Live 12's Max Audio Effect editor to produce the final `.amxd` container.

The Max source remains an implementation-grade prototype. The VST3 now passes
the automated build, DSP and pluginval gates; DAW listening and workflow tests
remain before calling v0.1 a release candidate.
