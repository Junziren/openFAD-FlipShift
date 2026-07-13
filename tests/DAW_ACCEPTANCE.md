# openFAD FlipShift VST3 DAW acceptance

## Environment

- Product: openFAD FlipShift 0.1.0
- Developer/vendor: UnpureBloom
- Format: VST3 x64
- Installed path: `C:\Program Files\Common Files\VST3\openFAD FlipShift.vst3`
- Primary DAW: Ableton Live 12 Suite

## Automated preflight

- [x] Release build and DSP tests pass with MSVC from `D:\VS2022BuildTools`.
- [x] pluginval 1.0.4 strictness 10 passes two randomised three-repeat seeds.
- [x] Installed VST3 binary matches the verified Release binary.
- [ ] Complete the following listening and workflow checks in Ableton Live.

## Scan and load

- [ ] Live lists the plug-in under developer `UnpureBloom`.
- [ ] A fresh stereo audio track loads the editor without an error or blank view.
- [ ] Closing and reopening the editor does not interrupt audio.
- [ ] Silence in produces silence out in Off, Bypass and every Spectral mode.

## Frequency mapping

- [ ] With `440hz-sine.wav`, Shift `+100 Hz` produces a dominant peak near 540 Hz.
- [ ] With `440hz-sine.wav`, Shift `-100 Hz` produces a dominant peak near 340 Hz.
- [ ] Pitch Shift Scale `0.5x`, Pivot `20 Hz` produces a peak near 220 Hz.
- [ ] Pitch Shift Scale `2.0x`, Pivot `20 Hz` produces a peak near 880 Hz.
- [ ] Mirror with Pivot `1000 Hz` maps the 440 Hz peak near 1560 Hz.

## Sound and automation

- [ ] Every Spectral mode is auditioned with sine, chord, sweep and white noise.
- [ ] Automating Mode, Shift, Scale, Pivot and Amount produces no hard clicks or bursts.
- [ ] Mix at 0%, 50% and 100% maintains the expected dry/wet timing.
- [ ] Freeze holds one spectral frame and resumes live input when released.
- [ ] Bypass remains time-aligned with the compensated track.
- [ ] Low, Normal and High quality changes are checked during stopped and active playback.

## Analyzer and state

- [ ] Spectrum, Waterfall and Both views display correctly.
- [ ] Waterfall X axis advances in time, Y axis rises in frequency and colour tracks energy.
- [ ] Pivot and transform guides follow the current parameter values.
- [ ] Closing the editor does not change audio or CPU materially.
- [ ] Save/reopen the Live Set restores all parameters and automation.
- [ ] Duplicate, freeze/flatten and offline export produce repeatable audio.

## Sample-rate matrix

- [ ] 44.1 kHz
- [ ] 48 kHz
- [ ] 88.2 kHz
- [ ] 96 kHz

Record audible defects with the mode, quality, sample rate, buffer size and exact parameter values.
