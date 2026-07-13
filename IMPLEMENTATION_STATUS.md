# openFAD FlipShift implementation status

## VST3 implementation

Implemented:

- JUCE/CMake VST3 project in `vst3/` with product name `openFAD FlipShift`.
- VST3 developer/vendor metadata is `UnpureBloom`.
- Spectral-only parameter model matching the requested reference menu.
- STFT engine with Low/Normal/High FFT quality, dry-path latency alignment,
  sample-level smoothing for continuous parameters and stereo-safe processing.
- First-pass spectral modes: Off, Detune +/-, Smear, Spread +/-, Harmonics,
  Subharm, Gate, Robotize, Shift, Mirror, Peak Follow, Peak Oct +/-12,
  Peak Hmx +/-12, Harm Sweep, Shepard, Shepard W, Comb, Pitch Blend,
  Pitch Shift and Phase Twist.
- JUCE editor with input/output spectrum, waterfall, pivot marker and compact
  industrial controls. Waterfall uses time on X, logarithmic frequency on Y,
  and purple/green/yellow energy colour, matching the supplied reference logic.
- Waterfall rendering now advances at 60 Hz by one pixel and interpolates the
  full FFT over the display height for substantially higher visual resolution.
- Dropdowns are compact, inactive mode parameters are disabled, and all modes
  and controls expose hover descriptions in the editor.
- Knob defaults are centred where practical, with nonlinear ranges around the
  musically useful values for Shift, Scale, Pivot, Width/Q and output gain.
- DSP test executable and PowerShell build/pluginval scripts.
- Three static UI previews plus one interactive HTML preview.

Validated locally on Windows:

- Release build uses `D:\VS2022BuildTools` and `D:\JUCE`; the build script sets
  UTF-8 console output so Ninja records MSVC header dependencies correctly.
- Shift, scale, mirror, unity-gain reconstruction, every spectral mode,
  Freeze latching, sample-rate/quality setup, dry-path latency and deterministic
  random mode/quality stress tests pass.
- pluginval 1.0.4 passes strictness level 10 for three randomised repeats with
  seeds `0x1dfb91` and `0x5eed2026`, including editor, processing, automation,
  state, locale, thread-safety, bus-layout and parameter-fuzz tests.
- The verified bundle is installed at
  `C:\Program Files\Common Files\VST3\openFAD FlipShift.vst3`; its binary hash
  matches the Release build and the installed copy passes strictness level 10.
- Bypass is exported as the standard host bypass parameter. Quality, Analyzer
  View and Freeze are intentionally non-automatable; Mode and sound controls
  remain automatable.
- LTO is disabled by default after `/GL` reproducibly triggered heap corruption
  with MSVC 19.44 and JUCE 8.0.12. Normal Release optimisation remains enabled.
- The optional standalone Steinberg VST3 validator is not installed, so that
  pluginval sub-test is skipped.

Remaining release checks:

- Audition and tune each spectral mode in Ableton Live 12 Suite.
- Confirm Mode/Shift/Scale/Amount automation and quality changes by ear.
- Verify preset restore and latency compensation in Ableton Live.
- REAPER is not installed on this machine, so cross-host listening remains open.

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

## Requires the next DSP pass

- Add three parallel FFT-size instances for runtime Low/Normal/High switching and latency reporting.
- Add a delayed dry path matched to each selected spectral quality.
- Save the source through Live 12's Max Audio Effect editor to produce the final `.amxd` container.

The Max source remains an implementation-grade prototype. The VST3 now passes
the automated build, DSP and pluginval gates; DAW listening and workflow tests
remain before calling v0.1 a release candidate.
