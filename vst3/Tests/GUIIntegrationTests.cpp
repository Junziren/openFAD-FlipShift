#include "PluginProcessor.h"
#include "PresetFormat.h"

#include <JuceHeader.h>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace
{
constexpr double harnessSampleRate = 48000.0;
constexpr int blockSize = 256;

class HarnessWindow final : public juce::DocumentWindow
{
public:
    HarnessWindow()
        : DocumentWindow("openFAD FlipShift WebView integration test",
                         juce::Colour(0xff202226),
                         juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(false, false);
    }

    void closeButtonPressed() override {}
};

juce::WebBrowserComponent* findBrowser(juce::Component& component)
{
    if (auto* browser = dynamic_cast<juce::WebBrowserComponent*>(&component))
        return browser;

    for (auto* child : component.getChildren())
        if (child != nullptr)
            if (auto* browser = findBrowser(*child))
                return browser;

    return nullptr;
}

struct EvaluationState
{
    bool completed = false;
    std::optional<juce::var> result;
    juce::String error;
};

void pumpMessages(int milliseconds)
{
    if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
        manager->runDispatchLoopUntil(juce::jmax(1, milliseconds));
}

std::optional<juce::var> evaluateJavascript(juce::WebBrowserComponent& browser,
                                            const juce::String& script,
                                            int timeoutMs,
                                            juce::String& error)
{
    auto state = std::make_shared<EvaluationState>();
    browser.evaluateJavascript(script, [state](juce::WebBrowserComponent::EvaluationResult evaluation)
    {
        if (const auto* value = evaluation.getResult())
            state->result = *value;
        else if (const auto* failure = evaluation.getError())
            state->error = failure->message;

        state->completed = true;
    });

    const auto deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(timeoutMs);
    while (!state->completed && juce::Time::getMillisecondCounterHiRes() < deadline)
        pumpMessages(5);

    if (!state->completed)
    {
        error = "JavaScript evaluation timed out";
        return std::nullopt;
    }

    if (state->result.has_value())
        return state->result;

    error = state->error.isNotEmpty() ? state->error : "JavaScript evaluation failed";
    return std::nullopt;
}

std::optional<juce::var> evaluateJson(juce::WebBrowserComponent& browser,
                                      const juce::String& script,
                                      int timeoutMs,
                                      juce::String& error)
{
    const auto result = evaluateJavascript(browser, script, timeoutMs, error);
    if (!result.has_value())
        return std::nullopt;

    const auto parsed = juce::JSON::parse(result->toString());
    if (!parsed.isObject())
    {
        error = "JavaScript returned invalid JSON: " + result->toString();
        return std::nullopt;
    }

    return parsed;
}

std::optional<juce::var> waitForJson(juce::WebBrowserComponent& browser,
                                     const juce::String& script,
                                     int timeoutMs,
                                     const std::function<bool(const juce::var&)>& predicate,
                                     juce::String& error)
{
    std::optional<juce::var> latest;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(timeoutMs);

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        juce::String evaluationError;
        if (auto result = evaluateJson(browser, script, 1000, evaluationError))
        {
            latest = std::move(result);
            if (predicate(*latest))
                return latest;
        }
        else if (evaluationError.isNotEmpty())
        {
            error = evaluationError;
        }

        pumpMessages(30);
    }

    if (!latest.has_value() && error.isEmpty())
        error = "Timed out before JavaScript returned a JSON state";

    return latest;
}

double numberProperty(const juce::var& object, const char* name, double fallback = 0.0)
{
    const auto value = object.getProperty(name, fallback);
    return value.isDouble() || value.isInt() || value.isInt64()
        ? static_cast<double>(value)
        : fallback;
}

juce::String stringProperty(const juce::var& object, const char* name)
{
    return object.getProperty(name, {}).toString();
}

bool boolProperty(const juce::var& object, const char* name)
{
    return static_cast<bool>(object.getProperty(name, false));
}

bool setPlainParameter(openfad::flipshift::OpenFADFlipShiftAudioProcessor& processor,
                       const char* parameterID,
                       float plainValue)
{
    auto* parameter = processor.getState().getParameter(parameterID);
    if (parameter == nullptr)
        return false;

    const auto normalised = parameter->convertTo0to1(plainValue);
    if (!std::isfinite(normalised))
        return false;

    parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalised));
    return true;
}

class DeterministicAudioDriver
{
public:
    explicit DeterministicAudioDriver(juce::AudioProcessor& processorIn)
        : processor(processorIn), buffer(2, blockSize)
    {
    }

    void processNextBlock()
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto left = 0.30 * std::sin(phaseA)
                            + 0.14 * std::sin(phaseB)
                            + 0.07 * std::sin(phaseC);
            const auto right = 0.27 * std::sin(phaseA + 0.17)
                             + 0.12 * std::sin(phaseB + 0.39)
                             + 0.09 * std::sin(phaseC + 0.71);

            buffer.setSample(0, sample, static_cast<float>(left));
            buffer.setSample(1, sample, static_cast<float>(right));

            advancePhase(phaseA, 233.0);
            advancePhase(phaseB, 1197.0);
            advancePhase(phaseC, 5279.0);
        }

        midi.clear();
        processor.processBlock(buffer, midi);
    }

private:
    static void advancePhase(double& phase, double frequency)
    {
        constexpr auto twoPi = juce::MathConstants<double>::twoPi;
        phase += twoPi * frequency / harnessSampleRate;
        if (phase >= twoPi)
            phase -= twoPi;
    }

    juce::AudioProcessor& processor;
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;
    double phaseA = 0.0;
    double phaseB = 0.0;
    double phaseC = 0.0;
};

void runAudioAndMessages(DeterministicAudioDriver& audio, int durationMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(durationMs);
    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        audio.processNextBlock();
        pumpMessages(5);
    }
}

const juce::String readinessScript = R"JS(
(() => JSON.stringify({
  ready: document.readyState === "complete",
  nativeBridge: Boolean(window.__JUCE__ && window.__JUCE__.backend),
  bridgeStatus: document.getElementById("bridgeStatus")?.textContent || "",
  surfaceVisible: document.documentElement.dataset.surfaceVisible || "",
  visibilityState: document.visibilityState,
  waterfallWidth: document.getElementById("waterfallCanvas")?.width || 0,
  waterfallHeight: document.getElementById("waterfallCanvas")?.height || 0
}))())JS";

const juce::String uiContractScript = R"JS(
(() => {
  const mode = document.getElementById("mode");
  const modeOptions = Array.from(mode?.options || []);
  const rootOptions = Array.from(document.getElementById("pitchRoot")?.options || []);
  const scaleOptions = Array.from(document.getElementById("pitchScale")?.options || []);
  return JSON.stringify({
    processOptionCount: modeOptions.length,
    processValuesSequential: modeOptions.every((option, index) => Number(option.value) === index),
    firstProcessValue: Number(modeOptions[0]?.value ?? -1),
    lastProcessValue: Number(modeOptions.at(-1)?.value ?? -1),
    pitchRootOptionCount: rootOptions.length,
    pitchRootValuesSequential: rootOptions.every((option, index) => Number(option.value) === index),
    firstPitchRoot: rootOptions[0]?.textContent || "",
    lastPitchRoot: rootOptions.at(-1)?.textContent || "",
    pitchScaleOptionCount: scaleOptions.length,
    firstPitchScale: scaleOptions[0]?.textContent || "",
    lastPitchScale: scaleOptions.at(-1)?.textContent || ""
  });
})())JS";

const juce::String installProbeScript = R"JS(
(() => {
  const backend = window.__JUCE__ && window.__JUCE__.backend;
  if (!backend) return JSON.stringify({ installed: false });
  if (window.__openfadIntegrationProbe?.analyzerToken)
    backend.removeEventListener(window.__openfadIntegrationProbe.analyzerToken);
  if (window.__openfadIntegrationProbe?.parameterToken)
    backend.removeEventListener(window.__openfadIntegrationProbe.parameterToken);

  const probe = {
    eventCount: 0,
    parameterEventCount: 0,
    inputCount: 0,
    outputCount: 0,
    maxInputDb: -96,
    maxOutputDb: -96,
    lastParameterState: {},
    analyzerToken: 0,
    parameterToken: 0
  };
  probe.analyzerToken = backend.addEventListener("analyzerFrame", (payload) => {
    probe.eventCount += 1;
    const input = payload && Array.isArray(payload.input) ? payload.input : [];
    const output = payload && Array.isArray(payload.output) ? payload.output : [];
    probe.inputCount = input.length;
    probe.outputCount = output.length;
    for (const value of input) if (Number.isFinite(Number(value))) probe.maxInputDb = Math.max(probe.maxInputDb, Number(value));
    for (const value of output) if (Number.isFinite(Number(value))) probe.maxOutputDb = Math.max(probe.maxOutputDb, Number(value));
  });
  probe.waterfallToken = backend.addEventListener("waterfallFrame", (payload) => {
    probe.waterfallEvents = (probe.waterfallEvents || 0) + 1;
    probe.waterfallBins = payload.output.length;
    probe.waterfallFftSize = payload.fftSize;
    probe.waterfallPeak = Math.max(...payload.output);
  });
  probe.parameterToken = backend.addEventListener("parameterState", (payload) => {
    probe.parameterEventCount += 1;
    probe.lastParameterState = payload && payload.values ? { ...payload.values } : {};
  });
  window.__openfadIntegrationProbe = probe;
  return JSON.stringify({ installed: true });
})())JS";

const juce::String pitchMapStateScript = R"JS(
(() => {
  const probe = window.__openfadIntegrationProbe || {};
  const parameterState = probe.lastParameterState || {};
  const mode = document.getElementById("mode");
  const root = document.getElementById("pitchRoot");
  const scale = document.getElementById("pitchScale");
  const axisModule = document.getElementById("axisModule");
  const pitchMapModule = document.getElementById("pitchMapModule");
  const pitchMapReadout = document.getElementById("pitchMapReadout");
  return JSON.stringify({
    parameterEventCount: Number(probe.parameterEventCount || 0),
    eventMode: Number(parameterState.mode ?? -1),
    eventPitchRoot: Number(parameterState.pitchRoot ?? -1),
    eventPitchScale: Number(parameterState.pitchScale ?? -1),
    modeValue: Number(mode?.value ?? -1),
    modeText: mode?.selectedOptions?.[0]?.textContent || "",
    pitchRootValue: Number(root?.value ?? -1),
    pitchRootText: root?.selectedOptions?.[0]?.textContent || "",
    pitchScaleValue: Number(scale?.value ?? -1),
    pitchScaleText: scale?.selectedOptions?.[0]?.textContent || "",
    pitchRootDisabled: Boolean(root?.disabled),
    pitchScaleDisabled: Boolean(scale?.disabled),
    axisHidden: Boolean(axisModule?.hidden),
    axisAriaHidden: axisModule?.getAttribute("aria-hidden") || "",
    pitchMapModuleHidden: Boolean(pitchMapModule?.hidden),
    pitchMapModuleAriaHidden: pitchMapModule?.getAttribute("aria-hidden") || "",
    pitchMapReadoutHidden: Boolean(pitchMapReadout?.hidden),
    pitchMapReadoutText: pitchMapReadout?.textContent || "",
    glitchSelectionHidden: Boolean(document.getElementById("glitchSelection")?.hidden),
    pivotGuideDisplay: getComputedStyle(document.getElementById("pivotGuide")).display
  });
})())JS";

const juce::String glitchStateScript = R"JS(
(() => {
  const probe = window.__openfadIntegrationProbe || {};
  const parameterState = probe.lastParameterState || {};
  const mode = document.getElementById("mode");
  const selection = document.getElementById("glitchSelection");
  const selectionBounds = selection?.getBoundingClientRect();
  return JSON.stringify({
    parameterEventCount: Number(probe.parameterEventCount || 0),
    eventMode: Number(parameterState.mode ?? -1),
    eventPivotHz: Number(parameterState.pivotHz ?? -1),
    eventWidthQ: Number(parameterState.widthQ ?? -1),
    modeValue: Number(mode?.value ?? -1),
    modeText: mode?.selectedOptions?.[0]?.textContent || "",
    glitchSelectionHidden: Boolean(selection?.hidden),
    glitchSelectionDisplay: selection ? getComputedStyle(selection).display : "none",
    glitchSelectionWidth: Number(selectionBounds?.width || 0),
    glitchSelectionHeight: Number(selectionBounds?.height || 0),
    glitchSelectionTopPercent: Number.parseFloat(selection?.style.top || "-1"),
    glitchSelectionHeightPercent: Number.parseFloat(selection?.style.height || "-1"),
    glitchLowHz: Number(selection?.dataset.lowHz || -1),
    glitchHighHz: Number(selection?.dataset.highHz || -1),
    glitchLabel: document.getElementById("glitchSelectionLabel")?.textContent || "",
    axisHidden: Boolean(document.getElementById("axisModule")?.hidden),
    pitchMapModuleHidden: Boolean(document.getElementById("pitchMapModule")?.hidden),
    pitchMapReadoutHidden: Boolean(document.getElementById("pitchMapReadout")?.hidden),
    pivotGuideDisplay: getComputedStyle(document.getElementById("pivotGuide")).display,
    pivotGuideLabel: document.getElementById("pivotGuideLabel")?.textContent || ""
  });
})())JS";

const juce::String snapshotScript = R"JS(
(() => {
  const canvas = document.getElementById("waterfallCanvas");
  const inputPeak = document.getElementById("inputPeak");
  const outputPeak = document.getElementById("outputPeak");
  const inputMeter = document.getElementById("inputMeter");
  const outputMeter = document.getElementById("outputMeter");
  const probe = window.__openfadIntegrationProbe || {};
  let uniqueColours = 0;
  let brightPixels = 0;
  let sampledPixels = 0;
  let pixelHash = 2166136261;

  if (canvas && canvas.width > 0 && canvas.height > 0) {
    const context = canvas.getContext("2d");
    const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
    const colours = new Set();
    const stridePixels = Math.max(1, Math.floor((canvas.width * canvas.height) / 60000));
    for (let pixel = 0; pixel < canvas.width * canvas.height; pixel += stridePixels) {
      const offset = pixel * 4;
      const red = pixels[offset];
      const green = pixels[offset + 1];
      const blue = pixels[offset + 2];
      colours.add((red << 16) | (green << 8) | blue);
      if (Math.max(red, green, blue) >= 96) brightPixels += 1;
      pixelHash = Math.imul(pixelHash ^ red, 16777619);
      pixelHash = Math.imul(pixelHash ^ green, 16777619);
      pixelHash = Math.imul(pixelHash ^ blue, 16777619);
      sampledPixels += 1;
    }
    uniqueColours = colours.size;
  }

  return JSON.stringify({
    waterfallEvents: Number(probe.waterfallEvents || 0),
    waterfallBins: Number(probe.waterfallBins || 0),
    waterfallFftSize: Number(probe.waterfallFftSize || 0),
    waterfallPeak: Number(probe.waterfallPeak ?? -96),
    eventCount: Number(probe.eventCount || 0),
    inputCount: Number(probe.inputCount || 0),
    outputCount: Number(probe.outputCount || 0),
    maxInputDb: Number(probe.maxInputDb ?? -96),
    maxOutputDb: Number(probe.maxOutputDb ?? -96),
    advanceCount: Number(canvas?.dataset.advanceCount || 0),
    fullRenderCount: Number(canvas?.dataset.fullRenderCount || 0),
    waterfallWidth: Number(canvas?.width || 0),
    waterfallHeight: Number(canvas?.height || 0),
    uniqueColours,
    brightPixels,
    sampledPixels,
    pixelHash: pixelHash >>> 0,
    inputPeakDb: Number(inputPeak?.value ?? -96),
    outputPeakDb: Number(outputPeak?.value ?? -96),
    inputLevelPercent: Number.parseFloat(inputMeter?.style.getPropertyValue("--level") || "0"),
    outputLevelPercent: Number.parseFloat(outputMeter?.style.getPropertyValue("--level") || "0"),
    bridgeStatus: document.getElementById("bridgeStatus")?.textContent || "",
    surfaceVisible: document.documentElement.dataset.surfaceVisible || "",
    visibilityState: document.visibilityState
  });
})())JS";

class Expectations
{
public:
    void expect(bool condition, const std::string& message)
    {
        if (condition)
        {
            std::cout << "PASS: " << message << '\n';
            return;
        }

        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }

    int getFailureCount() const noexcept { return failures; }

private:
    int failures = 0;
};
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Expectations expectations;

    try
    {
        openfad::flipshift::OpenFADFlipShiftAudioProcessor processor;
        using namespace openfad::flipshift;
        expectations.expect(*processor.getState().getRawParameterValue(ParameterIDs::quality) == 2.0f,
                            "new processors default to High FFT");
        const auto initial = processor.initialPreset();
        auto named = initial.clone();
        named.getDynamicObject()->setProperty("name", juce::String::fromUTF8("中文预设测试"));
        named["parameters"].getDynamicObject()->setProperty(ParameterIDs::shiftHz, 327.25);
        named["parameters"].getDynamicObject()->setProperty(ParameterIDs::quality, 1);
        const auto testId = juce::Uuid().toString().removeCharacters("-");
        processor.getState().getParameter(ParameterIDs::bypass)->setValueNotifyingHost(1.0f);
        processor.getState().getParameter(ParameterIDs::freeze)->setValueNotifyingHost(1.0f);
        expectations.expect(processor.applyPreset(named, testId).wasOk(), "valid preset applies");
        expectations.expect(std::abs(*processor.getState().getRawParameterValue(ParameterIDs::shiftHz) - 327.25f) < 0.02f
            && *processor.getState().getRawParameterValue(ParameterIDs::quality) == 1.0f,
            "preset restores physical values and quality");
        expectations.expect(*processor.getState().getRawParameterValue(ParameterIDs::bypass) == 1.0f
            && *processor.getState().getRawParameterValue(ParameterIDs::freeze) == 0.0f,
            "preset preserves bypass and releases transient freeze");
        expectations.expect(!static_cast<bool>(processor.presetStatus()["dirty"]), "loaded preset is clean");
        auto broken = named.clone();
        broken["parameters"].getDynamicObject()->setProperty(ParameterIDs::shiftHz, 200.0);
        broken["parameters"].getDynamicObject()->setProperty(ParameterIDs::pitchRoot, 12);
        expectations.expect(processor.applyPreset(broken, "init").failed()
            && std::abs(*processor.getState().getRawParameterValue(ParameterIDs::shiftHz) - 327.25f) < 0.02f,
            "invalid trailing field leaves all current parameters unchanged");
        broken = named.clone(); broken.getDynamicObject()->setProperty("version", 2);
        expectations.expect(processor.applyPreset(broken, "init").failed(), "future preset version rejected");
        broken = named.clone(); broken["parameters"].getDynamicObject()->removeProperty(ParameterIDs::scale);
        expectations.expect(processor.applyPreset(broken, "init").failed(), "missing field rejected");
        broken = named.clone(); broken["parameters"].getDynamicObject()->setProperty(ParameterIDs::amount, std::numeric_limits<double>::infinity());
        expectations.expect(processor.applyPreset(broken, "init").failed(), "non-finite field rejected");
        broken = named.clone(); broken["parameters"].getDynamicObject()->setProperty(ParameterIDs::mode, 1.5);
        expectations.expect(processor.applyPreset(broken, "init").failed(), "fractional enum rejected");
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(testId + ".flipshift");
        expectations.expect(presets::write(file, named).wasOk(), "atomic UTF-8 preset save succeeds");
        juce::var reread;
        expectations.expect(presets::read(file, reread, processor.getState()).wasOk()
            && reread["name"].toString() == named["name"].toString(), "preset JSON round trip preserves Chinese names");
        juce::MemoryBlock saved;
        processor.getStateInformation(saved);
        file.deleteFile();
        processor.applyPreset(initial, "init");
        processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        expectations.expect(processor.presetStatus()["id"].toString() == testId
            && *processor.getState().getRawParameterValue(ParameterIDs::quality) == 1.0f,
            "DAW state recalls preset identity and saved quality without the preset file");
        processor.getState().getParameter(ParameterIDs::mix)->setValueNotifyingHost(0.9f);
        expectations.expect(static_cast<bool>(processor.presetStatus()["dirty"]), "host parameter change marks preset modified");
        auto oldState = processor.getState().copyState();
        oldState.removeProperty("flipshiftPreset", nullptr); oldState.removeProperty("flipshiftPresetId", nullptr);
        oldState.setProperty("legacy", true, nullptr);
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary(*oldState.createXml(), legacy);
        processor.setStateInformation(legacy.getData(), static_cast<int>(legacy.getSize()));
        expectations.expect(*processor.getState().getRawParameterValue(ParameterIDs::quality) == 1.0f,
                            "legacy host state retains Normal quality");
        processor.applyPreset(initial, "init");
        processor.getState().getParameter(ParameterIDs::bypass)->setValueNotifyingHost(0.0f);
        processor.setPlayConfigDetails(2, 2, harnessSampleRate, blockSize);
        processor.prepareToPlay(harnessSampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        expectations.expect(editor != nullptr, "processor created its real editor");
        if (editor == nullptr)
            return 1;

        auto* browser = findBrowser(*editor);
        expectations.expect(browser != nullptr, "editor contains a real JUCE WebBrowserComponent");
        if (browser == nullptr)
            return 1;

        HarnessWindow window;
        window.setContentNonOwned(editor.get(), false);
        window.centreWithSize(editor->getWidth(), editor->getHeight());
        window.setVisible(true);
        window.toFront(false);

        std::optional<juce::var> readyState;
        juce::String javascriptError;
        const auto readinessDeadline = juce::Time::getMillisecondCounterHiRes() + 15000.0;
        while (juce::Time::getMillisecondCounterHiRes() < readinessDeadline)
        {
            javascriptError.clear();
            readyState = evaluateJson(*browser, readinessScript, 1500, javascriptError);
            if (readyState.has_value()
                && boolProperty(*readyState, "ready")
                && boolProperty(*readyState, "nativeBridge")
                && stringProperty(*readyState, "bridgeStatus") == juce::String::fromUTF8("已连接")
                && stringProperty(*readyState, "surfaceVisible") == "true")
                break;

            pumpMessages(50);
        }

        expectations.expect(readyState.has_value(), "WebView2 returned a DOM readiness result");
        if (!readyState.has_value())
        {
            std::cerr << "JavaScript error: " << javascriptError << '\n';
            return 1;
        }

        expectations.expect(boolProperty(*readyState, "ready"), "embedded WebUI finished loading");
        expectations.expect(boolProperty(*readyState, "nativeBridge"), "JUCE native bridge exists in the actual WebView2 DOM");
        expectations.expect(stringProperty(*readyState, "bridgeStatus") == juce::String::fromUTF8("已连接"), "frontend selected native mode rather than preview data");
        expectations.expect(stringProperty(*readyState, "surfaceVisible") == "true", "frontend received visible native-surface state");
        expectations.expect(stringProperty(*readyState, "visibilityState") == "visible", "WebView document is visible to requestAnimationFrame");

        javascriptError.clear();
        const auto nativePresetStart = evaluateJson(*browser, R"JS((() => {
          window.__presetTestResult = null;
          window.__presetTestToken = window.__JUCE__.backend.addEventListener("presetResult", p => { window.__presetTestResult = p; });
          window.__JUCE__.backend.emitEvent("presetCommand", {action: "saveAs", name: "Native bridge QA", requestId: 9901});
          return JSON.stringify({sent: true});
        })())JS", 2000, javascriptError);
        pumpMessages(200);
        const auto nativePresetResult = evaluateJson(*browser, "JSON.stringify(window.__presetTestResult || {})", 2000, javascriptError);
        expectations.expect(nativePresetStart.has_value() && nativePresetResult.has_value()
            && boolProperty(*nativePresetResult, "ok"), "real WebView preset command saves through the C++ bridge");
        const auto savedPresetId = processor.presetStatus()["id"].toString();
        expectations.expect(presets::validId(savedPresetId), "native save assigns a unique user preset id");
        if (presets::validId(savedPresetId))
        {
            const auto savedFile = presets::directory().getChildFile(savedPresetId + ".flipshift");
            juce::var savedDocument;
            expectations.expect(presets::read(savedFile, savedDocument, processor.getState()).wasOk(),
                                "native save creates a valid .flipshift JSON file");
            processor.applyPreset(processor.initialPreset(), "init");
            const auto loadScript = "(() => { window.__presetTestResult = null; window.__JUCE__.backend.emitEvent('presetCommand', {action:'load', id:'"
                + savedPresetId + "', requestId:9902}); return JSON.stringify({sent:true}); })()";
            evaluateJson(*browser, loadScript, 2000, javascriptError);
            pumpMessages(200);
            expectations.expect(processor.presetStatus()["id"].toString() == savedPresetId,
                                "real WebView preset command reloads the saved file");
            savedFile.deleteFile();
        }
        processor.applyPreset(processor.initialPreset(), "init");
        pumpMessages(100);
        const auto uiContract = evaluateJson(*browser, uiContractScript, 2000, javascriptError);
        expectations.expect(uiContract.has_value(), "captured the PROCESS and Pitch Map option contract");
        if (!uiContract.has_value())
        {
            std::cerr << "JavaScript error: " << javascriptError << '\n';
            return 1;
        }

        expectations.expect(numberProperty(*uiContract, "processOptionCount") == 24.0,
                            "PROCESS exposes all 24 spectral modes");
        expectations.expect(boolProperty(*uiContract, "processValuesSequential")
                                && numberProperty(*uiContract, "firstProcessValue", -1.0) == 0.0
                                && numberProperty(*uiContract, "lastProcessValue", -1.0) == 23.0,
                            "PROCESS preserves the stable sequential mode values 0 through 23");
        expectations.expect(numberProperty(*uiContract, "pitchRootOptionCount") == 12.0
                                && boolProperty(*uiContract, "pitchRootValuesSequential")
                                && stringProperty(*uiContract, "firstPitchRoot") == "C"
                                && stringProperty(*uiContract, "lastPitchRoot") == "B",
                            "Pitch Map exposes all 12 chromatic root notes");
        expectations.expect(numberProperty(*uiContract, "pitchScaleOptionCount") == 2.0
                                && stringProperty(*uiContract, "firstPitchScale") == juce::String::fromUTF8("大调")
                                && stringProperty(*uiContract, "lastPitchScale") == juce::String::fromUTF8("自然小调"),
                            "Pitch Map exposes major and natural-minor scale choices");

        javascriptError.clear();
        const auto probeResult = evaluateJson(*browser, installProbeScript, 2000, javascriptError);
        expectations.expect(probeResult.has_value() && boolProperty(*probeResult, "installed"),
                            "installed an observer on the real analyzerFrame native event");
        if (!probeResult.has_value() || !boolProperty(*probeResult, "installed"))
        {
            std::cerr << "JavaScript error: " << javascriptError << '\n';
            return 1;
        }

        javascriptError.clear();
        const auto before = evaluateJson(*browser, snapshotScript, 2500, javascriptError);
        expectations.expect(before.has_value(), "captured the baseline DOM and canvas state");
        if (!before.has_value())
        {
            std::cerr << "JavaScript error: " << javascriptError << '\n';
            return 1;
        }

        DeterministicAudioDriver audio(processor);
        runAudioAndMessages(audio, 3200);

        javascriptError.clear();
        const auto after = evaluateJson(*browser, snapshotScript, 3000, javascriptError);
        expectations.expect(after.has_value(), "captured the post-audio DOM and canvas state");
        if (!after.has_value())
        {
            std::cerr << "JavaScript error: " << javascriptError << '\n';
            return 1;
        }

        const auto eventCount = numberProperty(*after, "eventCount");
        const auto advanceBefore = numberProperty(*before, "advanceCount");
        const auto advanceAfter = numberProperty(*after, "advanceCount");
        const auto inputPeakBefore = numberProperty(*before, "inputPeakDb", -96.0);
        const auto outputPeakBefore = numberProperty(*before, "outputPeakDb", -96.0);
        const auto inputPeakAfter = numberProperty(*after, "inputPeakDb", -96.0);
        const auto outputPeakAfter = numberProperty(*after, "outputPeakDb", -96.0);

        std::cout << "DOM diagnostics: " << juce::JSON::toString(*after, true) << '\n';

        expectations.expect(numberProperty(*after, "waterfallEvents") >= 5
                                && numberProperty(*after, "waterfallBins") == 1024
                                && numberProperty(*after, "waterfallFftSize") == 8192
                                && numberProperty(*after, "waterfallPeak") > -80,
                            "Dedicated 8192-point output waterfall reaches native WebView");
        expectations.expect(eventCount >= 5.0, "C++ emitted at least five analyzerFrame events into WebView2");
        expectations.expect(numberProperty(*after, "inputCount") == 1024.0
                                && numberProperty(*after, "outputCount") == 1024.0,
                            "analyzerFrame delivered both 1024-point input and output arrays");
        expectations.expect(numberProperty(*after, "maxInputDb", -96.0) > -45.0,
                            "native analyzer input payload contains audible energy");
        expectations.expect(numberProperty(*after, "maxOutputDb", -96.0) > -45.0,
                            "native analyzer output payload contains audible energy");
        expectations.expect(advanceAfter - advanceBefore >= 20.0,
                            "waterfall advanced by at least twenty columns while native frames arrived");
        expectations.expect(numberProperty(*after, "waterfallWidth") >= 64.0
                                && numberProperty(*after, "waterfallHeight") >= 48.0,
                            "waterfall canvas has a non-trivial rendered size");
        expectations.expect(numberProperty(*after, "uniqueColours") >= 8.0,
                            "waterfall canvas contains diverse rendered colours");
        expectations.expect(numberProperty(*after, "brightPixels") >= 8.0,
                            "waterfall canvas contains visible high-energy pixels");
        expectations.expect(numberProperty(*after, "pixelHash") != numberProperty(*before, "pixelHash"),
                            "waterfall pixel content changed after deterministic audio");
        expectations.expect(inputPeakAfter > inputPeakBefore + 6.0 && inputPeakAfter > -50.0,
                            "input dB meter changed from silence to an audible value");
        expectations.expect(outputPeakAfter > outputPeakBefore + 6.0 && outputPeakAfter > -50.0,
                            "output dB meter changed from silence to an audible value");
        expectations.expect(numberProperty(*after, "inputLevelPercent") > 5.0,
                            "input meter fill level is visibly above zero");
        expectations.expect(numberProperty(*after, "outputLevelPercent") > 5.0,
                            "output meter fill level is visibly above zero");

        expectations.expect(setPlainParameter(processor, openfad::flipshift::ParameterIDs::pitchRoot, 7.0f),
                            "set Pitch Map root through the real C++ parameter");
        expectations.expect(setPlainParameter(processor, openfad::flipshift::ParameterIDs::pitchScale, 1.0f),
                            "set Pitch Map scale through the real C++ parameter");
        expectations.expect(setPlainParameter(processor, openfad::flipshift::ParameterIDs::mode, 23.0f),
                            "selected Pitch Map through the real C++ parameter");

        javascriptError.clear();
        const auto pitchMapState = waitForJson(*browser, pitchMapStateScript, 4000,
                                               [](const juce::var& state)
                                               {
                                                   return numberProperty(state, "eventMode", -1.0) == 23.0
                                                       && numberProperty(state, "eventPitchRoot", -1.0) == 7.0
                                                       && numberProperty(state, "eventPitchScale", -1.0) == 1.0
                                                       && numberProperty(state, "modeValue", -1.0) == 23.0
                                                       && numberProperty(state, "pitchRootValue", -1.0) == 7.0
                                                       && numberProperty(state, "pitchScaleValue", -1.0) == 1.0;
                                               },
                                               javascriptError);
        expectations.expect(pitchMapState.has_value(), "captured the Pitch Map state after native parameter automation");
        if (!pitchMapState.has_value())
        {
            std::cerr << "JavaScript error: " << javascriptError << '\n';
            return 1;
        }

        std::cout << "Pitch Map diagnostics: " << juce::JSON::toString(*pitchMapState, true) << '\n';
        expectations.expect(numberProperty(*pitchMapState, "parameterEventCount") >= 1.0
                                && numberProperty(*pitchMapState, "eventMode", -1.0) == 23.0
                                && numberProperty(*pitchMapState, "eventPitchRoot", -1.0) == 7.0
                                && numberProperty(*pitchMapState, "eventPitchScale", -1.0) == 1.0,
                            "C++ parameterState delivered Pitch Map, G root and minor scale to WebView2");
        expectations.expect(numberProperty(*pitchMapState, "modeValue", -1.0) == 23.0
                                && numberProperty(*pitchMapState, "pitchRootValue", -1.0) == 7.0
                                && numberProperty(*pitchMapState, "pitchScaleValue", -1.0) == 1.0
                                && stringProperty(*pitchMapState, "pitchRootText") == "G"
                                && stringProperty(*pitchMapState, "pitchScaleText") == juce::String::fromUTF8("自然小调"),
                            "Pitch Map selectors rendered the native G minor parameter state");
        expectations.expect(boolProperty(*pitchMapState, "axisHidden")
                                && stringProperty(*pitchMapState, "axisAriaHidden") == "true"
                                && !boolProperty(*pitchMapState, "pitchMapModuleHidden")
                                && stringProperty(*pitchMapState, "pitchMapModuleAriaHidden") == "false"
                                && !boolProperty(*pitchMapState, "pitchRootDisabled")
                                && !boolProperty(*pitchMapState, "pitchScaleDisabled"),
                            "Pitch Map module replaces AXIS and enables both pitch selectors");
        expectations.expect(!boolProperty(*pitchMapState, "pitchMapReadoutHidden")
                                && stringProperty(*pitchMapState, "pitchMapReadoutText").containsIgnoreCase(juce::String::fromUTF8("G 自然小调"))
                                && boolProperty(*pitchMapState, "glitchSelectionHidden")
                                && stringProperty(*pitchMapState, "pivotGuideDisplay") == "none",
                            "Pitch Map readout is visible while unrelated analyzer guides stay hidden");

        expectations.expect(setPlainParameter(processor, openfad::flipshift::ParameterIDs::pivotHz, 4000.0f),
                            "set Glitch band centre through the real C++ parameter");
        expectations.expect(setPlainParameter(processor, openfad::flipshift::ParameterIDs::widthQ, 2.0f),
                            "set Glitch band Q through the real C++ parameter");
        expectations.expect(setPlainParameter(processor, openfad::flipshift::ParameterIDs::mode, 22.0f),
                            "selected Glitch through the real C++ parameter");

        javascriptError.clear();
        const auto glitchState = waitForJson(*browser, glitchStateScript, 4000,
                                             [](const juce::var& state)
                                             {
                                                 return numberProperty(state, "eventMode", -1.0) == 22.0
                                                     && std::abs(numberProperty(state, "eventPivotHz", -1.0) - 4000.0) < 0.1
                                                     && std::abs(numberProperty(state, "eventWidthQ", -1.0) - 2.0) < 0.01
                                                     && numberProperty(state, "modeValue", -1.0) == 22.0
                                                     && !boolProperty(state, "glitchSelectionHidden");
                                             },
                                             javascriptError);
        expectations.expect(glitchState.has_value(), "captured the Glitch state after native parameter automation");
        if (!glitchState.has_value())
        {
            std::cerr << "JavaScript error: " << javascriptError << '\n';
            return 1;
        }

        std::cout << "Glitch diagnostics: " << juce::JSON::toString(*glitchState, true) << '\n';
        expectations.expect(numberProperty(*glitchState, "parameterEventCount") >= 2.0
                                && numberProperty(*glitchState, "eventMode", -1.0) == 22.0
                                && std::abs(numberProperty(*glitchState, "eventPivotHz", -1.0) - 4000.0) < 0.1
                                && std::abs(numberProperty(*glitchState, "eventWidthQ", -1.0) - 2.0) < 0.01,
                            "C++ parameterState delivered the Glitch mode and selected band controls to WebView2");
        expectations.expect(!boolProperty(*glitchState, "glitchSelectionHidden")
                                && stringProperty(*glitchState, "glitchSelectionDisplay") != "none"
                                && numberProperty(*glitchState, "glitchSelectionWidth") > 100.0
                                && numberProperty(*glitchState, "glitchSelectionHeight") >= 2.0
                                && numberProperty(*glitchState, "glitchSelectionTopPercent", -1.0) >= 0.0
                                && numberProperty(*glitchState, "glitchSelectionHeightPercent", -1.0) > 0.0,
                            "Glitch band overlay is visibly laid out over the waterfall");
        expectations.expect(std::abs(numberProperty(*glitchState, "glitchLowHz", -1.0) - 3000.0) <= 1.0
                                && std::abs(numberProperty(*glitchState, "glitchHighHz", -1.0) - 5000.0) <= 1.0
                                && stringProperty(*glitchState, "glitchLabel").containsIgnoreCase(juce::String::fromUTF8("Glitch 选区")),
                            "Glitch overlay represents the expected 3-5 kHz band for centre 4 kHz and Q 2");
        expectations.expect(!boolProperty(*glitchState, "axisHidden")
                                && boolProperty(*glitchState, "pitchMapModuleHidden")
                                && boolProperty(*glitchState, "pitchMapReadoutHidden")
                                && stringProperty(*glitchState, "pivotGuideDisplay") == "block"
                                && stringProperty(*glitchState, "pivotGuideLabel").containsIgnoreCase(juce::String::fromUTF8("频带中心")),
                            "Glitch restores AXIS and shows its explicit band-centre guide");

        window.setVisible(false);
        window.clearContentComponent();
        pumpMessages(100);
        editor.reset();
        processor.releaseResources();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Unhandled exception: " << exception.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unhandled non-standard exception\n";
        return 1;
    }

    if (expectations.getFailureCount() != 0)
    {
        std::cerr << expectations.getFailureCount() << " integration assertion(s) failed\n";
        return 1;
    }

    std::cout << "All native WebView analyzer integration checks passed\n";
    return 0;
}
