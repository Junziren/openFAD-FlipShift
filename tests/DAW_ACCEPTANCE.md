# openFAD FlipShift VST3 DAW acceptance

## Environment

- Product: openFAD FlipShift 0.1.0
- Developer/vendor: UnpureBloom
- Format: VST3 x64
- Installed path: `C:\Program Files\Common Files\VST3\openFAD FlipShift.vst3`
- Primary DAW: Ableton Live 12 Suite

## Automated preflight

- [x] Release build and CTest pass 2/2 with MSVC from `D:\VS2022BuildTools`: DSP tests plus the real WebView2 GUI integration test.
- [x] The GUI integration test uses the same Processor/Editor/WebUI sources, deterministic audio and the actual DOM; a representative run records 46 analyzerFrame events, 190 waterfall columns, 192+192 points and -10.5 dB meters.
- [x] `tests/validate-webui.py` passes at 1000x650, 621x844, 390x844 and 320x280, asserts waterfall pixels and meter fills/readouts, and produces screenshots.
- [x] The current 24-transform Release is installed at `C:\Program Files\Common Files\VST3\openFAD FlipShift.vst3`.
- [x] The Release and installed bundles share SHA-256 `9B0D45FD15D651DE5CF0603C1934C4A0242D4414D8893EDA1F64989D2A584B0C`.
- [x] The newly installed system copy passes pluginval 1.0.4 strictness level 10 with `Repeat=3`.
- [ ] The GUI integration executable does not load the installed VST3 or cover Ableton's wrapper/cache; complete the following DAW checks separately.
- [ ] Complete the following listening and workflow checks in Ableton Live.

## Scan and load

- [ ] Fully quit Live before installing a rebuilt bundle, then reopen Live.
- [ ] In Preferences > Plug-Ins, hold Alt while clicking Rescan to force a deep
  rescan when the plug-in version number has not changed.
- [ ] Live lists the plug-in under developer `UnpureBloom`.
- [ ] A fresh stereo audio track loads the editor without an error or blank view.
- [ ] Closing and reopening the editor does not interrupt audio.
- [ ] The editor behaves as a plug-in surface: no document-level scrolling, text selection, context menu, browser drag/drop, internal navigation, or refresh/source/location/zoom shortcuts.
- [ ] At 320x280 the ANALYZE/CONTROL tabs keep the root fixed; only the explicit parameter tool region may scroll.
- [ ] Hiding the editor pauses the WebView main animation, gestures and knob particles but leaves analyzer production active; destroying the Editor disables the analyzer consumer. Reopening preserves history and does not alter parameters.
- [ ] Silence in produces silence out in Neutral, Bypass and every Spectral mode.

## Mode guidance

- [ ] All 24 PROCESS values expose a non-empty effect description that matches the selected mode.
- [ ] PROCESS lists the behavior-based names Neutral, Pivot Bend, Magnitude Diffusion, Stereo Translate, Harmonic Sieve, Spectral Divider, Peak-Relative Gate, Phase Reset, Hz Translation, Pivot Reflection, Peak Repel, Peak Stretch 2x, Peak Compress 2:1, Peak Stretch, Peak Compress, Harmonic Scan, Octave Stack Tight, Octave Stack Wide, Magnitude Comb, Ratio Crossfade, Phase-Tracked Scale, Phase Ripple, Band Glitch and Pitch Map in that order.
- [ ] The selected effect description updates after mouse selection, keyboard selection, host automation and Live Set restore.
- [ ] Hovering or focusing PROCESS shows the description without resizing the top bar, clipping text or covering the select and adjacent controls.
- [ ] Shift, Scale, Pivot, Amount and Width/Q tooltips follow the selected mode-specific meaning while inactive controls remain visible and disabled.

## Interaction and About

- [ ] Pointer drag, keyboard adjustment and double-click reset produce a short, restrained module-coloured knob trail without obscuring the pointer, focus outline, label or value.
- [ ] Continuous fast dragging never exceeds the fixed 24-particle pool, and no knob particle animation frame remains active after the trail fades.
- [ ] Host automation playback, initial parameter state and Live Set restore update knobs without producing particles or changing automation gesture order.
- [ ] With reduced motion enabled, knob particles never appear and any active trail is cleared immediately.
- [ ] About contains openFAD, UnpureBloom, source repository and main-feature sections; its license section says FlipShift is not yet licensed, identifies GNU AGPLv3 only as the upstream openFAD license, and says the JUCE AGPLv3/commercial route is still undeclared. Tab stays inside the modal, Escape closes it and focus returns to the About button.
- [ ] In the VST3 editor, every About link opens its exact allow-listed URL in the system browser without navigating the embedded WebView.

## Frequency mapping

- [ ] With `440hz-sine.wav`, Hz Translation `+100 Hz` produces a dominant peak near 540 Hz.
- [ ] With `440hz-sine.wav`, Hz Translation `-100 Hz` produces a dominant peak near 340 Hz.
- [ ] Phase-Tracked Scale `0.5x`, Pivot `20 Hz` produces a peak near 220 Hz.
- [ ] Phase-Tracked Scale `2.0x`, Pivot `20 Hz` produces a peak near 880 Hz.
- [ ] Pivot Reflection with Pivot `1000 Hz` maps the 440 Hz peak near 1560 Hz.

## Band Glitch and Pitch Map

- [ ] Band Glitch labels Pivot/Width Q as Band Center/Band Q, Shift as Offset and Amount as Density; the selected-band overlay follows those values without recolouring, clearing or recomputing waterfall history.
- [ ] With Density at 0%, Band Glitch is neutral. Raising Density increases event probability, wet depth and refresh cadence without changing the selected frequency bounds.
- [ ] Replaying identical input and restored parameters produces the same Band Glitch event sequence, left/right channels remain coherent, successive held epochs vary, and bins outside the selected band remain unchanged.
- [ ] Pitch Map exposes all roots C through B and Major/Minor, with Minor using natural-minor intervals; Root, Scale and Map Depth survive automation and Live Set restore.
- [ ] At 100% Map Depth, an E4 tone remains near 329.6 Hz in C Major and maps near E-flat4/311.1 Hz in C Minor; at 0% it remains unchanged in both scales.
- [ ] Pitch Map conserves practical output energy without bursts, and dense chords do not create non-finite output or unstable level jumps while Root, Scale or Map Depth changes.

## Sound and automation

- [ ] Every Spectral mode is auditioned with sine, chord, sweep and white noise.
- [ ] Automating Mode, Shift, Scale, Pivot, Amount, Pitch Root and Pitch Scale produces no hard clicks or bursts.
- [ ] Mix at 0%, 50% and 100% maintains the expected dry/wet timing.
- [ ] Freeze holds one spectral frame and resumes live input when released.
- [ ] Bypass remains time-aligned with the compensated track.
- [ ] Low, Normal and High quality changes are checked during stopped and active playback.

## Analyzer and state

- [ ] WF and FFT layer switches reproduce Spectrum, Waterfall and Both views and never allow both layers to be hidden.
- [ ] Waterfall X axis advances in time, Y axis rises in frequency, high-energy bins reach near-white and narrow peaks remain visually distinct.
- [ ] Waterfall 1x/2x/4x/8x speeds are visibly different at approximately 15/30/60/120 visual columns per second without changing audio, latency, the native 15 Hz analyzer transfer or the host parameter list.
- [ ] Changing any DSP parameter preserves previous waterfall history; only subsequent real analyzer output may change future heat-map energy.
- [ ] Parameter markers appear only in the dedicated event track and the short-lived readout stays in the toolbar; neither overlaps, tints, masks, recolours nor redraws heat-map pixels.
- [ ] VISION, PULSE, PHOSPHOR and XRAY are visually distinct, and every LOOK keeps low energy dark, high energy bright and narrow peaks crisp without interpolation blur.
- [ ] Switching LOOK recolours the existing visible history without changing its peak positions, time alignment, event markers, history count or advance count.
- [ ] Switching LOOK does not change audio, latency, analyzer transfer rate, scroll speed, layout, host automation or the host parameter list.
- [ ] While HOLD is active, switching LOOK recolours stored history but does not add a new history column or move the event timeline.
- [ ] SPEED, LOOK and LAYOUT remain local WebView controls and never create parameter markers, toolbar parameter readouts or host automation gestures.
- [ ] STACK, OVERLAY and SPLIT preserve the same history; SPLIT falls back to a vertical 58/42 layout at 620px and below.
- [ ] Browser preview in every LOOK and at every speed has no regular moving diagonal bands, dark-blue slanted stripes or visible refresh boundary.
- [ ] Pivot and transform guides follow the current parameter values.
- [ ] Band Glitch uses a separate selected-band DOM overlay and Pitch Map uses a text key readout; neither writes synthetic energy into the analyzer Canvas or changes stored waterfall data.
- [ ] Closing the editor does not change audio or CPU materially.
- [ ] Save/reopen the Live Set restores all parameters and automation.
- [ ] Duplicate, freeze/flatten and offline export produce repeatable audio.
- [ ] At 1000x650, 621x844, 390x844 and 320x280, PROCESS help and the LOOK selector have no clipping, overlap or horizontal overflow, and the document root never scrolls.

## Sample-rate matrix

- [ ] 44.1 kHz
- [ ] 48 kHz
- [ ] 88.2 kHz
- [ ] 96 kHz

Record audible defects with the mode, quality, sample rate, buffer size and exact parameter values.
