# Full DSP optimization plan

This document is both the optimization plan and its status record. The first
realtime-safety and low-risk scalar optimization pass was completed on Windows
on 2026-07-20. It preserves the product parameter model, while changing
implementation details including analyzer transport and the Magnitude
Diffusion algorithm's complexity. Raspberry Pi and Apple performance results
remain unverified.

## Target and release gates

Treat a Raspberry Pi 4 as the minimum performance class and a Raspberry Pi 5
as the normal ARM development target. The primary realtime gate is stereo,
48 kHz, 128-sample blocks, Normal quality, with no deadline misses during a
30-minute randomized mode/automation run. Low quality should remain usable at
64-sample blocks. High quality may use a 256-sample minimum block on Pi 4, but
must remain stable on Pi 5.

Every optimization must meet all of these conditions:

- No heap allocation, lock acquisition, file access, logging, or host latency
  mutation on the audio thread.
- Existing deterministic DSP tests pass within explicitly recorded numerical
  tolerances; spectral peak locations and unity reconstruction cannot drift
  silently.
- Release builds are measured for median, p95, p99, and maximum callback time,
  not only average CPU.
- Output is checked for NaN/Inf, denormals, discontinuities, excess gain, and
  state-restore differences at 44.1, 48, 88.2, 96, and 192 kHz.
- x86-64 scalar/SIMD and ARM64 NEON paths produce perceptually and numerically
  equivalent results.

## Phase 0: reproducible baseline

1. Add an offline benchmark executable that runs every mode for fixed audio,
   silence, impulses, noise, and dense music-like material across FFT sizes and
   block sizes. Record samples/second, callback percentiles, allocations, and
   maximum resident memory as JSON artifacts.
2. Add realtime stress harnesses for rapid mode changes, quality changes,
   freeze toggles, parameter ramps, editor open/close, and analyzer enabled or
   disabled.
3. Capture profiles with Windows Performance Recorder/Visual Studio, macOS
   Instruments, Linux `perf`, and ARM PMU counters. Separate FFT cost, spectral
   mode cost, ring-buffer movement, analyzer publishing, and parameter work.
4. Save reference WAVs and per-frame complex spectra for all 24 modes. Include
   Glitch selected-band/out-of-band references and Pitch Map coverage for
   all 12 roots in Major and Minor, using the natural-minor interval set.
   Define tolerances before changing code.

## Phase 1: realtime safety before speed

- [x] Move `configureQuality()` and `setLatencySamples()` out of the audio
  callback. Quality changes are handled asynchronously, preserving the host's
  prior suspended state while waiting for the callback lock before rebuilding.
- [x] Use one lock order, callback lock followed by engine configuration lock,
  for prepare, release, explicit reset, and quality reconfiguration.
- [x] Replace the analyzer `SpinLock` with an SPSC fixed three-buffer exchange,
  atomic sequence publication, and generation checks that reject stale frames.
- [x] Allocate all `SpectralFrameMemory` arrays during prepare; reset clears
  runtime history without releasing capacity.
- [x] Disable analyzer work when no Editor consumer exists and limit publication
  to approximately 15 Hz. Temporary Editor hiding does not disable production;
  the consumer remains active until Editor destruction.
- [x] Add finite/range guards for sample rate, parameters, input, output, and
  WebView-normalized parameter events.
- [ ] Add automated allocation hooks around the audio callback and fail tests
  on any allocation after `prepareToPlay()`.
- [ ] Prebuild all three quality states and add a measured transition crossfade;
  the current asynchronous rebuild is realtime-safe but may still pause briefly.

## Phase 2: remove avoidable inner-loop work

- [x] Cache channel write pointers once per block.
- [x] Replace ring-position modulo operations with a power-of-two mask.
- [x] Smooth output gain in the linear domain, removing per-sample dB-to-linear
  conversion.
- [x] Precompute per-bin phase advance and wrap long-running phase accumulators.
- [x] Run peak search, Gate work, and phase-history maintenance
  only for modes that require them; peak-dependent modes read the frozen
  spectrum while Freeze is active.
- [ ] Split wrapped copies into contiguous spans where benchmarks show value.
- [ ] Advance additional smoothers at frame/block granularity only after
  listening and automation tests establish equivalence.
- [ ] Precompute more mode-specific frequency grids and avoid redundant FFT
  buffer clearing where JUCE's packed layout permits it.

## Phase 3: spectral-mode specialization

1. Split `applySpectralMode()` into mode-specific kernels selected once per
   frame. Remove the large per-bin switch and flags that are constant for the
   whole frame.
2. Create fast paths for Off, bypass, zero Amount, unity Scale, zero Shift,
   and unchanged Freeze. Off should reuse the input spectrum without
   remapping work.
3. Peak work is now conditional by mode; further reuse of squared magnitudes is
   still pending.
4. Replace repeated `pow`, `log2`, `sin`, `cos`, `atan2`, `polar`, and `exp`
   calls with frame-level constants, recurrence relations, bounded lookup
   tables, or measured polynomial approximations. Each approximation needs an
   error bound and listening/aliasing tests.
5. [x] Smear now uses prefix sums, reducing wide-radius work from
   O(N * radius) to O(N).
6. Precompute Harmonics, Harm Sweep and Oct Stack masks when their
   controlling parameters are stable, then interpolate or rebuild only after a
   meaningful parameter delta.
7. Review remap modes for gather/scatter locality. Prefer sequential source
   index tables and interpolation weights generated once per frame.
8. [x] Pitch Map precomputes source-to-destination bins and interpolation
   weights, rebuilding only when root, scale, sample rate or FFT configuration
   changes. Keep steady-state processing free of allocation, locks and per-bin
   `log2`/`pow`.
9. [x] Glitch uses deterministic hashes and bounded frame-held epochs
   rather than mutable RNG state. Preserve stereo consistency and exact
   out-of-band identity while benchmarking hash cost at Pi-class FFT sizes.

## Phase 4: data layout and SIMD

1. Benchmark array-of-complex against split real/imaginary arrays. Use the
   layout that minimizes packing around JUCE FFT and enables contiguous SIMD.
2. Add JUCE SIMD or explicit compiler-vectorizable loops for windowing,
   magnitude, dry/wet mixing, gain, overlap-add, and simple masks.
3. Add ARM64 NEON kernels only after scalar loops and data alignment are clean.
   Keep a portable scalar reference path for tests and unsupported CPUs.
4. Align hot buffers to at least 32 bytes, keep channel state contiguous, and
   reduce simultaneous working-set size so Low/Normal processing stays in L1/L2
   cache on Pi-class CPUs.
5. Compare JUCE FFT with ARM-friendly alternatives only under a license and
   packaging review. A replacement must beat end-to-end callback time, not just
   an isolated FFT microbenchmark.

## Phase 5: adaptive operation and product policy

1. [x] Analyzer production is conditional on Editor lifetime. Hiding the editor
   only pauses frontend animation hints; native production remains enabled until
   Editor destruction. View-specific analyzer reductions remain a future
   profiling option.
2. Add an optional Eco policy for ARM/mobile builds: lower analyzer rate,
   capped display bins, and a conservative default FFT quality. Do not change
   audible processing without an explicit user-facing quality choice.
3. Consider a CPU guard that reports overload and recommends a lower quality;
   automatic quality changes are allowed only if latency and sound changes are
   host-safe and clearly communicated.
4. Measure WebView GPU/CPU/memory separately from DSP so UI optimization never
   masks an audio-thread regression.

## Delivery order

The realtime-safety portion of Phase 1 and the listed scalar items in Phases 2,
3, and 5 are complete. Phase 0 benchmark artifacts and allocation hooks remain
the next gate before quality-state prebuilding, broader mode specialization,
SIMD, or NEON work. Those later changes must still land in small benchmarked
steps with reference renders.

## 2026-07-20 Windows validation record

- Release build and CTest pass 2/2, covering the DSP executable and the real
  WebView2 GUI integration executable.
- DSP coverage includes 44.1-192 kHz, irregular block sizes from 0 to 4096,
  finite-value isolation, analyzer concurrency/generation behavior, long phase
  stability, Freeze memory, and randomized mode/quality stress.
- Glitch coverage verifies deterministic replay, stereo consistency,
  epoch changes and per-frame out-of-band identity. Pitch Map coverage verifies
  expected dominant-frequency mapping for C/G Major and Minor using the
  natural-minor interval set.
- The GUI integration test uses the same Processor/Editor/WebUI sources,
  deterministic audio, and the actual WebView2 DOM. A representative run
  reports 46 analyzer events, 190 waterfall columns, 192+192 points and -10.5 dB
  meters. It does not load the installed VST3 or exercise Ableton wrapper/cache.
- The current Release and system-installed bundles share SHA-256
  `345B61AB31D6B7575712065F7D51B16845479C4A5B8DD60D08A77260214EC211`.
  The installed copy passes pluginval 1.0.4 strictness 10 with `Repeat=3`.
- No Apple/Xcode/AUv3 device result or Raspberry Pi 4/5 benchmark has been
  produced; neither platform may be described as validated.

The optimization effort is complete only when benchmark artifacts, numerical
comparisons, realtime stress logs, pluginval/AU validation, and Pi 4/Pi 5 test
results are reproducible in CI or documented hardware runs.
