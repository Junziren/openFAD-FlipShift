autowatch = 1;
inlets = 1;
outlets = 2;

var bufferName = jsarguments.length > 1 ? jsarguments[1] : "vectorshift-mask";
var binCount = 1024;
var sampleRate = 48000;
var modeValue = 0;
var frequencyValue = 1000;
var qValue = 0.7;
var amountValue = 1;
var seedValue = 41;
var pivotValue = 110;

function clamp(value, low, high) {
  return Math.max(low, Math.min(high, value));
}

function seeded(index) {
  var value = Math.sin((index + 1) * 12.9898 + seedValue * 78.233) * 43758.5453;
  return value - Math.floor(value);
}

function smoothStep(edge0, edge1, value) {
  var x = clamp((value - edge0) / Math.max(0.000001, edge1 - edge0), 0, 1);
  return x * x * (3 - 2 * x);
}

function maskForBin(index) {
  var hz = index * sampleRate / (binCount * 2);
  var amount = clamp(amountValue, 0, 1);
  var width = Math.max(20, frequencyValue / Math.max(0.1, qValue));
  var mask = 1;

  if (modeValue === 0) {
    mask = 1 - smoothStep(frequencyValue - width * 0.5, frequencyValue + width * 0.5, hz);
  } else if (modeValue === 1) {
    var distance = Math.abs(hz - frequencyValue);
    mask = 1 - smoothStep(width * 0.35, width * 0.55, distance);
  } else if (modeValue === 2) {
    var period = Math.max(20, frequencyValue);
    var phase = (hz % period) / period;
    mask = phase < 0.5 ? 1 : 1 - amount;
  } else if (modeValue === 3) {
    mask = index % 2 === 0 ? 1 : 1 - amount;
  } else if (modeValue === 4) {
    mask = 1;
  } else if (modeValue === 5) {
    mask = 1;
  } else if (modeValue === 6) {
    mask = seeded(index) > amount ? 1 : 0;
  } else if (modeValue === 7) {
    var fundamental = Math.max(20, pivotValue);
    var harmonic = Math.max(1, Math.round(hz / fundamental));
    var target = harmonic * fundamental;
    var tolerance = Math.max(3, fundamental * (0.02 + (1 - amount) * 0.25));
    mask = Math.abs(hz - target) <= tolerance ? 1 : 0;
  }

  return clamp(1 - amount + mask * amount, 0, 1);
}

function rebuild() {
  var target = new Buffer(bufferName);
  var values = [];
  for (var index = 0; index < binCount; index++) values[index] = maskForBin(index);
  target.poke(1, 0, values);
  outlet(0, modeValue === 5 ? 100000 : 0);
  outlet(1, "mask", values);
}

function mode(value) { modeValue = clamp(Math.floor(value), 0, 7); rebuild(); }
function frequency(value) { frequencyValue = clamp(value, 20, sampleRate * 0.5); rebuild(); }
function q(value) { qValue = clamp(value, 0.1, 20); rebuild(); }
function amount(value) { amountValue = clamp(value, 0, 1); rebuild(); }
function seed(value) { seedValue = Math.floor(value); rebuild(); }
function pivot(value) { pivotValue = clamp(value, 20, 10000); rebuild(); }
function samplerate(value) { sampleRate = Math.max(8000, value); rebuild(); }
function bins(value) { binCount = Math.max(16, Math.floor(value)); rebuild(); }
function bang() { rebuild(); }
