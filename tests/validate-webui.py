#!/usr/bin/env python3
"""Runtime validation for the embedded WebView control surface."""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import sys

from playwright.sync_api import sync_playwright


VIEWPORTS = ((1000, 650), (621, 844), (390, 844), (320, 280))

MOCK_BRIDGE_SCRIPT = """
window.__bridgeEvents = [];
window.__bridgeInboundEvents = [];
window.__bridgeListeners = new Map();
window.__bridgeListenerId = 0;
window.__JUCE__ = { backend: {
  emitEvent: (name, payload) => window.__bridgeEvents.push({ name, payload }),
  addEventListener: (name, callback) => {
    const token = `${name}:${++window.__bridgeListenerId}`;
    window.__bridgeListeners.set(token, { name, callback });
    return token;
  },
  removeEventListener: (token) => window.__bridgeListeners.delete(token)
}};
window.__emitFromCpp = (name, payload) => {
  let listenerCount = 0;
  for (const listener of window.__bridgeListeners.values()) {
    if (listener.name !== name) continue;
    listener.callback(payload);
    listenerCount += 1;
  }
  window.__bridgeInboundEvents.push({ name, listenerCount });
  return listenerCount;
};
"""


def find_browser(explicit: str | None) -> Path:
    candidates = [
        explicit,
        os.environ.get("OPENFAD_CHROME_PATH"),
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    raise FileNotFoundError("Chrome or Edge was not found. Pass --browser or OPENFAD_CHROME_PATH.")


def collect_metrics(page):
    return page.evaluate(
        """() => {
          const root = document.documentElement;
          const body = document.body;
          const strip = document.getElementById("parameterStrip");
          const tabs = document.getElementById("compactViewTabs");

          const selects = [...document.querySelectorAll("select")].map((element) => {
            const style = getComputedStyle(element);
            const bounds = element.getBoundingClientRect();
            const visible = style.display !== "none" && style.visibility !== "hidden" && bounds.width > 0 && bounds.height > 0;
            const context = document.createElement("canvas").getContext("2d");
            context.font = `${style.fontStyle} ${style.fontWeight} ${style.fontSize} ${style.fontFamily}`;
            const widths = [...element.options].map((option) => ({
              text: option.textContent.trim(),
              width: context.measureText(option.textContent.trim()).width
            }));
            const widest = widths.reduce((left, right) => left.width > right.width ? left : right);
            const available = element.clientWidth
              - parseFloat(style.paddingLeft)
              - parseFloat(style.paddingRight);
            return {
              id: element.id,
              visible,
              available,
              widest,
              fits: widest.width <= available + 0.5
            };
          });

          const canvasColours = (id) => {
            const canvas = document.getElementById(id);
            const context = canvas.getContext("2d");
            if (!context || canvas.width < 2 || canvas.height < 2) return 0;
            const data = context.getImageData(0, 0, canvas.width, canvas.height).data;
            const colours = new Set();
            const step = Math.max(4, Math.floor(data.length / 12000 / 4) * 4);
            for (let index = 0; index < data.length; index += step)
              colours.add(`${data[index]},${data[index + 1]},${data[index + 2]},${data[index + 3]}`);
            return colours.size;
          };

          return {
            root: [root.clientWidth, root.scrollWidth, root.clientHeight, root.scrollHeight],
            body: [body.clientWidth, body.scrollWidth, body.clientHeight, body.scrollHeight],
            pageScroll: [scrollX, scrollY],
            strip: [strip.clientWidth, strip.scrollWidth, strip.clientHeight, strip.scrollHeight],
            visibleModules: [...strip.querySelectorAll(".parameter-module")]
              .filter((module) => getComputedStyle(module).display !== "none").length,
            tabsDisplay: getComputedStyle(tabs).display,
            waterfallColours: canvasColours("waterfallCanvas"),
            spectrumColours: canvasColours("spectrumCanvas"),
            selects
          };
        }"""
    )


def validate_metrics(size, metrics, errors):
    width, height = size
    root_width, root_scroll_width, root_height, root_scroll_height = metrics["root"]
    body_width, body_scroll_width, body_height, body_scroll_height = metrics["body"]

    if root_width != root_scroll_width or root_height != root_scroll_height:
        errors.append(f"{width}x{height}: document root scrolls: {metrics['root']}")
    if body_width != body_scroll_width or body_height != body_scroll_height:
        errors.append(f"{width}x{height}: body scrolls: {metrics['body']}")
    if metrics["pageScroll"] != [0, 0]:
        errors.append(f"{width}x{height}: page has a non-zero scroll offset")
    if metrics["visibleModules"] != 5:
        errors.append(f"{width}x{height}: parameter strip has {metrics['visibleModules']} visible modules instead of 5")
    if metrics["waterfallColours"] <= 8 or metrics["spectrumColours"] <= 8:
        errors.append(f"{width}x{height}: analyzer canvas is blank or nearly blank")

    compact_expected = size == (320, 280)
    compact_visible = metrics["tabsDisplay"] != "none"
    if compact_visible != compact_expected:
        errors.append(f"{width}x{height}: compact tabs visibility is incorrect")

    for select in metrics["selects"]:
        if not select["visible"]:
            continue
        if not select["fits"]:
            errors.append(
                f"{width}x{height}: {select['id']} option '{select['widest']['text']}' "
                f"needs {select['widest']['width']:.1f}px but has {select['available']:.1f}px"
            )


def build_signal_payload(phase: int) -> dict[str, list[float]]:
    def shaped_values(baseline, peaks):
        values = []
        for index in range(192):
            value = baseline
            for centre, peak, width in peaks:
                distance = (index - centre) / width
                value = max(value, baseline + (peak - baseline) * math.exp(-0.5 * distance * distance))
            values.append(round(value, 4))
        return values

    return {
        "input": shaped_values(
            -88.0,
            ((38 + phase, -9.0, 5.5), (92 - phase, -18.0, 8.0), (149 + phase, -25.0, 6.0)),
        ),
        "output": shaped_values(
            -92.0,
            ((29 + phase, 3.0, 4.5), (76 - phase, -5.0, 7.0), (126 + phase, -11.0, 5.0), (166, -19.0, 8.5)),
        ),
    }


def emit_analyzer_frames(page, payloads, interval_ms=45):
    listener_counts = []
    for payload in payloads:
        listener_counts.append(
            page.evaluate("payload => window.__emitFromCpp('analyzerFrame', payload)", payload)
        )
        page.wait_for_timeout(interval_ms)
    return listener_counts


def collect_analyzer_response_state(page, label, compare_to=None):
    return page.evaluate(
        """({ label, compareTo }) => {
          window.__webuiVisualSnapshots = window.__webuiVisualSnapshots || new Map();

          const canvas = document.getElementById("waterfallCanvas");
          const context = canvas.getContext("2d");
          const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
          const previous = compareTo ? window.__webuiVisualSnapshots.get(compareTo) : null;
          const comparable = previous && previous.width === canvas.width && previous.height === canvas.height;
          const bandWidth = Math.min(canvas.width, Math.max(24, Math.round(canvas.width * 0.12)));
          const bandStart = canvas.width - bandWidth;
          let hash = 2166136261;
          let changedPixels = 0;
          let absoluteDelta = 0;
          let luminanceSum = 0;
          let maximumLuminance = 0;
          let brightPixels = 0;
          let bandLuminanceSum = 0;
          let bandMaximumLuminance = 0;
          let bandBrightPixels = 0;

          for (let offset = 0, pixel = 0; offset < pixels.length; offset += 4, pixel += 1) {
            const red = pixels[offset];
            const green = pixels[offset + 1];
            const blue = pixels[offset + 2];
            hash ^= red;
            hash = Math.imul(hash, 16777619);
            hash ^= green;
            hash = Math.imul(hash, 16777619);
            hash ^= blue;
            hash = Math.imul(hash, 16777619);

            const luminance = red * 0.2126 + green * 0.7152 + blue * 0.0722;
            luminanceSum += luminance;
            maximumLuminance = Math.max(maximumLuminance, luminance);
            if (luminance >= 145) brightPixels += 1;

            const x = pixel % canvas.width;
            if (x >= bandStart) {
              bandLuminanceSum += luminance;
              bandMaximumLuminance = Math.max(bandMaximumLuminance, luminance);
              if (luminance >= 145) bandBrightPixels += 1;
            }

            if (comparable) {
              const delta = Math.abs(red - previous.pixels[offset])
                + Math.abs(green - previous.pixels[offset + 1])
                + Math.abs(blue - previous.pixels[offset + 2]);
              if (delta > 0) changedPixels += 1;
              absoluteDelta += delta;
            }
          }

          window.__webuiVisualSnapshots.set(label, {
            width: canvas.width,
            height: canvas.height,
            pixels: new Uint8ClampedArray(pixels)
          });

          const readMeter = (meterId, peakId) => {
            const meter = document.getElementById(meterId);
            const rail = meter.parentElement;
            const peakOutput = document.getElementById(peakId);
            const railHeight = rail.getBoundingClientRect().height;
            const fillHeight = meter.getBoundingClientRect().height;
            const inlineLevel = Number.parseFloat(meter.style.getPropertyValue("--level"));
            const peakValue = Number.parseFloat(peakOutput.value || peakOutput.textContent);
            return {
              levelPercent: Number.isFinite(inlineLevel) ? inlineLevel : null,
              renderedPercent: railHeight > 0 ? fillHeight / railHeight * 100 : null,
              railHeight,
              fillHeight,
              peakValue: Number.isFinite(peakValue) ? peakValue : null,
              clipping: rail.classList.contains("is-clipping") || peakOutput.classList.contains("is-clipping")
            };
          };

          const pixelCount = canvas.width * canvas.height;
          const bandPixelCount = bandWidth * canvas.height;
          return {
            label,
            waterfall: {
              width: canvas.width,
              height: canvas.height,
              hash: (hash >>> 0).toString(16).padStart(8, "0"),
              advanceCount: Number(canvas.dataset.advanceCount || 0),
              changedPixels: comparable ? changedPixels : null,
              changedRatio: comparable ? changedPixels / Math.max(1, pixelCount) : null,
              absoluteDelta: comparable ? absoluteDelta : null,
              meanLuminance: luminanceSum / Math.max(1, pixelCount),
              maximumLuminance,
              brightPixels,
              rightBand: {
                width: bandWidth,
                meanLuminance: bandLuminanceSum / Math.max(1, bandPixelCount),
                maximumLuminance: bandMaximumLuminance,
                brightPixels: bandBrightPixels
              }
            },
            meters: {
              input: readMeter("inputMeter", "inputPeak"),
              output: readMeter("outputMeter", "outputPeak")
            },
            bridge: {
              status: document.getElementById("bridgeStatus").textContent.trim(),
              inboundAnalyzerFrames: window.__bridgeInboundEvents.filter((event) => event.name === "analyzerFrame").length,
              analyzerListenerCounts: window.__bridgeInboundEvents
                .filter((event) => event.name === "analyzerFrame")
                .map((event) => event.listenerCount),
              uiReadyEvents: window.__bridgeEvents.filter((event) => event.name === "uiReady").length
            }
          };
        }""",
        {"label": label, "compareTo": compare_to},
    )


def validate_analyzer_visual_response(browser, url, screenshot_dir, errors):
    page = browser.new_page(viewport={"width": 1000, "height": 650}, device_scale_factor=1)
    console_errors = []
    page_errors = []
    page.on("console", lambda message: console_errors.append(message.text) if message.type == "error" else None)
    page.on("pageerror", lambda error: page_errors.append(str(error)))
    page.add_init_script(MOCK_BRIDGE_SCRIPT)
    page.goto(url, wait_until="load")
    page.wait_for_timeout(180)

    artifacts = []
    baseline = collect_analyzer_response_state(page, "baseline")
    if screenshot_dir:
        path = screenshot_dir / "webui-native-no-signal.png"
        page.screenshot(path=str(path))
        artifacts.append(str(path))

    quiet_payload = {"input": [-66.0] * 192, "output": [-72.0] * 192}
    quiet_listener_counts = emit_analyzer_frames(page, [quiet_payload] * 5)
    page.wait_for_timeout(110)
    quiet = collect_analyzer_response_state(page, "quiet", "baseline")

    signal_payloads = [build_signal_payload(phase) for phase in (-3, -2, -1, 0, 1, 2, 3, 4)]
    signal_listener_counts = emit_analyzer_frames(page, signal_payloads)
    page.wait_for_timeout(110)
    signal = collect_analyzer_response_state(page, "signal", "quiet")

    if screenshot_dir:
        screenshots = (
            (page, "webui-native-signal.png"),
            (page.locator(".waterfall-pane"), "webui-native-waterfall-signal.png"),
            (page.locator(".mini-meters"), "webui-native-meters-signal.png"),
        )
        for target, name in screenshots:
            path = screenshot_dir / name
            target.screenshot(path=str(path))
            artifacts.append(str(path))

    page.close()

    result = {
        "baseline": baseline,
        "quiet": quiet,
        "signal": signal,
        "listenerCounts": quiet_listener_counts + signal_listener_counts,
        "artifacts": artifacts,
        "consoleErrors": console_errors,
        "pageErrors": page_errors,
    }

    if console_errors or page_errors:
        errors.append(f"native analyzer response emitted console/page errors: {console_errors + page_errors}")

    if baseline["bridge"]["status"] != "NATIVE" or baseline["bridge"]["uiReadyEvents"] != 1:
        errors.append(f"native analyzer response did not initialise the mock bridge: {baseline['bridge']}")
    if any(count != 1 for count in result["listenerCounts"]):
        errors.append(f"analyzerFrame was not delivered to exactly one frontend listener: {result['listenerCounts']}")

    baseline_waterfall = baseline["waterfall"]
    quiet_waterfall = quiet["waterfall"]
    signal_waterfall = signal["waterfall"]
    if quiet_waterfall["advanceCount"] <= baseline_waterfall["advanceCount"]:
        errors.append("waterfall did not advance after quiet analyzer frames")
    if signal_waterfall["advanceCount"] <= quiet_waterfall["advanceCount"]:
        errors.append("waterfall did not continue advancing after signal analyzer frames")
    if quiet_waterfall["hash"] == baseline_waterfall["hash"] or not quiet_waterfall["changedPixels"]:
        errors.append("waterfall pixels did not change after the first analyzer frames")
    if signal_waterfall["hash"] == quiet_waterfall["hash"]:
        errors.append("waterfall pixel hash did not change for a spectrally different frame")
    if (signal_waterfall["changedPixels"] or 0) < signal_waterfall["height"]:
        errors.append(f"too few waterfall pixels changed for the signal frame: {signal_waterfall}")
    quiet_band = quiet_waterfall["rightBand"]
    signal_band = signal_waterfall["rightBand"]
    if signal_band["meanLuminance"] <= quiet_band["meanLuminance"] + 5.0:
        errors.append(f"waterfall signal band did not become visibly brighter: quiet={quiet_band}, signal={signal_band}")
    if signal_band["brightPixels"] <= quiet_band["brightPixels"]:
        errors.append(f"waterfall signal band gained no bright pixels: quiet={quiet_band}, signal={signal_band}")

    for meter_name in ("input", "output"):
        quiet_meter = quiet["meters"][meter_name]
        signal_meter = signal["meters"][meter_name]
        if quiet_meter["railHeight"] <= 0 or signal_meter["railHeight"] <= 0:
            errors.append(f"{meter_name} dB meter is not visibly laid out")
            continue
        if signal_meter["levelPercent"] is None or quiet_meter["levelPercent"] is None:
            errors.append(f"{meter_name} dB meter did not expose a numeric level")
            continue
        if signal_meter["levelPercent"] <= quiet_meter["levelPercent"] + 25.0:
            errors.append(f"{meter_name} dB meter did not visibly rise: quiet={quiet_meter}, signal={signal_meter}")
        if signal_meter["fillHeight"] <= quiet_meter["fillHeight"]:
            errors.append(f"{meter_name} dB meter rendered height did not rise: quiet={quiet_meter}, signal={signal_meter}")

    if abs(signal["meters"]["input"]["peakValue"] + 9.0) > 0.15:
        errors.append(f"input peak readout did not receive the injected -9 dB peak: {signal['meters']['input']}")
    if abs(signal["meters"]["output"]["peakValue"] - 3.0) > 0.15 or not signal["meters"]["output"]["clipping"]:
        errors.append(f"output peak/clipping state did not receive the injected +3 dB peak: {signal['meters']['output']}")

    return result


def validate_native_surface(browser, url, errors, screenshot_dir=None):
    page = browser.new_page(viewport={"width": 320, "height": 280}, device_scale_factor=1)
    page.add_init_script(MOCK_BRIDGE_SCRIPT)
    page.goto(url, wait_until="load")
    page.wait_for_timeout(350)

    analyzer_payload = {"input": [-36.0] * 192, "output": [-24.0] * 192}
    page.evaluate("payload => window.__emitFromCpp('analyzerFrame', payload)", analyzer_payload)
    page.wait_for_timeout(80)
    before_hide = int(page.locator("#waterfallCanvas").get_attribute("data-advance-count") or "0")
    page.evaluate("window.__emitFromCpp('surfaceVisibility', { visible: false })")
    hidden_start = int(page.locator("#waterfallCanvas").get_attribute("data-advance-count") or "0")
    page.wait_for_timeout(160)
    after_hide = int(page.locator("#waterfallCanvas").get_attribute("data-advance-count") or "0")
    hidden_marker = page.locator("html").get_attribute("data-surface-visible")
    page.evaluate("window.__emitFromCpp('surfaceVisibility', { visible: true })")
    page.evaluate("payload => window.__emitFromCpp('analyzerFrame', payload)", analyzer_payload)
    page.wait_for_timeout(120)
    after_show = int(page.locator("#waterfallCanvas").get_attribute("data-advance-count") or "0")
    shown_marker = page.locator("html").get_attribute("data-surface-visible")

    result = page.evaluate(
        """() => {
          const test = (target, event) => {
            target.dispatchEvent(event);
            return event.defaultPrevented;
          };
          const parameterValues = () => [...document.querySelectorAll("[data-param]")].map((element) => ({
            id: element.dataset.param,
            value: element.value ?? element.getAttribute("aria-pressed")
          }));
          const waterfall = document.getElementById("waterfallCanvas");
          const before = {
            values: JSON.stringify(parameterValues()),
            advances: waterfall.dataset.advanceCount,
            bridgeCount: window.__bridgeEvents.length
          };
          document.getElementById("compactControlTab").click();
          document.getElementById("compactAnalyzeTab").click();
          const after = {
            values: JSON.stringify(parameterValues()),
            advances: waterfall.dataset.advanceCount,
            gestures: window.__bridgeEvents.filter((event) => event.name === "parameterGesture").length
          };

          return {
            nativeClass: document.documentElement.classList.contains("native-surface"),
            guards: {
              contextmenu: test(document, new MouseEvent("contextmenu", { bubbles: true, cancelable: true })),
              selectstart: test(document, new Event("selectstart", { bubbles: true, cancelable: true })),
              dragstart: test(document, new Event("dragstart", { bubbles: true, cancelable: true })),
              ctrlR: test(document, new KeyboardEvent("keydown", { key: "r", ctrlKey: true, bubbles: true, cancelable: true })),
              f5: test(document, new KeyboardEvent("keydown", { key: "F5", bubbles: true, cancelable: true })),
              ctrlWheel: test(window, new WheelEvent("wheel", { ctrlKey: true, bubbles: true, cancelable: true })),
              plainTab: test(document, new KeyboardEvent("keydown", { key: "Tab", bubbles: true, cancelable: true }))
            },
            tabIsolation: {
              valuesUnchanged: before.values === after.values,
              advancesUnchanged: before.advances === after.advances,
              gestures: after.gestures
            }
          };
        }"""
    )

    page.evaluate("window.__bridgeEvents.length = 0")
    page.evaluate(
        "payload => window.__emitFromCpp('parameterState', payload)",
        {"values": {"mode": 23, "pitchRoot": 9, "pitchScale": 1, "amount": 0.7, "freeze": 1}, "sampleRate": 48000},
    )
    page.wait_for_timeout(40)
    pitch_map = page.evaluate(
        """() => {
          const root = document.getElementById("pitchRoot");
          const scale = document.getElementById("pitchScale");
          return {
            axisHidden: document.getElementById("axisModule").hidden,
            moduleHidden: document.getElementById("pitchMapModule").hidden,
            rootDisabled: root.disabled,
            scaleDisabled: scale.disabled,
            rootValue: root.value,
            scaleValue: scale.value,
            readoutHidden: document.getElementById("pitchMapReadout").hidden,
            readout: document.getElementById("pitchMapReadout").textContent
          };
        }"""
    )
    if screenshot_dir:
        page.locator("#compactControlTab").click()
        page.wait_for_timeout(40)
        page.screenshot(path=str(screenshot_dir / "webui-native-pitchmap-320x280.png"))
        page.locator("#compactAnalyzeTab").click()
    page.evaluate(
        """() => {
          const root = document.getElementById("pitchRoot");
          root.value = "5";
          root.dispatchEvent(new Event("change", { bubbles: true }));
        }"""
    )
    pitch_gestures = page.evaluate(
        """() => window.__bridgeEvents
          .filter((event) => event.name === "parameterGesture" && event.payload.id === "pitchRoot")
          .map((event) => ({ phase: event.payload.phase, value: event.payload.value }))"""
    )

    canvas_before_glitch = page.evaluate("document.getElementById('waterfallCanvas').toDataURL()")
    page.evaluate(
        "payload => window.__emitFromCpp('parameterState', payload)",
        {"values": {"mode": 22, "pivotHz": 1000, "widthQ": 2, "quality": 1}, "sampleRate": 48000},
    )
    page.wait_for_timeout(40)
    glitch = page.evaluate(
        """() => {
          const overlay = document.getElementById("glitchSelection");
          return {
            hidden: overlay.hidden,
            axisHidden: document.getElementById("axisModule").hidden,
            pitchModuleHidden: document.getElementById("pitchMapModule").hidden,
            pitchReadoutHidden: document.getElementById("pitchMapReadout").hidden,
            lowHz: Number(overlay.dataset.lowHz),
            highHz: Number(overlay.dataset.highHz),
            height: parseFloat(overlay.style.height),
            label: document.getElementById("glitchSelectionLabel").textContent
          };
        }"""
    )
    if screenshot_dir:
        page.screenshot(path=str(screenshot_dir / "webui-native-glitch-320x280.png"))
    canvas_after_glitch = page.evaluate("document.getElementById('waterfallCanvas').toDataURL()")
    result["pitchMap"] = pitch_map
    result["pitchRootGestures"] = pitch_gestures
    result["glitchOverlay"] = glitch
    result["modeOverlayCanvasUnchanged"] = canvas_before_glitch == canvas_after_glitch
    page.close()

    if not result["nativeClass"]:
        errors.append("native surface marker was not installed")
    for name, prevented in result["guards"].items():
        expected = name != "plainTab"
        if prevented != expected:
            errors.append(f"native guard {name} returned {prevented}, expected {expected}")
    isolation = result["tabIsolation"]
    if not isolation["valuesUnchanged"] or not isolation["advancesUnchanged"] or isolation["gestures"] != 0:
        errors.append(f"compact tab changed plugin state or waterfall history: {isolation}")

    lifecycle = {
        "beforeHide": before_hide,
        "hiddenStart": hidden_start,
        "afterHide": after_hide,
        "afterShow": after_show,
        "hiddenMarker": hidden_marker,
        "shownMarker": shown_marker,
    }
    result["surfaceLifecycle"] = lifecycle
    if after_hide != hidden_start or after_show <= after_hide or hidden_marker != "false" or shown_marker != "true":
        errors.append(f"native surface animation lifecycle failed: {lifecycle}")

    if not pitch_map["axisHidden"] or pitch_map["moduleHidden"] or pitch_map["rootDisabled"] or pitch_map["scaleDisabled"]:
        errors.append(f"Pitch Map did not replace AXIS with active selectors: {pitch_map}")
    if pitch_map["rootValue"] != "9" or pitch_map["scaleValue"] != "1" or pitch_map["readoutHidden"] or "A MINOR" not in pitch_map["readout"]:
        errors.append(f"Pitch Map state/readout did not follow host state: {pitch_map}")
    gesture_phases = [event["phase"] for event in pitch_gestures]
    if gesture_phases != ["begin", "value", "end"] or pitch_gestures[1].get("value") != 5:
        errors.append(f"pitchRoot did not emit a complete automation gesture: {pitch_gestures}")
    if glitch["hidden"] or glitch["axisHidden"] or not glitch["pitchModuleHidden"] or not glitch["pitchReadoutHidden"]:
        errors.append(f"Glitch/Pitch Map conditional surfaces are incorrect: {glitch}")
    if not (0 < glitch["lowHz"] < glitch["highHz"] <= 24000) or glitch["height"] <= 0 or "GLITCH BAND" not in glitch["label"]:
        errors.append(f"Glitch selected-band overlay is invalid: {glitch}")
    if canvas_before_glitch != canvas_after_glitch:
        errors.append("mode overlays modified waterfallCanvas pixels instead of remaining separate DOM guides")

    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--browser", help="Path to Chrome or Edge")
    parser.add_argument("--screenshots-dir", type=Path)
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    url = (root / "vst3" / "WebUI" / "index.html").as_uri()
    browser_path = find_browser(args.browser)
    screenshot_dir = args.screenshots_dir.resolve() if args.screenshots_dir else None
    if screenshot_dir:
        screenshot_dir.mkdir(parents=True, exist_ok=True)

    report = {"browser": str(browser_path), "viewports": [], "native": None, "analyzerResponse": None}
    failures = []

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=str(browser_path), headless=True)
        for width, height in VIEWPORTS:
            page = browser.new_page(viewport={"width": width, "height": height}, device_scale_factor=1)
            console_errors = []
            page_errors = []
            page.on("console", lambda message, target=console_errors: target.append(message.text) if message.type == "error" else None)
            page.on("pageerror", lambda error, target=page_errors: target.append(str(error)))
            page.goto(url, wait_until="load")
            page.wait_for_timeout(900)
            metrics = collect_metrics(page)
            validate_metrics((width, height), metrics, failures)
            if console_errors or page_errors:
                failures.append(f"{width}x{height}: console/page errors: {console_errors + page_errors}")
            if screenshot_dir:
                page.screenshot(path=str(screenshot_dir / f"webui-{width}x{height}.png"))
                if (width, height) == (320, 280):
                    page.locator("#compactControlTab").click()
                    page.wait_for_timeout(100)
                    page.screenshot(path=str(screenshot_dir / "webui-320x280-control.png"))
                    page.locator("#compactAnalyzeTab").click()

            page.locator("#mode").select_option("23")
            if (width, height) == (320, 280):
                page.locator("#compactControlTab").click()
            page.wait_for_timeout(100)
            pitch_map_metrics = collect_metrics(page)
            validate_metrics((width, height), pitch_map_metrics, failures)
            if screenshot_dir:
                page.screenshot(path=str(screenshot_dir / f"webui-{width}x{height}-pitchmap.png"))
            report["viewports"].append({
                "size": [width, height],
                "metrics": metrics,
                "pitchMapMetrics": pitch_map_metrics,
                "consoleErrors": console_errors,
                "pageErrors": page_errors,
            })
            page.close()

        report["native"] = validate_native_surface(browser, url, failures, screenshot_dir)
        report["analyzerResponse"] = validate_analyzer_visual_response(
            browser, url, screenshot_dir, failures
        )
        browser.close()

    print(json.dumps(report, ensure_ascii=False, indent=2))
    if failures:
        print("WebUI runtime validation failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print("WebUI runtime validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
