(() => {
  "use strict";

  const t = (text) => window.FlipShiftI18n.t(text);

  const modeNames = [
    "Off", "Bend", "Smear", "Spread", "Harmonics", "Subharm",
    "Gate", "Zero Phase", "Shift", "Mirror", "Peak Push", "Peak x2",
    "Peak /2", "Peak Expand", "Peak Compress", "Harm Sweep",
    "Oct Stack", "Wide Oct Stack", "Comb", "Pitch Blend",
    "Spectral Scale", "Phase Ripple", "Glitch", "Pitch Map"
  ];

  const pitchRootNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const pitchScaleNames = ["MAJOR", "MINOR"];
  const previewMajorScale = [true, false, true, false, true, true, false, true, false, true, false, true];
  const previewMinorScale = [true, false, true, true, false, true, false, true, true, false, true, false];

  const defaultControlTooltips = {
    shiftHz: "Additive frequency offset used by this spectral transform.",
    scale: "Frequency ratio around the selected pivot or detected peak.",
    pivotHz: "Frequency reference used as a centre, anchor or fundamental.",
    amount: "Depth of the selected spectral transformation.",
    widthQ: "Bandwidth, radius or selectivity of the transformation."
  };

  const modes = [
    {
      description: "Leaves the spectral frame unchanged; Freeze and the global Mix path still apply."
    },
    {
      shift: "BEND", pivot: "CENTER", uses: ["shiftHz", "pivotHz", "amount"],
      description: "Expands or contracts frequency distance around Center by a limited ratio; Amount also blends the transformed spectrum.",
      tooltips: {
        shiftHz: "Maximum pivot-centred expansion or contraction; positive values expand and negative values contract.",
        pivotHz: "Frequency that remains fixed while surrounding frequencies bend.",
        amount: "Increase the bend ratio and blend toward the bent spectrum."
      }
    },
    {
      amount: "DIFFUSION", width: "RADIUS", uses: ["amount", "widthQ"],
      description: "Averages neighbouring bin magnitudes, smooths them over time, and retains each bin's current phase.",
      tooltips: {
        amount: "Blend toward the diffused spectrum and increase its averaging range.",
        widthQ: "Radius of neighbouring bins included in the magnitude average."
      }
    },
    {
      shift: "OFFSET", uses: ["shiftHz", "amount"],
      description: "Translates the left and right spectra by equal Hz offsets in opposite directions; mono input uses one translation direction.",
      tooltips: { shiftHz: "Maximum opposite Hz translation applied to the two channels." }
    },
    {
      pivot: "FUNDAMENTAL", amount: "REJECTION", width: "BAND WIDTH", uses: ["pivotHz", "amount", "widthQ"],
      description: "Retains bands near integer multiples of Fundamental and attenuates the gaps without boosting them.",
      tooltips: {
        pivotHz: "Fundamental frequency used to construct the harmonic grid.",
        amount: "Increase attenuation between the retained harmonic bands.",
        widthQ: "Width of each band retained around the harmonic grid."
      }
    },
    {
      amount: "DIVISOR", uses: ["amount"],
      description: "Reads stepped 1x to 4x source frequencies, dividing spectral content downward by integer ratios.",
      tooltips: { amount: "Select the integer frequency divisor and blend it with the original spectrum." }
    },
    {
      amount: "THRESHOLD", uses: ["amount"],
      description: "Derives a threshold from the strongest spectral bin and attenuates bins below it.",
      tooltips: { amount: "Raise both the spectral gate threshold and the reduction below it." }
    },
    {
      amount: "RESET", uses: ["amount"],
      description: "Sets the transformed branch to zero phase, then blends it with the original spectrum.",
      tooltips: { amount: "Blend toward the zero-phase spectrum." }
    },
    {
      shift: "OFFSET Hz", uses: ["shiftHz", "amount"],
      description: "Adds the same signed Hz offset to every frequency component, bending harmonic ratios.",
      tooltips: { shiftHz: "Frequency added to or subtracted from every spectral bin." }
    },
    {
      scale: "MAP SCALE", pivot: "REFLECT AXIS", uses: ["scale", "pivotHz", "amount"],
      description: "Reflects frequency order around Reflect Axis; larger Map Scale values compress the audible output toward the axis.",
      tooltips: {
        scale: "Source-map slope; larger values compress output distances toward Reflect Axis.",
        pivotHz: "Frequency axis around which the spectrum is reversed."
      }
    },
    {
      pivot: "ANCHOR", amount: "REPEL", uses: ["pivotHz", "amount"],
      description: "Detects the strongest bin and moves the full spectrum farther from Anchor by the peak-to-anchor offset.",
      tooltips: {
        pivotHz: "Reference frequency that the detected dominant peak moves away from.",
        amount: "Increase the peak-relative displacement and blend toward the repelled spectrum."
      }
    },
    {
      amount: "BLEND", uses: ["amount"],
      description: "Keeps the detected peak fixed and doubles every component's linear Hz distance from it.",
      tooltips: { amount: "Blend toward the fixed 2x peak-relative stretch." }
    },
    {
      amount: "BLEND", uses: ["amount"],
      description: "Keeps the detected peak fixed and halves every component's linear Hz distance from it.",
      tooltips: { amount: "Blend toward the fixed 2:1 peak-relative compression." }
    },
    {
      scale: "SPAN", amount: "BLEND", uses: ["scale", "amount"],
      description: "Keeps the strongest peak fixed and expands linear Hz distances by the Span control plus one.",
      tooltips: { scale: "Expansion control; the applied peak-relative factor is Span plus one." }
    },
    {
      scale: "SPAN", amount: "BLEND", uses: ["scale", "amount"],
      description: "Keeps the strongest peak fixed and divides linear Hz distances by the Span control plus one.",
      tooltips: { scale: "Compression control; peak-relative distances are divided by Span plus one." }
    },
    {
      shift: "SCAN", pivot: "FUNDAMENTAL", width: "BAND WIDTH", uses: ["shiftHz", "pivotHz", "amount", "widthQ"],
      description: "Retains bands near integer multiples of a fundamental transposed by Scan in cents.",
      tooltips: {
        shiftHz: "Harmonic-grid offset in cents; small values provide fine movement.",
        pivotHz: "Base fundamental frequency for the moving harmonic grid.",
        widthQ: "Width of each retained band in the harmonic grid."
      }
    },
    {
      pivot: "OCTAVE ANCHOR", amount: "OCTAVE POSITION", uses: ["pivotHz", "amount"],
      description: "Sums seven octave-related resamplings through a tight octave-periodic window.",
      tooltips: {
        pivotHz: "Frequency anchor for the repeating octave window.",
        amount: "Move the seven-layer stack through one octave of resampling position."
      }
    },
    {
      pivot: "OCTAVE ANCHOR", amount: "OCTAVE POSITION", uses: ["pivotHz", "amount"],
      description: "Sums seven octave-related resamplings through a broad octave-periodic window.",
      tooltips: {
        pivotHz: "Frequency anchor for the repeating octave window.",
        amount: "Move the seven-layer stack through one octave of resampling position."
      }
    },
    {
      shift: "SPACING", pivot: "BASE", amount: "MASK DEPTH", uses: ["shiftHz", "pivotHz", "amount"],
      description: "Applies a cosine comb to spectral magnitudes. Tooth spacing is the absolute Spacing value plus ten percent of Base.",
      tooltips: {
        shiftHz: "Unsigned spacing contribution; positive and negative values produce the same spacing.",
        pivotHz: "Base frequency contributing ten percent of the mask spacing."
      }
    },
    {
      scale: "RATIO", pivot: "PIVOT", amount: "CROSSFADE", uses: ["scale", "pivotHz", "amount"],
      description: "Crossfades complex bins between the original and pivot-scaled spectra without phase tracking.",
      tooltips: { amount: "Crossfade the complex spectrum; intermediate values can produce phase cancellation." }
    },
    {
      scale: "SCALE", pivot: "PIVOT", amount: "BLEND", uses: ["scale", "pivotHz", "amount"],
      description: "Rescales frequency distance around Pivot and applies phase tracking to remapped bins.",
      tooltips: { scale: "Frequency-distance scale around Pivot; this is not a whole-spectrum pitch ratio." }
    },
    {
      pivot: "RIPPLE SCALE", amount: "ROTATION", uses: ["pivotHz", "amount"],
      description: "Applies sinusoidal frequency-dependent phase rotation; partial blending can also alter magnitude.",
      tooltips: {
        pivotHz: "Frequency scale of the sinusoidal phase ripple; one full cycle spans about 2 pi times this value.",
        amount: "Increase phase rotation and blend toward the rotated spectrum."
      }
    },
    {
      shift: "OFFSET", pivot: "BAND CENTER", amount: "DENSITY", width: "BAND Q",
      uses: ["shiftHz", "pivotHz", "amount", "widthQ"],
      description: "Randomly offsets spectral content only inside the band selected by Band Center and Band Q; Density sets event probability and wet depth.",
      tooltips: {
        shiftHz: "Signed source-bin offset applied when a selected spectral region glitches.",
        pivotHz: "Centre frequency of the spectral band eligible for glitch processing.",
        amount: "Probability and wet depth of random glitch events inside the selected band.",
        widthQ: "Quality factor for the selected band; higher values make the band narrower."
      }
    },
    {
      amount: "MAP DEPTH", uses: ["amount", "pitchRoot", "pitchScale"],
      description: "Quantizes spectral energy toward the nearest pitch classes in the selected major or minor key. Map Depth blends the original and quantized spectra.",
      tooltips: {
        amount: "Blend between the original spectrum and the scale-quantized pitch map."
      }
    }
  ];

  const skewForCentre = (start, end, centre) => Math.log(0.5) / Math.log((centre - start) / (end - start));
  const specs = {
    shiftHz: { start: -5000, end: 5000, skew: 0.45, symmetric: true, defaultValue: 0 },
    scale: { start: 0.25, end: 4, skew: skewForCentre(0.25, 4, 1), defaultValue: 1 },
    pivotHz: { start: 20, end: 20000, skew: skewForCentre(20, 20000, 1000), defaultValue: 1000 },
    amount: { start: 0, end: 1, skew: 1, defaultValue: 0.5 },
    widthQ: { start: 0.05, end: 8, skew: skewForCentre(0.05, 8, 1), defaultValue: 1 },
    mix: { start: 0, end: 1, skew: 1, defaultValue: 0.5 },
    outputGainDb: { start: -24, end: 12, skew: skewForCentre(-24, 12, 0), defaultValue: 0 }
  };

  const state = {
    mode: 8,
    shiftHz: 0,
    scale: 1,
    pivotHz: 1000,
    amount: 0.5,
    widthQ: 1,
    pitchRoot: 0,
    pitchScale: 0,
    mix: 0.5,
    outputGainDb: 0,
    quality: 2,
    analyzerView: 2,
    bypass: 0,
    freeze: 0,
    sampleRate: 48000
  };

  const pointCount = 1024;
  const waterfallHistoryCapacity = 4096;
  const analyzerData = {
    input: new Float32Array(pointCount),
    output: new Float32Array(pointCount)
  };
  const waterfallData = new Float32Array(pointCount).fill(-96);
  let receivedDedicatedWaterfall = false;
  let lastWaterfallFrameTime = null;
  const previewPitchMapEnergy = new Float32Array(pointCount);
  const waterfallHistory = new Uint8Array(waterfallHistoryCapacity * pointCount);
  const waterfallHistoryMarkers = new Uint8Array(waterfallHistoryCapacity);
  const waterfallHistoryRowScratch = new Uint8Array(pointCount);
  analyzerData.input.fill(-96);
  analyzerData.output.fill(-96);

  const backend = window.__JUCE__ && window.__JUCE__.backend;
  const listenerTokens = [];
  const activeGestures = new Set();
  const activePointerDrags = new Map();
  const reducedMotionQuery = window.matchMedia("(prefers-reduced-motion: reduce)");
  const compactWorkspaceQuery = window.matchMedia("(max-width: 820px) and (max-height: 540px)");

  const rootElement = document.documentElement;
  const analyzer = document.getElementById("analyzer");
  const parameterStrip = document.getElementById("parameterStrip");
  const compactViewTabs = document.getElementById("compactViewTabs");
  const compactViewButtons = Array.from(compactViewTabs.querySelectorAll("[data-compact-view]"));
  const waterfallCanvas = document.getElementById("waterfallCanvas");
  const waterfallEventCanvas = document.getElementById("waterfallEventCanvas");
  const spectrumCanvas = document.getElementById("spectrumCanvas");
  const pivotGuide = document.getElementById("pivotGuide");
  const destinationGuide = document.getElementById("destinationGuide");
  const glitchSelection = document.getElementById("glitchSelection");
  const glitchSelectionLabel = document.getElementById("glitchSelectionLabel");
  const pitchMapReadout = document.getElementById("pitchMapReadout");
  const axisModule = document.getElementById("axisModule");
  const pitchMapModule = document.getElementById("pitchMapModule");
  const modeSelect = document.getElementById("mode");
  const modeSelectContainer = modeSelect.closest(".mode-select");
  const modeEffectDescription = document.getElementById("modeEffectDescription");
  const waterfallRateSelect = document.getElementById("waterfallRate");
  const waterfallVisualSelect = document.getElementById("waterfallVisual");
  const analyzerLayoutSelect = document.getElementById("analyzerLayout");
  const waterfallChangeReadout = document.getElementById("waterfallChangeReadout");
  const inputMeter = document.getElementById("inputMeter");
  const outputMeter = document.getElementById("outputMeter");
  const aboutOpenButton = document.getElementById("aboutOpen");
  const aboutDialog = document.getElementById("aboutDialog");
  const aboutPanel = aboutDialog.querySelector(".about-panel");
  const aboutCloseButton = document.getElementById("aboutClose");
  const knobTrailCanvas = document.createElement("canvas");
  const knobTrailContext = knobTrailCanvas.getContext("2d", { alpha: true });
  const knobTrailParticles = Array.from({ length: 24 }, () => ({
    active: false,
    age: 0,
    life: 0,
    angle: 0,
    angularVelocity: 0,
    radius: 0,
    radialVelocity: 0,
    size: 0
  }));
  knobTrailCanvas.className = "knob-trail-canvas";
  knobTrailCanvas.setAttribute("aria-hidden", "true");
  knobTrailCanvas.dataset.activeParticles = "0";
  knobTrailCanvas.dataset.spawnCount = "0";

  let waterfallContext = null;
  let waterfallEventContext = null;
  let spectrumContext = null;
  let waterfallInitialised = false;
  let waterfallEventInitialised = false;
  let waterfallSpeedMultiplier = 4;
  let waterfallVisualProfile = "vision";
  let waterfallAdvanceCount = 0;
  let waterfallFullRenderCount = 0;
  let waterfallHistoryHead = 0;
  let waterfallHistoryCount = 0;
  let waterfallAdvanceAccumulator = 0;
  let lastWaterfallAnimationTime = null;
  let lastAnalyzerFrameTime = null;
  let lastPreviewUpdate = 0;
  let pendingWaterfallMarker = 0;
  let waterfallReadoutTimer = 0;
  let receivedInitialParameterState = false;
  let inputMeterDb = -60;
  let outputMeterDb = -60;
  let inputPeakDb = -60;
  let outputPeakDb = -60;
  let inputPeakHoldUntil = 0;
  let outputPeakHoldUntil = 0;
  let knobTrailStage = null;
  let knobTrailPointerId = null;
  let knobTrailFrame = 0;
  let knobTrailLastFrameTime = 0;
  let knobTrailLastEmitTime = 0;
  let knobTrailLastNormalised = 0;
  let knobTrailPoolCursor = 0;
  let knobTrailSpawnCount = 0;
  let knobTrailCentre = 38;
  let knobTrailRadius = 27;
  let knobTrailColour = "#d49a37";
  let aboutCloseTimer = 0;
  let aboutPreviousFocus = null;
  let compactView = rootElement.dataset.compactView === "control" ? "control" : "analyze";
  let nativeSurfaceVisible = true;
  let animationFrameHandle = 0;
  const heatPixelScratch = new Uint8ClampedArray(4);
  const waterfallMarkerPalette = [
    null,
    "#d49a37",
    "#d45d73",
    "#a7798b",
    "#5db6bf",
    "#6ac887",
    "#ecffe4"
  ];
  const waterfallMarkerByParameter = {
    mode: 1,
    shiftHz: 1,
    scale: 1,
    pivotHz: 2,
    widthQ: 2,
    pitchRoot: 6,
    pitchScale: 6,
    amount: 3,
    mix: 4,
    outputGainDb: 5,
    quality: 6,
    bypass: 2
  };
  const waterfallVisualProfiles = {
    vision: {
      label: "VISION",
      description: "High-contrast violet, cyan and mint map with clear near-white peak cores.",
      floorDb: -90,
      ceilingDb: 0,
      gamma: 1.25,
      stops: [
        [0.00, [2, 3, 8]],
        [0.16, [13, 11, 43]],
        [0.34, [57, 44, 130]],
        [0.52, [49, 95, 157]],
        [0.68, [18, 179, 194]],
        [0.80, [114, 229, 183]],
        [0.88, [236, 255, 228]],
        [1.00, [255, 246, 231]]
      ]
    },
    pulse: {
      label: "PULSE",
      description: "Plum, coral and amber thermal map tuned for transients and harmonic peaks.",
      floorDb: -88,
      ceilingDb: 0,
      gamma: 1.15,
      stops: [
        [0.00, [4, 3, 7]],
        [0.16, [26, 11, 44]],
        [0.34, [91, 22, 79]],
        [0.52, [167, 44, 99]],
        [0.68, [232, 91, 82]],
        [0.80, [244, 166, 65]],
        [0.90, [255, 230, 163]],
        [1.00, [255, 251, 234]]
      ]
    },
    phosphor: {
      label: "PHOSPHOR",
      description: "Cyan and green phosphor map that keeps sustained partials and sidebands readable.",
      floorDb: -90,
      ceilingDb: -3,
      gamma: 1.20,
      stops: [
        [0.00, [2, 6, 6]],
        [0.18, [6, 35, 40]],
        [0.36, [10, 83, 96]],
        [0.54, [15, 143, 131]],
        [0.70, [63, 203, 128]],
        [0.82, [168, 232, 114]],
        [0.91, [235, 255, 208]],
        [1.00, [255, 255, 255]]
      ]
    },
    xray: {
      label: "XRAY",
      description: "Colour-neutral diagnostic map for comparing noise floor, shape and peak intensity.",
      floorDb: -84,
      ceilingDb: -3,
      gamma: 0.95,
      stops: [
        [0.00, [2, 4, 5]],
        [0.18, [17, 24, 32]],
        [0.36, [38, 55, 70]],
        [0.54, [75, 105, 123]],
        [0.70, [126, 159, 170]],
        [0.84, [189, 212, 216]],
        [0.93, [237, 247, 245]],
        [1.00, [255, 255, 255]]
      ]
    }
  };
  const waterfallColourTables = new Map();

  modeNames.forEach((name, index) => {
    const option = document.createElement("option");
    const description = modes[index] && modes[index].description ? modes[index].description : name;
    option.value = String(index);
    option.textContent = name.toUpperCase();
    option.title = description;
    option.dataset.description = description;
    modeSelect.appendChild(option);
  });

  Object.entries(waterfallVisualProfiles).forEach(([id, profile]) => {
    const option = waterfallVisualSelect.querySelector(`option[value="${id}"]`);
    if (!option) return;
    option.title = profile.description;
    option.dataset.description = profile.description;
  });

  rootElement.dataset.surface = backend ? "native" : "preview";
  rootElement.dataset.surfaceVisible = "true";
  rootElement.classList.toggle("native-surface", Boolean(backend));
  document.body.classList.toggle("is-native", Boolean(backend));

  function isAnalyzerPanelVisible() {
    return !compactWorkspaceQuery.matches || compactView === "analyze";
  }

  function syncCompactWorkspace(redrawAnalyzer = true) {
    const compact = compactWorkspaceQuery.matches;
    const analyzerSelected = compactView === "analyze";
    rootElement.dataset.compactView = compactView;
    rootElement.classList.toggle("is-compact-workspace", compact);
    compactViewTabs.dataset.activeView = compactView;
    analyzer.dataset.compactView = compactView;

    compactViewButtons.forEach((button) => {
      const selected = button.dataset.compactView === compactView;
      button.setAttribute("aria-selected", selected ? "true" : "false");
      button.tabIndex = selected ? 0 : -1;
    });

    if (compact) {
      analyzer.setAttribute("aria-hidden", analyzerSelected ? "false" : "true");
      parameterStrip.setAttribute("aria-hidden", analyzerSelected ? "true" : "false");
      if ("inert" in analyzer) analyzer.inert = !analyzerSelected;
      if ("inert" in parameterStrip) parameterStrip.inert = analyzerSelected;
    } else {
      analyzer.removeAttribute("aria-hidden");
      parameterStrip.removeAttribute("aria-hidden");
      if ("inert" in analyzer) analyzer.inert = false;
      if ("inert" in parameterStrip) parameterStrip.inert = false;
    }

    if (redrawAnalyzer && isAnalyzerPanelVisible()) {
      waterfallInitialised = false;
      waterfallEventInitialised = false;
      window.requestAnimationFrame(() => drawVisualFrame(window.performance.now(), false));
    }
  }

  function setCompactView(view) {
    const nextView = view === "control" ? "control" : "analyze";
    if (compactView === nextView) return;
    compactView = nextView;
    clearKnobTrail(false);
    syncCompactWorkspace(true);
  }

  function preventNativeSurfaceDefault(event) {
    if (event.target instanceof Element && event.target.closest('input[type="text"], textarea')
        && ["selectstart", "contextmenu"].includes(event.type)) return;
    event.preventDefault();
  }

  function blockNativeBrowserShortcut(event) {
    const key = String(event.key || "").toLowerCase();
    const commandModifier = event.ctrlKey || event.metaKey;
    const commandKey = commandModifier && ["r", "l", "u", "+", "=", "-", "0", "f", "p", "s"].includes(key);
    const developerTools = event.key === "F12" || (commandModifier && event.shiftKey && ["i", "j", "c"].includes(key));
    const historyKey = (event.altKey && (event.key === "ArrowLeft" || event.key === "ArrowRight"))
      || (commandModifier && (key === "[" || key === "]"));

    if (event.key !== "F5" && !commandKey && !developerTools && !historyKey) return;
    event.preventDefault();
    event.stopPropagation();
  }

  function blockNativeBrowserZoom(event) {
    if (!event.ctrlKey && !event.metaKey) return;
    event.preventDefault();
  }

  function installNativeSurfaceGuards() {
    if (!backend) return;
    document.addEventListener("contextmenu", preventNativeSurfaceDefault, true);
    document.addEventListener("selectstart", preventNativeSurfaceDefault, true);
    document.addEventListener("dragstart", preventNativeSurfaceDefault, true);
    document.addEventListener("dragover", preventNativeSurfaceDefault, true);
    document.addEventListener("drop", preventNativeSurfaceDefault, true);
    document.addEventListener("keydown", blockNativeBrowserShortcut, true);
    window.addEventListener("wheel", blockNativeBrowserZoom, { capture: true, passive: false });
  }

  function removeNativeSurfaceGuards() {
    if (!backend) return;
    document.removeEventListener("contextmenu", preventNativeSurfaceDefault, true);
    document.removeEventListener("selectstart", preventNativeSurfaceDefault, true);
    document.removeEventListener("dragstart", preventNativeSurfaceDefault, true);
    document.removeEventListener("dragover", preventNativeSurfaceDefault, true);
    document.removeEventListener("drop", preventNativeSurfaceDefault, true);
    document.removeEventListener("keydown", blockNativeBrowserShortcut, true);
    window.removeEventListener("wheel", blockNativeBrowserZoom, true);
  }

  function clamp(value, minimum, maximum) {
    return Math.min(maximum, Math.max(minimum, value));
  }

  function fromNormalised(spec, normalised) {
    let proportion = clamp(normalised, 0, 1);
    if (spec.symmetric) {
      let distance = 2 * proportion - 1;
      if (distance !== 0 && spec.skew !== 1)
        distance = Math.sign(distance) * Math.exp(Math.log(Math.abs(distance)) / spec.skew);
      return spec.start + (spec.end - spec.start) * 0.5 * (1 + distance);
    }
    if (spec.skew !== 1 && proportion > 0)
      proportion = Math.exp(Math.log(proportion) / spec.skew);
    return spec.start + (spec.end - spec.start) * proportion;
  }

  function toNormalised(spec, rawValue) {
    let proportion = clamp((rawValue - spec.start) / (spec.end - spec.start), 0, 1);
    if (spec.symmetric) {
      const distance = 2 * proportion - 1;
      return (1 + Math.sign(distance) * Math.pow(Math.abs(distance), spec.skew)) * 0.5;
    }
    return spec.skew === 1 ? proportion : Math.pow(proportion, spec.skew);
  }

  function clearKnobTrail(detach = false) {
    if (knobTrailFrame) window.cancelAnimationFrame(knobTrailFrame);
    knobTrailFrame = 0;
    knobTrailLastFrameTime = 0;
    knobTrailParticles.forEach((particle) => { particle.active = false; });
    knobTrailCanvas.classList.remove("is-active");
    knobTrailCanvas.dataset.activeParticles = "0";
    if (knobTrailContext)
      knobTrailContext.clearRect(0, 0, knobTrailCentre * 2, knobTrailCentre * 2);
    if (detach) {
      knobTrailCanvas.remove();
      knobTrailStage = null;
    }
  }

  function configureKnobTrail(stage) {
    if (!knobTrailContext || !stage) return false;
    if (stage === knobTrailStage) return true;

    clearKnobTrail(false);
    stage.appendChild(knobTrailCanvas);
    knobTrailStage = stage;
    knobTrailLastEmitTime = 0;
    knobTrailLastNormalised = 0;

    const stageSize = Math.max(42, stage.clientWidth || 58);
    const canvasSize = Math.round(stageSize + 18);
    const pixelRatio = Math.min(window.devicePixelRatio || 1, 1.5);
    const pixelSize = Math.round(canvasSize * pixelRatio);
    if (knobTrailCanvas.width !== pixelSize || knobTrailCanvas.height !== pixelSize) {
      knobTrailCanvas.width = pixelSize;
      knobTrailCanvas.height = pixelSize;
      knobTrailCanvas.style.width = `${canvasSize}px`;
      knobTrailCanvas.style.height = `${canvasSize}px`;
      knobTrailContext.setTransform(pixelRatio, 0, 0, pixelRatio, 0, 0);
    }

    knobTrailCentre = canvasSize * 0.5;
    knobTrailRadius = stageSize * 0.5 - 1;
    const module = stage.closest(".parameter-module");
    const accent = module ? window.getComputedStyle(module).getPropertyValue("--accent").trim() : "";
    knobTrailColour = accent || "#d49a37";
    return true;
  }

  function drawKnobTrail(now) {
    knobTrailFrame = 0;
    if (!knobTrailContext || !knobTrailStage || reducedMotionQuery.matches || document.hidden) {
      clearKnobTrail(false);
      return;
    }

    if (knobTrailLastFrameTime && now - knobTrailLastFrameTime < 1000 / 30) {
      knobTrailFrame = window.requestAnimationFrame(drawKnobTrail);
      return;
    }

    const deltaSeconds = knobTrailLastFrameTime
      ? clamp((now - knobTrailLastFrameTime) / 1000, 0, 0.05)
      : 1 / 30;
    knobTrailLastFrameTime = now;
    knobTrailContext.clearRect(0, 0, knobTrailCentre * 2, knobTrailCentre * 2);
    let activeCount = 0;

    knobTrailParticles.forEach((particle) => {
      if (!particle.active) return;
      particle.age += deltaSeconds;
      if (particle.age >= particle.life) {
        particle.active = false;
        return;
      }

      const progress = particle.age / particle.life;
      const angle = particle.angle + particle.angularVelocity * particle.age;
      const radius = particle.radius + particle.radialVelocity * particle.age;
      const x = knobTrailCentre + Math.sin(angle) * radius;
      const y = knobTrailCentre - Math.cos(angle) * radius;
      const size = particle.size * (1 - progress * 0.55);
      const opacity = Math.pow(1 - progress, 1.6) * 0.66;
      knobTrailContext.globalAlpha = opacity;
      knobTrailContext.fillStyle = knobTrailColour;
      knobTrailContext.beginPath();
      knobTrailContext.arc(x, y, size, 0, Math.PI * 2);
      knobTrailContext.fill();
      knobTrailContext.globalAlpha = opacity * 0.42;
      knobTrailContext.fillStyle = "#fffaf0";
      knobTrailContext.beginPath();
      knobTrailContext.arc(x, y, Math.max(0.45, size * 0.38), 0, Math.PI * 2);
      knobTrailContext.fill();
      activeCount += 1;
    });

    knobTrailContext.globalAlpha = 1;
    knobTrailCanvas.dataset.activeParticles = String(activeCount);
    if (activeCount > 0) {
      knobTrailCanvas.classList.add("is-active");
      knobTrailFrame = window.requestAnimationFrame(drawKnobTrail);
    } else {
      knobTrailCanvas.classList.remove("is-active");
      knobTrailLastFrameTime = 0;
    }
  }

  function emitKnobTrail(id, previousNormalised, nextNormalised, source = "pointer") {
    const movement = nextNormalised - previousNormalised;
    if (!knobTrailContext || reducedMotionQuery.matches || document.hidden || Math.abs(movement) < 0.000001)
      return;

    const input = document.getElementById(id);
    const stage = input ? input.closest(".knob-stage") : null;
    if (!stage || !configureKnobTrail(stage)) return;

    const now = window.performance.now();
    const minimumInterval = source === "pointer" ? 32 : 46;
    if (now - knobTrailLastEmitTime < minimumInterval && Math.abs(nextNormalised - knobTrailLastNormalised) < 0.012)
      return;

    const direction = movement > 0 ? 1 : -1;
    const particleCount = source === "pointer" ? (Math.abs(movement) > 0.05 ? 3 : 2) : 1;
    for (let index = 0; index < particleCount; index += 1) {
      const particle = knobTrailParticles[knobTrailPoolCursor];
      const trailPosition = clamp(previousNormalised + movement * (0.35 + index * 0.3), 0, 1);
      const variation = (knobTrailSpawnCount % 3) - 1;
      particle.active = true;
      particle.age = 0;
      particle.life = 0.30 + (knobTrailSpawnCount % 2) * 0.035;
      particle.angle = (-135 + trailPosition * 270) * Math.PI / 180;
      particle.angularVelocity = -direction * (0.30 + index * 0.08);
      particle.radius = knobTrailRadius + 3 - index;
      particle.radialVelocity = 5 + index * 1.5;
      particle.size = 1.55 + variation * 0.14;
      knobTrailPoolCursor = (knobTrailPoolCursor + 1) % knobTrailParticles.length;
      knobTrailSpawnCount += 1;
    }

    knobTrailLastEmitTime = now;
    knobTrailLastNormalised = nextNormalised;
    knobTrailCanvas.dataset.spawnCount = String(knobTrailSpawnCount);
    knobTrailCanvas.classList.add("is-active");
    if (!knobTrailFrame)
      knobTrailFrame = window.requestAnimationFrame(drawKnobTrail);
  }

  function openAboutDialog() {
    window.clearTimeout(aboutCloseTimer);
    endAllGestures();
    aboutPreviousFocus = document.activeElement;
    aboutDialog.hidden = false;
    aboutOpenButton.setAttribute("aria-expanded", "true");
    document.documentElement.classList.add("is-dialog-open");
    document.body.classList.add("is-dialog-open");
    window.requestAnimationFrame(() => {
      aboutDialog.classList.add("is-open");
      aboutCloseButton.focus({ preventScroll: true });
    });
  }

  function closeAboutDialog() {
    if (aboutDialog.hidden) return;
    aboutDialog.classList.remove("is-open");
    aboutOpenButton.setAttribute("aria-expanded", "false");
    document.documentElement.classList.remove("is-dialog-open");
    document.body.classList.remove("is-dialog-open");
    const finishClose = () => {
      aboutDialog.hidden = true;
      if (aboutPreviousFocus && typeof aboutPreviousFocus.focus === "function")
        aboutPreviousFocus.focus({ preventScroll: true });
      aboutPreviousFocus = null;
    };
    if (reducedMotionQuery.matches) finishClose();
    else aboutCloseTimer = window.setTimeout(finishClose, 160);
  }

  function handleAboutKeydown(event) {
    if (event.key === "Escape") {
      event.preventDefault();
      closeAboutDialog();
      return;
    }
    if (event.key !== "Tab") return;

    const focusable = Array.from(aboutPanel.querySelectorAll('button:not([disabled]), a[href]'));
    if (focusable.length < 1) return;
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    if (event.shiftKey && document.activeElement === first) {
      event.preventDefault();
      last.focus();
    } else if (!event.shiftKey && document.activeElement === last) {
      event.preventDefault();
      first.focus();
    }
  }

  function frequencyPosition(frequency) {
    const nyquist = Math.max(1, state.sampleRate * 0.5);
    const normalised = clamp(frequency / nyquist, 0, 1);
    return Math.log10(1 + normalised * 99) / 2;
  }

  function formatFrequency(value) {
    const absolute = Math.abs(value);
    if (absolute >= 1000)
      return `${(value / 1000).toFixed(absolute >= 10000 ? 1 : 2)}k`;
    return `${Math.round(value)}`;
  }

  function formatValue(id, value) {
    if (id === "shiftHz") {
      const mode = Math.round(state.mode);
      if (mode === 1)
        return `${value >= 0 ? "+" : ""}${(value * 0.005).toFixed(1)} %`;
      const unit = mode === 15 ? "ct" : "Hz";
      return `${value >= 0 ? "+" : ""}${Math.round(value)} ${unit}`;
    }
    if (id === "scale") return `${value.toFixed(2)}x`;
    if (id === "pivotHz") return formatFrequency(value);
    if (id === "amount" || id === "mix") return `${Math.round(value * 100)}%`;
    if (id === "widthQ") return value.toFixed(2);
    if (id === "outputGainDb") return `${value >= 0 ? "+" : ""}${value.toFixed(1)} dB`;
    if (id === "pitchRoot") return pitchRootNames[clamp(Math.round(value), 0, pitchRootNames.length - 1)];
    if (id === "pitchScale") return t(pitchScaleNames[clamp(Math.round(value), 0, pitchScaleNames.length - 1)]);
    return String(value);
  }

  function parameterChangeLabel(id) {
    if (id === "mode") return t("PROCESS");
    if (id === "quality") return t("QUALITY");
    if (id === "bypass") return t("BYPASS");
    if (id === "pitchRoot") return t("ROOT");
    if (id === "pitchScale") return t("PITCH SCALE");
    const label = document.getElementById(`${id}Label`);
    return label ? label.textContent : t(id.toUpperCase());
  }

  function parameterChangeValue(id) {
    if (id === "mode") return modeNames[clamp(Math.round(state.mode), 0, modeNames.length - 1)].toUpperCase();
    if (id === "quality") return t(["LOW", "NORMAL", "HIGH"][clamp(Math.round(state.quality), 0, 2)]);
    if (id === "bypass") return t(state.bypass >= 0.5 ? "ON" : "OFF");
    return formatValue(id, state[id]);
  }

  function showWaterfallParameterChange(id) {
    const markerIndex = waterfallMarkerByParameter[id];
    if (!markerIndex) return;
    pendingWaterfallMarker = markerIndex;
    waterfallChangeReadout.textContent = `${parameterChangeLabel(id)}  ${parameterChangeValue(id)}`;
    waterfallChangeReadout.style.setProperty("--marker-colour", waterfallMarkerPalette[markerIndex]);
    waterfallChangeReadout.classList.add("is-visible");
    window.clearTimeout(waterfallReadoutTimer);
    waterfallReadoutTimer = window.setTimeout(() => waterfallChangeReadout.classList.remove("is-visible"), 1100);
  }

  function emitGesture(id, phase, value) {
    if (!backend) return;
    const payload = { id, phase };
    if (value !== undefined) payload.value = value;
    backend.emitEvent("parameterGesture", payload);
  }

  function beginGesture(id) {
    if (activeGestures.has(id)) return;
    activeGestures.add(id);
    emitGesture(id, "begin");
  }

  function endGesture(id) {
    if (!activeGestures.has(id)) return;
    activeGestures.delete(id);
    emitGesture(id, "end");
  }

  function endAllGestures() {
    Array.from(activeGestures).forEach(endGesture);
    activePointerDrags.clear();
    knobTrailPointerId = null;
    clearKnobTrail(false);
  }

  function renderParameter(id) {
    const range = document.querySelector(`input[type="range"][data-param="${id}"]`);
    if (range && specs[id]) {
      const normalised = toNormalised(specs[id], state[id]);
      range.value = String(normalised);
      const knobStage = range.closest(".knob-stage");
      if (knobStage) {
        knobStage.style.setProperty("--angle", `${-135 + normalised * 270}deg`);
        knobStage.style.setProperty("--arc", `${normalised * 75}%`);
      }
      const output = document.getElementById(`${id}Value`);
      if (output) output.value = formatValue(id, state[id]);
    }

    const select = document.querySelector(`select[data-param="${id}"]`);
    if (select) select.value = String(Math.round(state[id]));

    const toggle = document.querySelector(`button[data-param="${id}"]`);
    if (toggle) toggle.setAttribute("aria-pressed", state[id] >= 0.5 ? "true" : "false");

    if (id === "analyzerView") {
      const view = clamp(Math.round(state.analyzerView), 0, 2);
      const waterfallVisible = view !== 0;
      const fftVisible = view !== 1;
      document.querySelectorAll("[data-analyzer-layer]").forEach((button) => {
        const active = button.dataset.analyzerLayer === "waterfall" ? waterfallVisible : fftVisible;
        button.setAttribute("aria-pressed", active ? "true" : "false");
        button.disabled = active && waterfallVisible !== fftVisible;
      });
      analyzerLayoutSelect.disabled = view !== 2;
    }

    if (id === "quality")
      document.getElementById("fftReadout").textContent = ["512", "1024", "2048"][Math.round(state.quality)] || "1024";

    if (id === "bypass")
      document.body.classList.toggle("is-bypassed", state.bypass >= 0.5);
  }

  function setLocalParameter(id, value, send = false, visualiseChange = true) {
    const nextValue = Number(value);
    const previousValue = state[id];
    const changed = !Number.isFinite(previousValue) || Math.abs(previousValue - nextValue) > 0.0000001;
    state[id] = nextValue;
    if (send) emitGesture(id, "value", value);
    renderParameter(id);

    if (changed && id === "mode") updateModeControls();
    if (changed && id === "analyzerView") updateAnalyzerLayout();
    if (changed && ["mode", "shiftHz", "pivotHz", "amount", "widthQ", "pitchRoot", "pitchScale", "quality"].includes(id))
      updateFrequencyGuides();
    const changesAnalyzer = ["mode", "shiftHz", "scale", "pivotHz", "amount", "widthQ", "pitchRoot", "pitchScale", "mix", "outputGainDb", "bypass"].includes(id);
    if (changed && changesAnalyzer && !backend) updatePreviewData(window.performance.now());
    if (changed && changesAnalyzer)
      drawSpectrum();
    if (changed && visualiseChange) showWaterfallParameterChange(id);
  }

  function updateModeControls() {
    const modeIndex = clamp(Math.round(state.mode), 0, modes.length - 1);
    const info = modes[modeIndex] || {};
    const used = new Set(info.uses || []);
    const description = t(info.description || modeNames[modeIndex]);
    const describedText = `${modeNames[modeIndex]}: ${description}`;
    const pitchMapActive = modeIndex === 23;

    modeSelectContainer.dataset.description = describedText;
    modeEffectDescription.textContent = describedText;

    ["shiftHz", "scale", "pivotHz", "amount", "widthQ"].forEach((id) => {
      const wrapper = document.querySelector(`[data-control="${id}"]`);
      const input = document.getElementById(id);
      const enabled = used.has(id);
      if (wrapper) {
        wrapper.classList.toggle("is-inactive", !enabled);
        wrapper.title = t(info.tooltips && info.tooltips[id] ? info.tooltips[id] : defaultControlTooltips[id]);
      }
      if (input) input.disabled = !enabled;
      if (!enabled) endGesture(id);
    });

    axisModule.hidden = pitchMapActive;
    axisModule.setAttribute("aria-hidden", pitchMapActive ? "true" : "false");
    pitchMapModule.hidden = !pitchMapActive;
    pitchMapModule.setAttribute("aria-hidden", pitchMapActive ? "false" : "true");
    ["pitchRoot", "pitchScale"].forEach((id) => {
      const input = document.getElementById(id);
      if (input) input.disabled = !pitchMapActive;
      if (!pitchMapActive) endGesture(id);
    });

    document.getElementById("shiftHzLabel").textContent = t(info.shift || "SHIFT");
    document.getElementById("scaleLabel").textContent = t(info.scale || "SCALE");
    document.getElementById("pivotHzLabel").textContent = t(info.pivot || "PIVOT");
    document.getElementById("amountLabel").textContent = t(info.amount || "AMOUNT");
    document.getElementById("widthQLabel").textContent = t(info.width || "WIDTH/Q");
    renderParameter("shiftHz");
    updateFrequencyGuides();
  }

  function updateAnalyzerLayout() {
    const targetClass = ["view-spectrum", "view-waterfall", "view-both"][Math.round(state.analyzerView)] || "view-both";
    const currentClass = ["view-spectrum", "view-waterfall", "view-both"].find((name) => analyzer.classList.contains(name));
    renderParameter("analyzerView");
    if (currentClass === targetClass) return false;
    analyzer.classList.remove("view-spectrum", "view-waterfall", "view-both");
    analyzer.classList.add(targetClass);
    waterfallInitialised = false;
    waterfallEventInitialised = false;
    resetWaterfallClock();
    window.requestAnimationFrame(() => drawVisualFrame(0, false));
    return true;
  }

  function updateAnalyzerArrangement() {
    const layout = ["stack", "overlay", "split"].includes(analyzerLayoutSelect.value) ? analyzerLayoutSelect.value : "overlay";
    const targetClass = `layout-${layout}`;
    const currentClass = ["layout-stack", "layout-overlay", "layout-split"].find((name) => analyzer.classList.contains(name));
    analyzerLayoutSelect.value = layout;
    analyzer.dataset.layout = layout;
    if (currentClass === targetClass) {
      drawSpectrum();
      return;
    }
    analyzer.classList.remove("layout-stack", "layout-overlay", "layout-split");
    analyzer.classList.add(targetClass);
    waterfallInitialised = false;
    waterfallEventInitialised = false;
    resetWaterfallClock();
    window.requestAnimationFrame(() => drawVisualFrame(0, false));
  }

  function updateWaterfallVisualProfile(profileId, redraw = true) {
    const nextProfile = Object.prototype.hasOwnProperty.call(waterfallVisualProfiles, profileId) ? profileId : "vision";
    const changed = waterfallVisualProfile !== nextProfile;
    const profile = waterfallVisualProfiles[nextProfile];
    waterfallVisualProfile = nextProfile;
    waterfallVisualSelect.value = nextProfile;
    waterfallVisualSelect.title = `${profile.label}: ${profile.description}`;
    analyzer.dataset.waterfallVisual = nextProfile;
    waterfallCanvas.dataset.visualProfile = nextProfile;
    if (changed && redraw) {
      waterfallInitialised = false;
      window.requestAnimationFrame(() => drawWaterfallColumn(0, window.performance.now()));
    }
    return changed;
  }

  function updateFrequencyGuides() {
    const modeIndex = Math.round(state.mode);
    const glitchActive = modeIndex === 22;
    const pitchMapActive = modeIndex === 23;
    const pivotPosition = 1 - frequencyPosition(state.pivotHz);
    pivotGuide.style.top = `${pivotPosition * 100}%`;
    pivotGuide.style.display = pitchMapActive ? "none" : "block";
    const pivotLabel = modeIndex === 9 ? "REFLECT" : (glitchActive ? "BAND CENTER" : "PIVOT");
    document.getElementById("pivotGuideLabel").textContent = `${t(pivotLabel)} ${formatFrequency(state.pivotHz)}`;

    glitchSelection.hidden = !glitchActive;
    if (glitchActive) {
      const nyquist = Math.max(20, state.sampleRate * 0.5);
      const fftSize = [512, 1024, 2048][clamp(Math.round(state.quality), 0, 2)];
      const binWidth = nyquist / Math.max(1, fftSize * 0.5);
      const centre = clamp(state.pivotHz, binWidth, nyquist);
      const bandwidth = clamp(centre / clamp(state.widthQ, 0.05, 8), 4 * binWidth, nyquist);
      const lowHz = Math.max(binWidth, centre - bandwidth * 0.5);
      const highHz = Math.min(nyquist, centre + bandwidth * 0.5);
      const top = (1 - frequencyPosition(highHz)) * 100;
      const bottom = (1 - frequencyPosition(lowHz)) * 100;
      glitchSelection.style.top = `${top}%`;
      glitchSelection.style.height = `${Math.max(0.3, bottom - top)}%`;
      glitchSelection.dataset.lowHz = String(Math.round(lowHz));
      glitchSelection.dataset.highHz = String(Math.round(highHz));
      glitchSelectionLabel.textContent = `${t("GLITCH BAND")} ${formatFrequency(lowHz)}-${formatFrequency(highHz)}`;
    }

    pitchMapReadout.hidden = !pitchMapActive;
    if (pitchMapActive) {
      const root = pitchRootNames[clamp(Math.round(state.pitchRoot), 0, pitchRootNames.length - 1)];
      const scaleName = pitchScaleNames[clamp(Math.round(state.pitchScale), 0, pitchScaleNames.length - 1)];
      pitchMapReadout.value = `Pitch Map  ${root} ${t(scaleName)}`;
      pitchMapReadout.textContent = pitchMapReadout.value;
    }

    const showsDestination = [1, 3, 8].includes(modeIndex);
    destinationGuide.style.display = showsDestination ? "block" : "none";
    if (showsDestination) {
      const destination = clamp(state.pivotHz + state.shiftHz * state.amount, 0, state.sampleRate * 0.5);
      destinationGuide.style.top = `${(1 - frequencyPosition(destination)) * 100}%`;
    }

    document.querySelectorAll("[data-frequency]").forEach((element) => {
      const frequency = Number(element.dataset.frequency);
      const visible = frequency <= state.sampleRate * 0.5;
      element.hidden = !visible;
      if (visible) element.style.top = `${(1 - frequencyPosition(frequency)) * 100}%`;
    });
  }

  function updateRateReadout() {
    const rate = state.sampleRate >= 1000 ? `${Math.round(state.sampleRate / 1000)}k` : `${Math.round(state.sampleRate)}`;
    document.getElementById("rateReadout").textContent = rate;
  }

  function interpolate(values, position) {
    const scaled = clamp(position, 0, 1) * (values.length - 1);
    const low = Math.floor(scaled);
    const high = Math.min(values.length - 1, low + 1);
    return values[low] + (values[high] - values[low]) * (scaled - low);
  }

  function sampleAnalyzerBin(values, position) {
    const index = Math.round(clamp(position, 0, 1) * (values.length - 1));
    return values[index];
  }

  function gaussian(position, centre, width) {
    const distance = (position - centre) / Math.max(width, 0.0001);
    return Math.exp(-distance * distance * 0.5);
  }

  function frequencyAtPosition(position) {
    const nyquist = Math.max(1, state.sampleRate * 0.5);
    return (Math.pow(100, clamp(position, 0, 1)) - 1) / 99 * nyquist;
  }

  function previewHash(value) {
    const result = Math.sin(value * 12.9898 + 78.233) * 43758.5453;
    return result - Math.floor(result);
  }

  function isPreviewScaleNote(midiNote, root, scaleIndex) {
    const pitchClass = ((midiNote - root) % 12 + 12) % 12;
    return (scaleIndex === 1 ? previewMinorScale : previewMajorScale)[pitchClass];
  }

  function previewPitchMapFrequency(frequency) {
    if (!(frequency > 0)) return 0;
    const root = clamp(Math.round(state.pitchRoot), 0, 11);
    const scaleIndex = clamp(Math.round(state.pitchScale), 0, 1);
    const midi = 69 + 12 * Math.log2(frequency / 440);
    const centreNote = Math.floor(midi);
    let bestNote = centreNote;
    let bestDistance = Number.POSITIVE_INFINITY;
    for (let candidate = centreNote - 6; candidate <= centreNote + 6; candidate += 1) {
      if (!isPreviewScaleNote(candidate, root, scaleIndex)) continue;
      const distance = Math.abs(midi - candidate);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestNote = candidate;
      }
    }
    return 440 * Math.pow(2, (bestNote - 69) / 12);
  }

  function updatePreviewData(timeMs) {
    const time = reducedMotionQuery.matches ? 0.65 : timeMs * 0.001;
    const peakPositions = [0.09, 0.17, 0.265, 0.38, 0.505, 0.64, 0.79];

    for (let index = 0; index < pointCount; index += 1) {
      const x = index / (pointCount - 1);
      let amplitude = 0.00005;
      for (let peak = 0; peak < peakPositions.length; peak += 1) {
        const centre = peakPositions[peak];
        const width = 0.009 + peak * 0.0014;
        const pulse = 0.42 + 0.14 * Math.sin(time * (0.65 + peak * 0.09) + peak * 1.17);
        amplitude += gaussian(x, centre, width) * pulse / (1 + peak * 0.22);
      }
      const stationaryTexture = Math.max(0, Math.sin(x * 68)) * (0.012 + 0.003 * Math.sin(time * 1.7));
      amplitude += stationaryTexture * Math.exp(-x * 1.7);
      analyzerData.input[index] = clamp(20 * Math.log10(Math.max(amplitude, 0.000016)), -96, -1);
    }

    const mode = Math.round(state.mode);
    const amount = clamp(state.amount, 0, 1);
    const pivot = frequencyPosition(state.pivotHz);
    const shift = state.shiftHz / 5000 * 0.16;
    const gain = Math.pow(10, state.outputGainDb / 20);
    const nyquist = Math.max(20, state.sampleRate * 0.5);
    const previewFftSize = [512, 1024, 2048][clamp(Math.round(state.quality), 0, 2)];
    const binWidth = nyquist / Math.max(1, previewFftSize * 0.5);
    const glitchCentre = clamp(state.pivotHz, binWidth, nyquist);
    const glitchBandwidth = clamp(glitchCentre / clamp(state.widthQ, 0.05, 8), 4 * binWidth, nyquist);
    const glitchLow = Math.max(binWidth, glitchCentre - glitchBandwidth * 0.5);
    const glitchHigh = Math.min(nyquist, glitchCentre + glitchBandwidth * 0.5);
    const glitchChunkWidth = Math.max(binWidth * 2, glitchBandwidth / 12);
    const glitchHoldSeconds = 0.11 + (0.025 - 0.11) * amount;
    const glitchEpoch = Math.floor(time / glitchHoldSeconds);

    previewPitchMapEnergy.fill(0);
    if (mode === 23) {
      for (let sourceIndex = 0; sourceIndex < pointCount; sourceIndex += 1) {
        const sourcePosition = sourceIndex / (pointCount - 1);
        const sourceFrequency = frequencyAtPosition(sourcePosition);
        const mappedFrequency = clamp(previewPitchMapFrequency(sourceFrequency), 0, nyquist);
        const mappedIndex = frequencyPosition(mappedFrequency) * (pointCount - 1);
        const lowIndex = clamp(Math.floor(mappedIndex), 0, pointCount - 1);
        const highIndex = clamp(lowIndex + 1, 0, pointCount - 1);
        const highWeight = mappedIndex - lowIndex;
        const sourceAmplitude = Math.pow(10, analyzerData.input[sourceIndex] / 20);
        const energy = sourceAmplitude * sourceAmplitude;
        previewPitchMapEnergy[lowIndex] += energy * (1 - highWeight);
        previewPitchMapEnergy[highIndex] += energy * highWeight;
      }
    }

    for (let index = 0; index < pointCount; index += 1) {
      const x = index / (pointCount - 1);
      let sourcePosition = x;
      if ([1, 3, 8, 15, 18].includes(mode)) sourcePosition = x - shift * amount;
      if (mode === 9) sourcePosition = pivot - (x - pivot) / Math.max(state.scale, 0.01);
      if ([13, 14, 19, 20].includes(mode)) sourcePosition = pivot + (x - pivot) / Math.max(state.scale, 0.01);
      if ([10, 11, 12].includes(mode)) sourcePosition = x + (pivot - x) * amount * 0.24;

      const dryDb = analyzerData.input[index];
      let wetDb = interpolate(analyzerData.input, sourcePosition);
      if (mode === 2) {
        wetDb = (wetDb + interpolate(analyzerData.input, sourcePosition - 0.025 * state.widthQ) + interpolate(analyzerData.input, sourcePosition + 0.025 * state.widthQ)) / 3;
      } else if (mode === 4) {
        wetDb += 7 * Math.max(0, Math.cos((x / Math.max(pivot, 0.035)) * Math.PI * 2));
      } else if (mode === 5) {
        wetDb = interpolate(analyzerData.input, x * (0.56 + amount * 0.24));
      } else if (mode === 6) {
        wetDb = wetDb < -44 + amount * 24 ? -96 : wetDb;
      } else if (mode === 7) {
        wetDb += Math.sin(x * 92) * amount * 4;
      } else if ([16, 17].includes(mode)) {
        const shepardShape = Math.sin((x - pivot) * Math.PI * (mode === 17 ? 12 : 8));
        const shepardPulse = 0.86 + 0.14 * Math.sin(time * 0.8);
        wetDb += shepardShape * shepardPulse * amount * 5;
      } else if (mode === 18) {
        wetDb -= Math.pow(Math.sin((x + pivot) * (20 + Math.abs(state.shiftHz) * 0.006)), 2) * amount * 14;
      } else if (mode === 21) {
        wetDb += Math.sin(x * Math.PI * 18 / Math.max(pivot, 0.08)) * amount * 2.5;
      } else if (mode === 22) {
        const frequency = frequencyAtPosition(x);
        wetDb = dryDb;
        if (frequency >= glitchLow && frequency <= glitchHigh) {
          const chunk = Math.max(0, Math.floor((frequency - glitchLow) / glitchChunkWidth));
          const seed = glitchEpoch * 131 + chunk * 17;
          if (previewHash(seed) <= 0.12 + amount * 0.88) {
            const randomOffset = previewHash(seed + 47) * 2 - 1;
            const displacement = state.shiftHz + randomOffset * glitchBandwidth * (0.12 + amount * 0.28);
            const sourceFrequency = frequency - displacement;
            const mappedDb = sourceFrequency < 0 || sourceFrequency > nyquist
              ? -96
              : interpolate(analyzerData.input, frequencyPosition(sourceFrequency));
            wetDb = dryDb * (1 - amount) + mappedDb * amount;
          }
        }
      } else if (mode === 23) {
        const dryAmplitude = Math.pow(10, dryDb / 20);
        const mappedAmplitude = Math.sqrt(Math.max(0, previewPitchMapEnergy[index]));
        const mappedOutput = dryAmplitude * (1 - amount) + mappedAmplitude * amount;
        wetDb = 20 * Math.log10(Math.max(mappedOutput, 0.000016));
      }

      if (mode === 0 || state.bypass >= 0.5) wetDb = dryDb;
      const dryAmplitude = Math.pow(10, dryDb / 20);
      const wetAmplitude = Math.pow(10, clamp(wetDb, -96, 6) / 20) * gain;
      const outputAmplitude = dryAmplitude * (1 - state.mix) + wetAmplitude * state.mix;
      analyzerData.output[index] = clamp(20 * Math.log10(Math.max(outputAmplitude, 0.000016)), -96, 6);
    }
  }

  function copyAnalyzerPayload(payload) {
    if (!payload || !Array.isArray(payload.input) || !Array.isArray(payload.output)) return false;
    const count = Math.min(pointCount, payload.input.length, payload.output.length);
    if (count < 2) return false;
    for (let index = 0; index < pointCount; index += 1) {
      const position = index / (pointCount - 1) * (count - 1);
      const low = Math.floor(position), high = Math.min(count - 1, low + 1), fraction = position - low;
      const inputDb = Number(payload.input[low]) * (1 - fraction) + Number(payload.input[high]) * fraction;
      const outputDb = Number(payload.output[low]) * (1 - fraction) + Number(payload.output[high]) * fraction;
      analyzerData.input[index] = Number.isFinite(inputDb) ? inputDb : -96;
      analyzerData.output[index] = Number.isFinite(outputDb) ? outputDb : -96;
    }
    return true;
  }

  function resizeCanvas(canvas) {
    const rect = canvas.getBoundingClientRect();
    if (rect.width < 1 || rect.height < 1) return false;
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    const width = Math.max(1, Math.round(rect.width * ratio));
    const height = Math.max(1, Math.round(rect.height * ratio));
    if (canvas.width === width && canvas.height === height) return false;
    canvas.width = width;
    canvas.height = height;
    return true;
  }

  function buildWaterfallColourTable(profile) {
    const table = new Uint8ClampedArray(256 * 4);
    for (let encoded = 0; encoded < 256; encoded += 1) {
      const db = decodeWaterfallDb(encoded);
      const normalised = clamp((db - profile.floorDb) / (profile.ceilingDb - profile.floorDb), 0, 1);
      const energy = Math.pow(normalised, profile.gamma);
      let lower = profile.stops[0];
      let upper = profile.stops[profile.stops.length - 1];
      for (let index = 1; index < profile.stops.length; index += 1) {
        if (energy <= profile.stops[index][0]) {
          lower = profile.stops[index - 1];
          upper = profile.stops[index];
          break;
        }
      }
      const span = Math.max(0.000001, upper[0] - lower[0]);
      const amount = clamp((energy - lower[0]) / span, 0, 1);
      const offset = encoded * 4;
      table[offset] = Math.round(lower[1][0] + (upper[1][0] - lower[1][0]) * amount);
      table[offset + 1] = Math.round(lower[1][1] + (upper[1][1] - lower[1][1]) * amount);
      table[offset + 2] = Math.round(lower[1][2] + (upper[1][2] - lower[1][2]) * amount);
      table[offset + 3] = 255;
    }
    return table;
  }

  function getWaterfallColourTable(profileId = waterfallVisualProfile) {
    if (!waterfallColourTables.has(profileId))
      waterfallColourTables.set(profileId, buildWaterfallColourTable(waterfallVisualProfiles[profileId]));
    return waterfallColourTables.get(profileId);
  }

  function writeHeatPixelFromEncoded(data, offset, encoded, table = getWaterfallColourTable()) {
    const colourOffset = clamp(Math.round(encoded), 0, 255) * 4;
    data[offset] = table[colourOffset];
    data[offset + 1] = table[colourOffset + 1];
    data[offset + 2] = table[colourOffset + 2];
    data[offset + 3] = 255;
  }

  function writeHeatPixel(data, offset, db) {
    writeHeatPixelFromEncoded(data, offset, encodeWaterfallDb(db));
  }

  function heatColour(db) {
    writeHeatPixel(heatPixelScratch, 0, db);
    return `rgb(${heatPixelScratch[0]}, ${heatPixelScratch[1]}, ${heatPixelScratch[2]})`;
  }

  function encodeWaterfallDb(db) {
    return Math.round(clamp((db + 96) / 102, 0, 1) * 255);
  }

  function decodeWaterfallDb(encoded) {
    return encoded / 255 * 102 - 96;
  }

  function captureWaterfallHistoryRow() {
    for (let index = 0; index < pointCount; index += 1)
      waterfallHistoryRowScratch[index] = encodeWaterfallDb(backend && receivedDedicatedWaterfall ? waterfallData[index] : analyzerData.output[index]);
  }

  function storeWaterfallHistoryRow(markerIndex = 0) {
    const offset = waterfallHistoryHead * pointCount;
    waterfallHistory.set(waterfallHistoryRowScratch, offset);
    waterfallHistoryMarkers[waterfallHistoryHead] = markerIndex;
    waterfallHistoryHead = (waterfallHistoryHead + 1) % waterfallHistoryCapacity;
    waterfallHistoryCount = Math.min(waterfallHistoryCapacity, waterfallHistoryCount + 1);
  }

  function seedWaterfallHistory(endTimeMs, desiredColumns) {
    if (backend || waterfallHistoryCount > 0) return;
    const totalColumns = Math.min(waterfallHistoryCapacity, Math.max(1, desiredColumns));
    const columnsPerAnalyzerFrame = waterfallSpeedMultiplier;
    const frameCount = Math.ceil(totalColumns / columnsPerAnalyzerFrame);
    const firstFrameColumns = totalColumns - (frameCount - 1) * columnsPerAnalyzerFrame;
    const finalTime = Number.isFinite(endTimeMs) && endTimeMs > 0 ? endTimeMs : (lastPreviewUpdate || 650);

    for (let frame = 0; frame < frameCount; frame += 1) {
      updatePreviewData(finalTime - (frameCount - 1 - frame) * (1000 / 15));
      captureWaterfallHistoryRow();
      const repeat = frame === 0 ? firstFrameColumns : columnsPerAnalyzerFrame;
      for (let column = 0; column < repeat; column += 1) storeWaterfallHistoryRow();
    }
    updatePreviewData(finalTime);
  }

  function renderWaterfallHistory(context, width, height) {
    const image = context.createImageData(width, height);
    const data = image.data;
    const colourTable = getWaterfallColourTable();
    for (let offset = 0; offset < data.length; offset += 4) {
      data[offset] = colourTable[0];
      data[offset + 1] = colourTable[1];
      data[offset + 2] = colourTable[2];
      data[offset + 3] = 255;
    }

    const pixelRatio = Math.min(window.devicePixelRatio || 1, 2);
    const columnWidth = Math.max(1, Math.round(pixelRatio));
    const visibleColumns = Math.ceil(width / columnWidth);
    const columnsToDraw = Math.min(waterfallHistoryCount, visibleColumns);
    const historyStart = waterfallHistoryCount - columnsToDraw;
    const screenStart = visibleColumns - columnsToDraw;
    const oldestSlot = (waterfallHistoryHead - waterfallHistoryCount + waterfallHistoryCapacity) % waterfallHistoryCapacity;

    for (let column = 0; column < columnsToDraw; column += 1) {
      const slot = (oldestSlot + historyStart + column) % waterfallHistoryCapacity;
      const historyOffset = slot * pointCount;
      const startX = (screenStart + column) * columnWidth;
      const endX = Math.min(width, startX + columnWidth);
      for (let y = 0; y < height; y += 1) {
        const bin = Math.round((1 - y / Math.max(1, height - 1)) * (pointCount - 1));
        const encoded = waterfallHistory[historyOffset + bin];
        writeHeatPixelFromEncoded(heatPixelScratch, 0, encoded, colourTable);
        for (let x = startX; x < endX; x += 1) {
          const offset = (y * width + x) * 4;
          data[offset] = heatPixelScratch[0];
          data[offset + 1] = heatPixelScratch[1];
          data[offset + 2] = heatPixelScratch[2];
          data[offset + 3] = 255;
        }
      }

    }

    context.putImageData(image, 0, 0);
    waterfallFullRenderCount += 1;
    waterfallCanvas.dataset.fullRenderCount = String(waterfallFullRenderCount);
  }

  function renderWaterfallEventHistory(context, width, height) {
    context.clearRect(0, 0, width, height);
    const pixelRatio = Math.min(window.devicePixelRatio || 1, 2);
    const columnWidth = Math.max(1, Math.round(pixelRatio));
    const visibleColumns = Math.ceil(width / columnWidth);
    const columnsToDraw = Math.min(waterfallHistoryCount, visibleColumns);
    const historyStart = waterfallHistoryCount - columnsToDraw;
    const screenStart = visibleColumns - columnsToDraw;
    const oldestSlot = (waterfallHistoryHead - waterfallHistoryCount + waterfallHistoryCapacity) % waterfallHistoryCapacity;

    for (let column = 0; column < columnsToDraw; column += 1) {
      const slot = (oldestSlot + historyStart + column) % waterfallHistoryCapacity;
      const markerColour = waterfallMarkerPalette[waterfallHistoryMarkers[slot]];
      if (!markerColour) continue;
      const startX = (screenStart + column) * columnWidth;
      context.fillStyle = markerColour;
      context.fillRect(startX, 0, Math.min(columnWidth, width - startX), height);
    }
  }

  function advanceWaterfallEventTrack(columns, markerIndex) {
    const context = waterfallEventContext;
    const width = waterfallEventCanvas.width;
    const height = waterfallEventCanvas.height;
    if (!context || width < 2 || height < 1) return;
    const pixelRatio = Math.min(window.devicePixelRatio || 1, 2);
    const columnWidth = Math.max(1, Math.round(pixelRatio));
    const shift = Math.min(width - 1, Math.max(1, columnWidth * columns));
    context.drawImage(waterfallEventCanvas, shift, 0, width - shift, height, 0, 0, width - shift, height);
    context.clearRect(width - shift, 0, shift, height);
    const markerColour = waterfallMarkerPalette[markerIndex];
    if (markerColour) {
      context.fillStyle = markerColour;
      context.fillRect(width - columnWidth, 0, columnWidth, height);
    }
  }

  function appendWaterfallHistory(columns) {
    captureWaterfallHistoryRow();
    const markerIndex = pendingWaterfallMarker;
    for (let column = 0; column < columns; column += 1)
      storeWaterfallHistoryRow(column === columns - 1 ? markerIndex : 0);
    pendingWaterfallMarker = 0;
    return markerIndex;
  }

  function advanceWaterfallHistoryWithoutPainting(advanceColumns) {
    const columns = Math.max(0, Math.floor(Number(advanceColumns) || 0));
    if (columns < 1) return;
    appendWaterfallHistory(columns);
    waterfallAdvanceCount += columns;
    waterfallCanvas.dataset.advanceCount = String(waterfallAdvanceCount);
  }

  function drawWaterfallColumn(advanceColumns, now) {
    if (analyzer.classList.contains("view-spectrum")) return;
    const resized = resizeCanvas(waterfallCanvas);
    const eventResized = resizeCanvas(waterfallEventCanvas);
    if (!waterfallContext) waterfallContext = waterfallCanvas.getContext("2d", { alpha: false });
    if (!waterfallEventContext) waterfallEventContext = waterfallEventCanvas.getContext("2d", { alpha: true });
    const context = waterfallContext;
    const width = waterfallCanvas.width;
    const height = waterfallCanvas.height;
    if (!context || width < 2 || height < 2) return;
    context.imageSmoothingEnabled = false;

    if (resized || !waterfallInitialised) {
      const pixelRatio = Math.min(window.devicePixelRatio || 1, 2);
      seedWaterfallHistory(now, Math.ceil(width / Math.max(1, Math.round(pixelRatio))));
      renderWaterfallHistory(context, width, height);
      waterfallInitialised = true;
    }

    if (eventResized || !waterfallEventInitialised) {
      renderWaterfallEventHistory(waterfallEventContext, waterfallEventCanvas.width, waterfallEventCanvas.height);
      waterfallEventInitialised = true;
    }

    if (!advanceColumns) return;

    const columns = Math.max(0, Math.floor(Number(advanceColumns) || 0));
    if (columns < 1) return;
    const markerIndex = appendWaterfallHistory(columns);
    const columnWidth = Math.max(1, Math.round(Math.min(window.devicePixelRatio || 1, 2)));
    const shift = Math.min(width - 1, columnWidth * columns);
    context.drawImage(waterfallCanvas, shift, 0, width - shift, height, 0, 0, width - shift, height);
    for (let y = 0; y < height; y += 1) {
      const frequency = 1 - y / Math.max(1, height - 1);
      context.fillStyle = heatColour(sampleAnalyzerBin(backend && receivedDedicatedWaterfall ? waterfallData : analyzerData.output, frequency));
      context.fillRect(width - shift, y, shift, 1);
    }
    advanceWaterfallEventTrack(columns, markerIndex);
    waterfallAdvanceCount += columns;
    waterfallCanvas.dataset.advanceCount = String(waterfallAdvanceCount);
  }

  function resetWaterfallClock(now = null) {
    lastWaterfallAnimationTime = Number.isFinite(now) ? now : null;
    waterfallAdvanceAccumulator = 0;
  }

  function waterfallColumnsForFrame(now) {
    if (lastWaterfallAnimationTime === null) {
      lastWaterfallAnimationTime = now;
      return 0;
    }
    const elapsed = clamp(now - lastWaterfallAnimationTime, 0, 250);
    lastWaterfallAnimationTime = now;
    const columnsPerSecond = 15 * (reducedMotionQuery.matches ? 1 : waterfallSpeedMultiplier);
    waterfallAdvanceAccumulator += elapsed * columnsPerSecond / 1000;
    const due = Math.floor(waterfallAdvanceAccumulator);
    const advance = Math.min(32, due);
    waterfallAdvanceAccumulator -= advance;
    return advance;
  }

  function drawSpectrum() {
    if (analyzer.classList.contains("view-waterfall")) return;
    resizeCanvas(spectrumCanvas);
    if (!spectrumContext) spectrumContext = spectrumCanvas.getContext("2d", { alpha: true });
    const context = spectrumContext;
    const width = spectrumCanvas.width;
    const height = spectrumCanvas.height;
    if (!context || width < 2 || height < 2) return;

    const overlayMode = analyzer.classList.contains("view-both") && analyzer.classList.contains("layout-overlay");
    if (overlayMode) {
      context.clearRect(0, 0, width, height);
      const ratio = Math.min(window.devicePixelRatio || 1, 2);
      const bandWidth = clamp(width * 0.28, 88 * ratio, 200 * ratio);
      const drawVerticalLine = (values, colour, lineWidth, fillColour) => {
        context.beginPath();
        for (let index = 0; index < values.length; index += 1) {
          const x = clamp((values[index] + 96) / 102, 0, 1) * bandWidth;
          const y = (1 - index / Math.max(1, values.length - 1)) * height;
          if (index === 0) context.moveTo(x, y);
          else context.lineTo(x, y);
        }
        if (fillColour) {
          context.lineTo(0, 0);
          context.lineTo(0, height);
          context.closePath();
          context.fillStyle = fillColour;
          context.fill();
        }
        context.strokeStyle = colour;
        context.lineWidth = lineWidth;
        context.lineJoin = "round";
        context.stroke();
      };
      drawVerticalLine(analyzerData.input, "rgba(155, 139, 229, 0.9)", 1.2 * ratio, "rgba(58, 47, 110, 0.12)");
      drawVerticalLine(analyzerData.output, "rgba(79, 224, 214, 0.96)", 1.6 * ratio, "rgba(53, 196, 207, 0.14)");
      return;
    }

    context.fillStyle = "#080a0d";
    context.fillRect(0, 0, width, height);
    context.lineWidth = 1;
    context.strokeStyle = "rgba(128, 142, 166, 0.17)";

    [100, 500, 1000, 5000, 10000, 20000].forEach((frequency) => {
      if (frequency >= state.sampleRate * 0.5) return;
      const x = frequencyPosition(frequency) * width;
      context.beginPath();
      context.moveTo(x, 0);
      context.lineTo(x, height);
      context.stroke();
    });

    [-72, -48, -24].forEach((db) => {
      const y = (1 - (db + 96) / 96) * height;
      context.beginPath();
      context.moveTo(0, y);
      context.lineTo(width, y);
      context.stroke();
    });

    const drawLine = (values, colour, lineWidth, fillColour) => {
      context.beginPath();
      for (let index = 0; index < values.length; index += 1) {
        const x = index / Math.max(1, values.length - 1) * width;
        const y = clamp(1 - (values[index] + 96) / 96, 0, 1) * height;
        if (index === 0) context.moveTo(x, y);
        else context.lineTo(x, y);
      }
      if (fillColour) {
        context.lineTo(width, height);
        context.lineTo(0, height);
        context.closePath();
        context.fillStyle = fillColour;
        context.fill();
      }
      context.strokeStyle = colour;
      context.lineWidth = lineWidth;
      context.lineJoin = "round";
      context.stroke();
    };

    drawLine(analyzerData.input, "rgba(101, 88, 180, 0.82)", 1.2, "rgba(58, 47, 110, 0.14)");
    drawLine(analyzerData.output, "#35c4cf", 1.8, "rgba(53, 196, 207, 0.16)");
  }

  function getPeakDb(values) {
    let peak = -96;
    for (let index = 0; index < values.length; index += 1)
      peak = Math.max(peak, values[index]);
    return peak;
  }

  function updateMeters(now) {
    const inputTarget = getPeakDb(analyzerData.input);
    const outputTarget = getPeakDb(analyzerData.output);
    inputMeterDb += (inputTarget - inputMeterDb) * (inputTarget > inputMeterDb ? 0.24 : 0.07);
    outputMeterDb += (outputTarget - outputMeterDb) * (outputTarget > outputMeterDb ? 0.24 : 0.07);

    if (inputTarget >= inputPeakDb) {
      inputPeakDb = inputTarget;
      inputPeakHoldUntil = now + 620;
    } else if (now > inputPeakHoldUntil) {
      inputPeakDb = Math.max(inputTarget, inputPeakDb - 0.4);
    }

    if (outputTarget >= outputPeakDb) {
      outputPeakDb = outputTarget;
      outputPeakHoldUntil = now + 620;
    } else if (now > outputPeakHoldUntil) {
      outputPeakDb = Math.max(outputTarget, outputPeakDb - 0.4);
    }

    const inputLevel = clamp((inputMeterDb + 60) / 60, 0, 1) * 100;
    const outputLevel = clamp((outputMeterDb + 60) / 60, 0, 1) * 100;
    const inputPeak = clamp((inputPeakDb + 60) / 60, 0, 1) * 100;
    const outputPeak = clamp((outputPeakDb + 60) / 60, 0, 1) * 100;
    inputMeter.style.setProperty("--level", `${inputLevel}%`);
    inputMeter.parentElement.style.setProperty("--peak", `${inputPeak}%`);
    outputMeter.style.setProperty("--level", `${outputLevel}%`);
    outputMeter.parentElement.style.setProperty("--peak", `${outputPeak}%`);

    const inputPeakOutput = document.getElementById("inputPeak");
    const outputPeakOutput = document.getElementById("outputPeak");
    inputPeakOutput.value = clamp(inputPeakDb, -96, 12).toFixed(1);
    outputPeakOutput.value = clamp(outputPeakDb, -96, 12).toFixed(1);
    document.getElementById("peakValue").value = clamp(outputPeakDb, -96, 12).toFixed(1);
    outputPeakOutput.classList.toggle("is-clipping", outputPeakDb >= 0);
    outputMeter.parentElement.classList.toggle("is-clipping", outputPeakDb >= 0);
  }

  function drawVisualFrame(now, advanceWaterfall) {
    if (isAnalyzerPanelVisible()) {
      drawWaterfallColumn(advanceWaterfall, now);
      drawSpectrum();
    }
    updateMeters(now);
  }

  function stopAnimationLoop() {
    if (animationFrameHandle) window.cancelAnimationFrame(animationFrameHandle);
    animationFrameHandle = 0;
    resetWaterfallClock();
  }

  function startAnimationLoop() {
    if (!animationFrameHandle && nativeSurfaceVisible && !document.hidden)
      animationFrameHandle = window.requestAnimationFrame(animationFrame);
  }

  function animationFrame(now) {
    animationFrameHandle = 0;
    if (document.hidden || !nativeSurfaceVisible) {
      resetWaterfallClock(now);
      return;
    }

    animationFrameHandle = window.requestAnimationFrame(animationFrame);
    if (state.freeze >= 0.5 || analyzer.classList.contains("view-spectrum")) {
      resetWaterfallClock(now);
      return;
    }

    if (!backend) {
      const interval = reducedMotionQuery.matches ? 250 : 1000 / 15;
      if (now - lastPreviewUpdate >= interval) {
        updatePreviewData(now);
        if (isAnalyzerPanelVisible()) drawSpectrum();
        updateMeters(now);
        lastPreviewUpdate = now;
        lastAnalyzerFrameTime = now;
      }
    } else if ((receivedDedicatedWaterfall ? lastWaterfallFrameTime === null || now - lastWaterfallFrameTime > 250 : lastAnalyzerFrameTime === null || now - lastAnalyzerFrameTime > 250)) {
      resetWaterfallClock(now);
      return;
    }

    const columns = waterfallColumnsForFrame(now);
    if (columns > 0) {
      if (isAnalyzerPanelVisible()) drawWaterfallColumn(columns, now);
      else advanceWaterfallHistoryWithoutPainting(columns);
    }
  }

  document.querySelectorAll('input[type="range"][data-param]').forEach((input) => {
    const id = input.dataset.param;

    input.addEventListener("pointerdown", (event) => {
      if (input.disabled) return;
      event.preventDefault();
      input.focus({ preventScroll: true });
      input.setPointerCapture(event.pointerId);
      activePointerDrags.set(event.pointerId, { id, input, startY: event.clientY, startValue: Number(input.value) });
      if (knobTrailPointerId === null) knobTrailPointerId = event.pointerId;
      beginGesture(id);
    });

    input.addEventListener("pointermove", (event) => {
      const drag = activePointerDrags.get(event.pointerId);
      if (!drag) return;
      const sensitivity = event.shiftKey ? 720 : 210;
      const normalised = clamp(drag.startValue + (drag.startY - event.clientY) / sensitivity, 0, 1);
      const previousNormalised = Number(drag.input.value);
      drag.input.value = String(normalised);
      setLocalParameter(drag.id, fromNormalised(specs[drag.id], normalised), true);
      if (event.pointerId === knobTrailPointerId)
        emitKnobTrail(drag.id, previousNormalised, normalised, "pointer");
    });

    const finishPointer = (event) => {
      const drag = activePointerDrags.get(event.pointerId);
      if (!drag) return;
      activePointerDrags.delete(event.pointerId);
      if (input.hasPointerCapture(event.pointerId)) input.releasePointerCapture(event.pointerId);
      if (event.pointerId === knobTrailPointerId) knobTrailPointerId = null;
      endGesture(drag.id);
    };

    input.addEventListener("pointerup", finishPointer);
    input.addEventListener("pointercancel", finishPointer);
    input.addEventListener("lostpointercapture", (event) => {
      const drag = activePointerDrags.get(event.pointerId);
      if (!drag) return;
      activePointerDrags.delete(event.pointerId);
      if (event.pointerId === knobTrailPointerId) knobTrailPointerId = null;
      endGesture(drag.id);
    });

    input.addEventListener("input", () => {
      if (activePointerDrags.size > 0) return;
      const previousNormalised = toNormalised(specs[id], state[id]);
      const normalised = Number(input.value);
      beginGesture(id);
      setLocalParameter(id, fromNormalised(specs[id], normalised), true);
      emitKnobTrail(id, previousNormalised, normalised, "input");
    });

    input.addEventListener("keydown", (event) => {
      if (input.disabled) return;
      let normalised = Number(input.value);
      const fineStep = event.shiftKey ? 0.001 : 0.01;
      if (event.key === "ArrowUp" || event.key === "ArrowRight") normalised += fineStep;
      else if (event.key === "ArrowDown" || event.key === "ArrowLeft") normalised -= fineStep;
      else if (event.key === "PageUp") normalised += 0.1;
      else if (event.key === "PageDown") normalised -= 0.1;
      else if (event.key === "Home") normalised = 0;
      else if (event.key === "End") normalised = 1;
      else return;

      event.preventDefault();
      const previousNormalised = Number(input.value);
      normalised = clamp(normalised, 0, 1);
      input.value = String(normalised);
      beginGesture(id);
      setLocalParameter(id, fromNormalised(specs[id], normalised), true);
      emitKnobTrail(id, previousNormalised, normalised, "keyboard");
    });

    input.addEventListener("change", () => endGesture(id));
    input.addEventListener("keyup", () => endGesture(id));
    input.addEventListener("blur", () => endGesture(id));
    input.addEventListener("dblclick", (event) => {
      if (input.disabled) return;
      event.preventDefault();
      const previousNormalised = Number(input.value);
      const normalised = toNormalised(specs[id], specs[id].defaultValue);
      beginGesture(id);
      setLocalParameter(id, specs[id].defaultValue, true);
      emitKnobTrail(id, previousNormalised, normalised, "reset");
      endGesture(id);
    });
  });

  document.querySelectorAll("select[data-param]").forEach((select) => {
    select.addEventListener("change", () => {
      const id = select.dataset.param;
      beginGesture(id);
      setLocalParameter(id, Number(select.value), true);
      endGesture(id);
    });
  });

  document.querySelectorAll("button[data-param]").forEach((button) => {
    button.addEventListener("click", () => {
      const id = button.dataset.param;
      beginGesture(id);
      setLocalParameter(id, state[id] >= 0.5 ? 0 : 1, true);
      endGesture(id);
    });
  });

  document.querySelectorAll("[data-analyzer-layer]").forEach((button) => {
    button.addEventListener("click", () => {
      const currentView = clamp(Math.round(state.analyzerView), 0, 2);
      let waterfallVisible = currentView !== 0;
      let fftVisible = currentView !== 1;
      if (button.dataset.analyzerLayer === "waterfall") waterfallVisible = !waterfallVisible;
      else fftVisible = !fftVisible;
      if (!waterfallVisible && !fftVisible) return;
      const nextView = waterfallVisible ? (fftVisible ? 2 : 1) : 0;
      beginGesture("analyzerView");
      setLocalParameter("analyzerView", nextView, true);
      endGesture("analyzerView");
    });
  });

  compactViewButtons.forEach((button, index) => {
    button.addEventListener("click", () => setCompactView(button.dataset.compactView));
    button.addEventListener("keydown", (event) => {
      let nextIndex = index;
      if (event.key === "ArrowLeft" || event.key === "ArrowUp") nextIndex = (index - 1 + compactViewButtons.length) % compactViewButtons.length;
      else if (event.key === "ArrowRight" || event.key === "ArrowDown") nextIndex = (index + 1) % compactViewButtons.length;
      else if (event.key === "Home") nextIndex = 0;
      else if (event.key === "End") nextIndex = compactViewButtons.length - 1;
      else return;

      event.preventDefault();
      const nextButton = compactViewButtons[nextIndex];
      setCompactView(nextButton.dataset.compactView);
      nextButton.focus({ preventScroll: true });
    });
  });

  aboutOpenButton.addEventListener("click", () => {
    if (aboutDialog.hidden) openAboutDialog();
    else closeAboutDialog();
  });
  aboutCloseButton.addEventListener("click", closeAboutDialog);
  aboutDialog.addEventListener("click", (event) => {
    if (event.target === aboutDialog) closeAboutDialog();
  });
  aboutDialog.addEventListener("keydown", handleAboutKeydown);
  document.querySelectorAll("[data-external-url]").forEach((link) => {
    link.addEventListener("click", (event) => {
      if (!backend) return;
      event.preventDefault();
      backend.emitEvent("openExternal", { url: link.href });
    });
  });

  waterfallRateSelect.addEventListener("change", () => {
    const nextMultiplier = Number(waterfallRateSelect.value);
    waterfallSpeedMultiplier = [1, 2, 4, 8].includes(nextMultiplier) ? nextMultiplier : 4;
    waterfallRateSelect.value = String(waterfallSpeedMultiplier);
    analyzer.dataset.waterfallSpeed = `${waterfallSpeedMultiplier}x`;
    analyzer.dataset.waterfallColumnsPerSecond = String(15 * waterfallSpeedMultiplier);
    resetWaterfallClock();
  });

  waterfallVisualSelect.addEventListener("change", () => updateWaterfallVisualProfile(waterfallVisualSelect.value));
  analyzerLayoutSelect.addEventListener("change", updateAnalyzerArrangement);
  analyzer.dataset.waterfallSpeed = `${waterfallSpeedMultiplier}x`;
  analyzer.dataset.waterfallColumnsPerSecond = String(15 * waterfallSpeedMultiplier);
  analyzer.dataset.layout = "overlay";
  waterfallCanvas.dataset.advanceCount = String(waterfallAdvanceCount);
  waterfallCanvas.dataset.fullRenderCount = String(waterfallFullRenderCount);
  waterfallCanvas.dataset.parameterMarkers = "none";
  waterfallEventCanvas.dataset.parameterMarkers = "timeline";
  updateWaterfallVisualProfile(waterfallVisualSelect.value, false);
  installNativeSurfaceGuards();
  syncCompactWorkspace(false);


  // Presets are owned by the native processor; the browser only presents commands.
  const presetSelect = document.getElementById("presetSelect");
  const presetMenu = document.getElementById("presetMenu");
  const presetDialog = document.getElementById("presetDialog");
  const presetName = document.getElementById("presetName");
  const presetRequests = new Map();
  let presetRequestId = 0;
  let presetState = {id: "init", name: "Init", dirty: false, entries: [{id: "init", name: "Init"}]};
  let presetBusy = false;
  let noticeTimer;
  function renderPresetState(payload = {}) {
    presetState = {...presetState, ...payload};
    const entries = [...presetState.entries];
    if (!entries.some(entry => entry.id === presetState.id)) entries.push({id: presetState.id, name: presetState.name});
    presetSelect.replaceChildren(...entries.map(entry => {
      const option = document.createElement("option");
      option.value = entry.id;
      option.textContent = entry.id === "init" ? t("Init") : entry.name;
      return option;
    }));
    presetSelect.value = presetState.id;
    presetSelect.title = (presetState.id === "init" ? t("Init") : presetState.name) + (presetState.dirty ? ` · ${t("Unsaved changes")}` : "");
    document.getElementById("presetDirty").hidden = !presetState.dirty;
  }
  function showPresetNotice(message) {
    const notice = document.getElementById("presetNotice");
    notice.textContent = t(message);
    notice.hidden = false;
    clearTimeout(noticeTimer);
    noticeTimer = setTimeout(() => { notice.hidden = true; }, 6000);
  }
  function presetCommand(action, extra = {}) {
    if (!backend) { showPresetNotice("This action is available in the installed plugin."); return Promise.resolve(false); }
    endAllGestures();
    return new Promise(resolve => {
      const requestId = ++presetRequestId;
      presetRequests.set(requestId, resolve);
      backend.emitEvent("presetCommand", {action, requestId, language: window.FlipShiftI18n.language, ...extra});
    });
  }
  function askPreset(kind) {
    endAllGestures();
    const naming = kind === "name";
    document.getElementById("presetDialogTitle").textContent = t(naming ? "Save as…" : "Unsaved changes");
    document.getElementById("presetDialogMessage").textContent = t(naming ? "Enter a name for this preset." : "Save changes before switching?");
    presetName.hidden = !naming;
    presetName.required = naming;
    document.getElementById("presetNameLabel").hidden = !naming;
    document.getElementById("presetDiscard").hidden = naming;
    presetName.value = presetState.id === "init" ? "" : presetState.name;
    presetName.setCustomValidity("");
    const focusBefore = document.activeElement;
    return new Promise(resolve => {
      presetDialog.addEventListener("close", () => {
        const choice = presetDialog.returnValue;
        if (focusBefore && focusBefore.isConnected) focusBefore.focus({preventScroll: true});
        resolve({choice, name: presetName.value.trim()});
      }, {once: true});
      presetDialog.returnValue = "cancel";
      presetDialog.showModal();
      if (naming) { presetName.focus(); presetName.select(); }
    });
  }
  presetName.addEventListener("input", () => presetName.setCustomValidity(presetName.value.trim() ? "" : t("Enter a name for this preset.")));
  async function savePreset(asNew = false) {
    const naming = asNew || presetState.id === "init";
    const answer = naming ? await askPreset("name") : {choice: "save", name: presetState.name};
    if (answer.choice !== "save" || !answer.name) return false;
    const ok = await presetCommand(naming ? "saveAs" : "save", {name: answer.name});
    if (ok) showPresetNotice("Saved");
    return ok;
  }
  async function beforePresetSwitch() {
    if (!presetState.dirty) return true;
    const answer = await askPreset("dirty");
    if (answer.choice === "discard") return true;
    if (answer.choice === "save") return savePreset();
    return false;
  }
  async function runPresetAction(action, id) {
    if (presetBusy) return;
    presetBusy = true;
    presetMenu.open = false;
    presetSelect.disabled = true;
    try {
      if (!backend) { showPresetNotice("This action is available in the installed plugin."); return; }
      if (action === "save" || action === "saveAs") await savePreset(action === "saveAs");
      else if (action === "load" || action === "import") {
        if (await beforePresetSwitch()) {
          const ok = await presetCommand(action, {id});
          if (ok && action === "import") showPresetNotice("Imported");
        }
      } else if (action === "export" && await presetCommand("export")) showPresetNotice("Exported");
    } finally {
      presetBusy = false;
      presetSelect.disabled = false;
      renderPresetState();
    }
  }
  presetSelect.addEventListener("change", () => { const id = presetSelect.value; renderPresetState(); runPresetAction("load", id); });
  presetMenu.addEventListener("toggle", () => { if (presetMenu.open && backend && !presetBusy) presetCommand("list"); });
  document.querySelectorAll("[data-preset-action]").forEach(button => button.addEventListener("click", () => runPresetAction(button.dataset.presetAction)));
  document.addEventListener("pointerdown", event => { if (!presetMenu.contains(event.target)) presetMenu.open = false; });
  document.addEventListener("keydown", event => {
    if (event.key === "Escape" && presetMenu.open) { presetMenu.open = false; presetMenu.querySelector("summary").focus(); }
  });
  document.getElementById("languageSelect").addEventListener("change", event => {
    window.FlipShiftI18n.apply(event.target.value);
    document.getElementById("bridgeStatus").textContent = t(backend ? "NATIVE" : "PREVIEW");
    Object.keys(state).forEach(renderParameter);
    updateModeControls();
    renderPresetState();
  });
  renderPresetState();
  if (backend) {
    listenerTokens.push(backend.addEventListener("presetState", renderPresetState));
    listenerTokens.push(backend.addEventListener("presetResult", payload => {
      const resolve = presetRequests.get(payload.requestId);
      if (!resolve) return;
      presetRequests.delete(payload.requestId);
      if (!payload.ok && payload.error !== "preset.cancelled") showPresetNotice(payload.error);
      resolve(Boolean(payload.ok));
    }));
  }

  Object.keys(state).forEach(renderParameter);
  updateModeControls();
  updateAnalyzerLayout();
  updateAnalyzerArrangement();
  updateFrequencyGuides();
  updateRateReadout();

  if (!backend) {
    const initialTime = window.performance.now();
    updatePreviewData(initialTime);
    lastPreviewUpdate = initialTime;
    lastAnalyzerFrameTime = initialTime;
    window.requestAnimationFrame(() => drawVisualFrame(initialTime, false));
  } else {
    document.getElementById("bridgeStatus").textContent = t("NATIVE");
    listenerTokens.push(backend.addEventListener("parameterState", (payload) => {
      if (!payload || !payload.values) return;
      state.sampleRate = Number(payload.sampleRate) || state.sampleRate;
      Object.entries(payload.values).forEach(([id, value]) => {
        if (id in state) setLocalParameter(id, Number(value), false, receivedInitialParameterState);
      });
      receivedInitialParameterState = true;
      updateRateReadout();
      updateModeControls();
      updateFrequencyGuides();
    }));
    listenerTokens.push(backend.addEventListener("waterfallFrame", (payload) => {
      if (state.freeze >= 0.5 || !payload || !Array.isArray(payload.output) || payload.output.length !== pointCount) return;
      for (let i = 0; i < pointCount; ++i) {
        const value = Number(payload.output[i]);
        waterfallData[i] = Number.isFinite(value) ? clamp(value, -96, 12) : -96;
      }
      receivedDedicatedWaterfall = true;
      lastWaterfallFrameTime = window.performance.now();
      waterfallCanvas.dataset.analysisFftSize = String(payload.fftSize);
    }));
    listenerTokens.push(backend.addEventListener("analyzerFrame", (payload) => {
      if (state.freeze >= 0.5) return;
      if (copyAnalyzerPayload(payload)) {
        const now = window.performance.now();
        lastAnalyzerFrameTime = now;
        if (isAnalyzerPanelVisible()) drawSpectrum();
        updateMeters(now);
      }
    }));
    listenerTokens.push(backend.addEventListener("surfaceVisibility", (payload) => {
      nativeSurfaceVisible = Boolean(payload && payload.visible);
      rootElement.dataset.surfaceVisible = nativeSurfaceVisible ? "true" : "false";
      if (!nativeSurfaceVisible) {
        endAllGestures();
        clearKnobTrail(false);
        stopAnimationLoop();
        return;
      }

      resetWaterfallClock();
      window.requestAnimationFrame(() => drawVisualFrame(0, false));
      startAnimationLoop();
    }));
    backend.emitEvent("uiReady", {});
  }

  const resizeObserver = new ResizeObserver(() => {
    waterfallInitialised = false;
    waterfallEventInitialised = false;
    resetWaterfallClock();
    window.requestAnimationFrame(() => drawVisualFrame(0, false));
  });
  resizeObserver.observe(analyzer);
  window.addEventListener("resize", () => clearKnobTrail(true));

  const handleReducedMotionChange = () => {
    if (reducedMotionQuery.matches) clearKnobTrail(false);
    resetWaterfallClock();
  };
  if (typeof reducedMotionQuery.addEventListener === "function")
    reducedMotionQuery.addEventListener("change", handleReducedMotionChange);
  else if (typeof reducedMotionQuery.addListener === "function")
    reducedMotionQuery.addListener(handleReducedMotionChange);

  const handleCompactWorkspaceChange = () => syncCompactWorkspace(true);
  if (typeof compactWorkspaceQuery.addEventListener === "function")
    compactWorkspaceQuery.addEventListener("change", handleCompactWorkspaceChange);
  else if (typeof compactWorkspaceQuery.addListener === "function")
    compactWorkspaceQuery.addListener(handleCompactWorkspaceChange);

  document.addEventListener("visibilitychange", () => {
    if (document.hidden) {
      endAllGestures();
      stopAnimationLoop();
    }
    else {
      resetWaterfallClock();
      window.requestAnimationFrame(() => drawVisualFrame(0, false));
      startAnimationLoop();
    }
  });

  window.addEventListener("unload", () => {
    endAllGestures();
    clearKnobTrail(true);
    window.clearTimeout(aboutCloseTimer);
    window.clearTimeout(waterfallReadoutTimer);
    stopAnimationLoop();
    if (backend) listenerTokens.forEach((token) => backend.removeEventListener(token));
    removeNativeSurfaceGuards();
    if (typeof compactWorkspaceQuery.removeEventListener === "function")
      compactWorkspaceQuery.removeEventListener("change", handleCompactWorkspaceChange);
    else if (typeof compactWorkspaceQuery.removeListener === "function")
      compactWorkspaceQuery.removeListener(handleCompactWorkspaceChange);
    resizeObserver.disconnect();
  });

  startAnimationLoop();
})();
