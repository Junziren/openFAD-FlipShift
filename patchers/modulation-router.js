autowatch = 1;
inlets = 1;
outlets = 5;

var values = {
  shift: 0,
  translate: 0,
  scale: 1,
  pivot: 1000,
  filter: 1000
};
var modulationValue = 0;
var targetValue = 0;
var depthValue = 0;

function clamp(value, low, high) {
  return Math.max(low, Math.min(high, value));
}

function emit() {
  var normalized = clamp(modulationValue, -1, 1) * clamp(depthValue, -1, 1);
  var shiftOut = values.shift;
  var translateOut = values.translate;
  var scaleOut = values.scale;
  var pivotOut = values.pivot;
  var filterOut = values.filter;

  if (targetValue === 0) shiftOut += normalized * 5000;
  else if (targetValue === 1) translateOut += normalized * 10000;
  else if (targetValue === 2) scaleOut *= Math.pow(2, normalized * 2);
  else if (targetValue === 3) pivotOut *= Math.pow(2, normalized * 4);
  else if (targetValue === 4) filterOut *= Math.pow(2, normalized * 6);

  outlet(4, clamp(filterOut, 20, 20000));
  outlet(3, clamp(pivotOut, 20, 10000));
  outlet(2, clamp(scaleOut, 0.25, 4));
  outlet(1, clamp(translateOut, -10000, 10000));
  outlet(0, clamp(shiftOut, -5000, 5000));
}

function shift(value) { values.shift = value; emit(); }
function translate(value) { values.translate = value; emit(); }
function scale(value) { values.scale = value; emit(); }
function pivot(value) { values.pivot = value; emit(); }
function filter(value) { values.filter = value; emit(); }
function modulation(value) { modulationValue = value; emit(); }
function target(value) { targetValue = Math.floor(value); emit(); }
function depth(value) { depthValue = value; emit(); }
function bang() { emit(); }
