autowatch = 1;
inlets = 2;
outlets = 1;

var inputBins = [];
var outputBins = [];
var history = [];
var maxHistory = 48;
var pivot = 1000;
var translate = 0;
var scale = 1;
var filterLow = 20;
var filterHigh = 20000;
var expanded = 1;

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }
function freqx(freq, width) {
  return Math.log(clamp(freq, 20, 20000) / 20) / Math.log(1000) * width;
}
function dbY(db, top, height) { return top + (1 - clamp((db + 90) / 90, 0, 1)) * height; }

function spectrum() {
  var a = arrayfromargs(arguments);
  var split = a.indexOf("out");
  if (split < 0) outputBins = a;
  else { inputBins = a.slice(0, split); outputBins = a.slice(split + 1); }
  if (outputBins.length) {
    history.unshift(outputBins.slice(0));
    if (history.length > maxHistory) history.pop();
  }
  mgraphics.redraw();
}

function jit_matrix(name) {
  var matrix = new JitterMatrix(name);
  var width = matrix.dim[0];
  var bins = [];
  var usable = Math.max(2, Math.floor(width * 0.5));
  for (var index = 0; index < usable; index++) {
    var cell = matrix.getcell(index);
    var normalized = cell instanceof Array ? cell[0] : cell;
    bins[index] = clamp(normalized, 0, 1) * 90 - 90;
  }
  if (inlet === 0) inputBins = bins;
  else {
    outputBins = bins;
    history.unshift(outputBins.slice(0));
    if (history.length > maxHistory) history.pop();
  }
  mgraphics.redraw();
}

function params(p, t, s, lo, hi) {
  pivot = p; translate = t; scale = s; filterLow = lo; filterHigh = hi;
  mgraphics.redraw();
}
function mode(v) { expanded = v ? 1 : 0; mgraphics.redraw(); }
function clear() { history = []; mgraphics.redraw(); }

function drawGrid(w, h) {
  var freqs = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
  mgraphics.set_line_width(1);
  mgraphics.set_source_rgba(0.31, 0.33, 0.34, 0.42);
  for (var i = 0; i < freqs.length; i++) {
    var x = freqx(freqs[i], w);
    mgraphics.move_to(x, 0); mgraphics.line_to(x, h); mgraphics.stroke();
  }
  for (var db = -90; db <= 0; db += 18) {
    var y = dbY(db, 0, h);
    mgraphics.move_to(0, y); mgraphics.line_to(w, y); mgraphics.stroke();
  }
}

function drawCurve(data, w, top, h, rgba) {
  if (!data || data.length < 2) return;
  mgraphics.set_source_rgba(rgba[0], rgba[1], rgba[2], rgba[3]);
  mgraphics.set_line_width(1.6);
  for (var i = 0; i < data.length; i++) {
    var x = i / (data.length - 1) * w;
    var y = dbY(data[i], top, h);
    if (i === 0) mgraphics.move_to(x, y); else mgraphics.line_to(x, y);
  }
  mgraphics.stroke();
}

function paint() {
  var w = box.rect[2] - box.rect[0];
  var h = box.rect[3] - box.rect[1];
  mgraphics.set_source_rgb(0.105, 0.115, 0.12); mgraphics.rectangle(0, 0, w, h); mgraphics.fill();
  var spectrumH = expanded ? h * 0.56 : h;
  drawGrid(w, spectrumH);

  var loX = freqx(filterLow, w), hiX = freqx(filterHigh, w);
  mgraphics.set_source_rgba(0.95, 0.68, 0.18, 0.10);
  mgraphics.rectangle(loX, 0, Math.max(1, hiX - loX), spectrumH); mgraphics.fill();

  drawCurve(inputBins, w, 0, spectrumH, [0.62, 0.65, 0.66, 0.55]);
  drawCurve(outputBins, w, 0, spectrumH, [0.33, 0.87, 0.72, 0.95]);

  var px = freqx(pivot, w);
  mgraphics.set_source_rgba(0.95, 0.68, 0.18, 0.9); mgraphics.set_line_width(1.5);
  mgraphics.move_to(px, 0); mgraphics.line_to(px, spectrumH); mgraphics.stroke();

  if (expanded) {
    var top = spectrumH + 2, rowH = (h - top) / Math.max(1, maxHistory);
    for (var r = 0; r < history.length; r++) {
      var bins = history[r];
      for (var j = 0; j < bins.length; j++) {
        var n = clamp((bins[j] + 90) / 90, 0, 1);
        mgraphics.set_source_rgba(0.08 + n * 0.25, 0.20 + n * 0.67, 0.24 + n * 0.48, 0.9);
        mgraphics.rectangle(j / bins.length * w, top + r * rowH, w / bins.length + 1, rowH + 1);
        mgraphics.fill();
      }
    }
  }
}

function onclick(x, y) {
  var w = box.rect[2] - box.rect[0];
  var freq = 20 * Math.pow(1000, clamp(x / w, 0, 1));
  outlet(0, "pivot", freq);
}
