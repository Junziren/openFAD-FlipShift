#include "PluginEditor.h"
#include "BinaryData.h"
#include "PresetFormat.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace openfad::flipshift
{
namespace
{
constexpr std::array<const char*, 14> parameterIDs {
    ParameterIDs::mode,
    ParameterIDs::shiftHz,
    ParameterIDs::scale,
    ParameterIDs::pivotHz,
    ParameterIDs::amount,
    ParameterIDs::widthQ,
    ParameterIDs::mix,
    ParameterIDs::outputGainDb,
    ParameterIDs::quality,
    ParameterIDs::analyzerView,
    ParameterIDs::bypass,
    ParameterIDs::freeze,
    ParameterIDs::pitchRoot,
    ParameterIDs::pitchScale
};

juce::Array<juce::var> makeLogResampledArray(const std::vector<float>& values, int pointCount)
{
    juce::Array<juce::var> result;
    if (values.empty() || pointCount <= 0)
        return result;

    result.ensureStorageAllocated(pointCount);
    const auto lastIndex = static_cast<float>(values.size() - 1);

    for (int i = 0; i < pointCount; ++i)
    {
        const auto normalised = pointCount == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(pointCount - 1);
        const auto logarithmic = (std::pow(100.0f, normalised) - 1.0f) / 99.0f;
        const auto sourcePosition = logarithmic * lastIndex;
        const auto low = juce::jlimit(0, static_cast<int>(values.size() - 1), static_cast<int>(sourcePosition));
        const auto high = juce::jmin(low + 1, static_cast<int>(values.size() - 1));
        const auto fraction = sourcePosition - static_cast<float>(low);
        auto magnitude = juce::jmap(fraction, values[static_cast<size_t>(low)], values[static_cast<size_t>(high)]);
        // Preserve narrow peaks when several source bins fall into one display cell.
        const auto nextUnit = juce::jmin(1.0f, (static_cast<float>(i) + 0.5f) / static_cast<float>(pointCount - 1));
        const auto prevUnit = juce::jmax(0.0f, (static_cast<float>(i) - 0.5f) / static_cast<float>(pointCount - 1));
        const auto from = static_cast<int>(std::ceil((std::pow(100.0f, prevUnit) - 1.0f) / 99.0f * lastIndex));
        const auto to = static_cast<int>(std::floor((std::pow(100.0f, nextUnit) - 1.0f) / 99.0f * lastIndex));
        for (auto bin = from; bin <= to; ++bin)
            magnitude = juce::jmax(magnitude, values[static_cast<size_t>(juce::jlimit(0, static_cast<int>(lastIndex), bin))]);
        result.add(magnitude);
    }

    return result;
}

bool isApprovedExternalUrl(const juce::String& url)
{
    static const juce::StringArray approvedUrls {
        "https://fadrecords.com/openfad/",
        "https://space.bilibili.com/227573145",
        "https://space.bilibili.com/227573145/",
        "https://github.com/Junziren/openFAD-FlipShift",
        "https://github.com/willren5/openFAD/blob/main/LICENSE"
    };
    return approvedUrls.contains(url);
}

void openApprovedExternalUrl(const juce::String& url)
{
    if (!isApprovedExternalUrl(url))
        return;

#if JUCE_IOS && defined(JucePlugin_Build_AUv3) && JucePlugin_Build_AUv3
    // AUv3 app extensions cannot use UIApplication to launch a browser.
    return;
#elif JUCE_MAC
    if (juce::SystemStats::isRunningInAppExtensionSandbox())
        return;
    juce::URL(url).launchInDefaultBrowser();
#else
    juce::URL(url).launchInDefaultBrowser();
#endif
}
} // namespace

OpenFADFlipShiftAudioProcessorEditor::OpenFADFlipShiftAudioProcessorEditor(OpenFADFlipShiftAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      audioProcessor(processor),
      browser(makeBrowserOptions())
{
    lastParameterValues.fill(std::numeric_limits<float>::quiet_NaN());
    analyzerInputScratch.reserve(1025);
    analyzerOutputScratch.reserve(1025);
    addAndMakeVisible(browser);

    setResizable(true, true);
    setResizeLimits(320, 280, 1600, 1200);
    setSize(1000, 650);

    audioProcessor.setAnalyzerConsumerActive(true);
    browser.goToURL(juce::WebBrowserComponent::getResourceProviderRoot() + "index.html?v=8");
    startTimerHz(30);
}

OpenFADFlipShiftAudioProcessorEditor::~OpenFADFlipShiftAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.setAnalyzerConsumerActive(false);
    audioProcessor.setWaterfallConsumerActive(false);
    endActiveParameterGestures();
}

juce::WebBrowserComponent::Options OpenFADFlipShiftAudioProcessorEditor::makeBrowserOptions()
{
    auto options = juce::WebBrowserComponent::Options {};

#if JUCE_WINDOWS
    options = options
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2 {}
            .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("openFAD-FlipShift-WebView2"))
            .withStatusBarDisabled()
            .withBuiltInErrorPageDisabled()
            .withBackgroundColour(juce::Colour(0xffd5d2cd)));
#endif

    return options
        .withNativeIntegrationEnabled()
        .withResourceProvider([](const juce::String& url) { return getResource(url); })
        .withEventListener("uiReady", [this](const juce::var&)
        {
            endActiveParameterGestures();
            frontendReady = true;
            lastParameterValues.fill(std::numeric_limits<float>::quiet_NaN());
            lastAnalyzerSequence = 0;
            syncNativeSurfaceState(true);
            sendPresetState(true);
        })
        .withEventListener("parameterGesture", [this](const juce::var& payload)
        {
            handleParameterEvent(payload);
        })
        .withEventListener("presetCommand", [this](const juce::var& payload) { handlePresetCommand(payload); })
        .withEventListener("openExternal", [](const juce::var& payload)
        {
            if (!payload.isObject())
                return;

            const auto url = payload.getProperty("url", {}).toString();
            openApprovedExternalUrl(url);
        });
}

juce::String OpenFADFlipShiftAudioProcessorEditor::getMimeType(const juce::String& path)
{
    if (path.endsWithIgnoreCase(".html")) return "text/html";
    if (path.endsWithIgnoreCase(".js")) return "text/javascript";
    if (path.endsWithIgnoreCase(".css")) return "text/css";
    if (path.endsWithIgnoreCase(".svg")) return "image/svg+xml";
    if (path.endsWithIgnoreCase(".png")) return "image/png";
    if (path.endsWithIgnoreCase(".json")) return "application/json";
    return "application/octet-stream";
}

OpenFADFlipShiftAudioProcessorEditor::ResourceResult
OpenFADFlipShiftAudioProcessorEditor::getResource(const juce::String& url)
{
    auto requestedPath = url.upToFirstOccurrenceOf("?", false, false);
    requestedPath = requestedPath.trimCharactersAtStart("/");
    if (requestedPath.isEmpty())
        requestedPath = "index.html";

    static const juce::StringArray allowedResources { "index.html", "styles.css", "app.js", "i18n.js" };
    if (!allowedResources.contains(requestedPath))
        return std::nullopt;

    const auto requestedFile = requestedPath;

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const auto* resourceName = BinaryData::namedResourceList[i];
        const auto originalPath = juce::String(BinaryData::getNamedResourceOriginalFilename(resourceName));
        if (juce::File(originalPath).getFileName() != requestedFile)
            continue;

        int dataSize = 0;
        if (const auto* data = BinaryData::getNamedResource(resourceName, dataSize); data != nullptr && dataSize > 0)
        {
            std::vector<std::byte> bytes(static_cast<size_t>(dataSize));
            std::memcpy(bytes.data(), data, static_cast<size_t>(dataSize));
            return Resource { std::move(bytes), getMimeType(requestedFile) };
        }
    }

    return std::nullopt;
}

void OpenFADFlipShiftAudioProcessorEditor::handleParameterEvent(const juce::var& payload)
{
    if (!payload.isObject())
        return;

    const auto parameterID = payload.getProperty("id", {}).toString();
    const auto phase = payload.getProperty("phase", {}).toString();
    if (phase != "begin" && phase != "value" && phase != "end")
        return;

    auto* parameter = audioProcessor.getState().getParameter(parameterID);
    if (parameter == nullptr)
        return;

    if (phase == "begin")
    {
        if (activeParameterGestures.contains(parameter))
            return;

        parameter->beginChangeGesture();
        activeParameterGestures.add(parameter);
        return;
    }

    if (phase == "end")
    {
        const auto activeGestureIndex = activeParameterGestures.indexOf(parameter);
        if (activeGestureIndex < 0)
            return;

        parameter->endChangeGesture();
        activeParameterGestures.remove(activeGestureIndex);
        return;
    }

    if (phase == "value" && payload.hasProperty("value"))
    {
        const auto value = static_cast<float>(payload.getProperty("value", 0.0));
        if (!std::isfinite(value))
            return;

        const auto normalised = parameter->convertTo0to1(value);
        if (std::isfinite(normalised))
            parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalised));
    }
}

void OpenFADFlipShiftAudioProcessorEditor::endActiveParameterGestures()
{
    for (auto* parameter : activeParameterGestures)
    {
        if (parameter != nullptr)
            parameter->endChangeGesture();
    }

    activeParameterGestures.clearQuick();
}

void OpenFADFlipShiftAudioProcessorEditor::timerCallback()
{
    syncNativeSurfaceState();
    if (!frontendReady)
        return;

    sendParameterState(false);
    sendPresetState();
    sendWaterfallFrame();
    if (++analyzerTick % 2 == 0)
        sendAnalyzerFrame();
}

void OpenFADFlipShiftAudioProcessorEditor::sendParameterState(bool force)
{
    std::array<float, parameterIDs.size()> currentValues {};
    bool changed = force;

    for (size_t i = 0; i < parameterIDs.size(); ++i)
    {
        const auto* rawValue = audioProcessor.getState().getRawParameterValue(parameterIDs[i]);
        if (rawValue == nullptr)
            continue;

        const auto value = rawValue->load();
        currentValues[i] = value;
        if (!std::isfinite(lastParameterValues[i]) || std::abs(lastParameterValues[i] - value) > 1.0e-6f)
            changed = true;
    }

    if (!changed)
        return;

    juce::DynamicObject::Ptr values = new juce::DynamicObject();
    for (size_t i = 0; i < parameterIDs.size(); ++i)
    {
        lastParameterValues[i] = currentValues[i];
        values->setProperty(parameterIDs[i], currentValues[i]);
    }

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("values", juce::var(values.get()));
    payload->setProperty("sampleRate", audioProcessor.getSampleRate() > 0.0 ? audioProcessor.getSampleRate() : 48000.0);
    browser.emitEventIfBrowserIsVisible("parameterState", juce::var(payload.get()));
}

void OpenFADFlipShiftAudioProcessorEditor::sendAnalyzerFrame()
{
    if (!audioProcessor.copyAnalyzerFrames(
            analyzerInputScratch, analyzerOutputScratch, lastAnalyzerSequence))
        return;

    constexpr auto displayPoints = 1024;
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("input", makeLogResampledArray(analyzerInputScratch, displayPoints));
    payload->setProperty("output", makeLogResampledArray(analyzerOutputScratch, displayPoints));
    browser.emitEventIfBrowserIsVisible("analyzerFrame", juce::var(payload.get()));
}


void OpenFADFlipShiftAudioProcessorEditor::sendWaterfallFrame()
{
    if (!isVisible() || !audioProcessor.copyWaterfallFrame(waterfallScratch)) return;
    auto* payload = new juce::DynamicObject();
    payload->setProperty("output", makeLogResampledArray(waterfallScratch, 1024));
    payload->setProperty("fftSize", WaterfallAnalyzer::fftSize);
    browser.emitEventIfBrowserIsVisible("waterfallFrame", juce::var(payload));
}

void OpenFADFlipShiftAudioProcessorEditor::sendPresetState(bool refreshList)
{
    if (!frontendReady || !isShowing()) return;
    auto status = audioProcessor.presetStatus();
    const auto encoded = juce::JSON::toString(status, true);
    if (!refreshList && encoded == lastPresetStatus) return;
    lastPresetStatus = encoded;
    if (refreshList)
    {
        juce::Array<juce::var> entries;
        auto* initial = new juce::DynamicObject();
        initial->setProperty("id", "init");
        initial->setProperty("name", "Init");
        entries.add(juce::var(initial));
        auto files = presets::directory().findChildFiles(juce::File::findFiles, false, "*.flipshift");
        for (const auto& file : files)
        {
            const auto id = file.getFileNameWithoutExtension();
            juce::var document;
            if (!presets::validId(id) || presets::read(file, document, audioProcessor.getState()).failed()) continue;
            auto* entry = new juce::DynamicObject();
            entry->setProperty("id", id);
            entry->setProperty("name", document["name"]);
            entries.add(juce::var(entry));
        }
        status.getDynamicObject()->setProperty("entries", entries);
    }
    browser.emitEventIfBrowserIsVisible("presetState", status);
}

void OpenFADFlipShiftAudioProcessorEditor::finishPresetCommand(int requestId, const juce::Result& result)
{
    sendParameterState(true);
    sendPresetState(true);
    auto* response = new juce::DynamicObject();
    response->setProperty("requestId", requestId);
    response->setProperty("ok", result.wasOk());
    response->setProperty("error", result.getErrorMessage());
    pendingPresetResults.add(juce::var(response));
    flushPresetResults();
}

void OpenFADFlipShiftAudioProcessorEditor::flushPresetResults()
{
    if (!frontendReady || !isShowing()) return;
    for (const auto& response : pendingPresetResults)
        browser.emitEventIfBrowserIsVisible("presetResult", response);
    pendingPresetResults.clear();
}

void OpenFADFlipShiftAudioProcessorEditor::handlePresetCommand(const juce::var& payload)
{
    if (!payload.isObject()) return;
    const auto requestId = static_cast<int>(payload["requestId"]);
    const auto action = payload["action"].toString();
    if (presetChooser) { finishPresetCommand(requestId, juce::Result::fail("preset.busy")); return; }
    if (action == "list") { finishPresetCommand(requestId, juce::Result::ok()); return; }
    endActiveParameterGestures();
    if (action == "load")
    {
        const auto id = payload["id"].toString();
        juce::var document;
        auto result = juce::Result::ok();
        if (id == "init") document = audioProcessor.initialPreset();
        else if (presets::validId(id)) result = presets::read(presets::directory().getChildFile(id + ".flipshift"), document, audioProcessor.getState());
        else result = juce::Result::fail("preset.read");
        if (result.wasOk()) result = audioProcessor.applyPreset(document, id);
        finishPresetCommand(requestId, result);
        return;
    }
    if (action == "save" || action == "saveAs")
    {
        const auto name = payload["name"].toString().trim();
        auto document = audioProcessor.capturePreset(name);
        auto result = presets::validate(document, audioProcessor.getState());
        auto id = audioProcessor.presetStatus()["id"].toString();
        if (action == "saveAs" || !presets::validId(id)) id = juce::Uuid().toString().removeCharacters("-");
        if (result.wasOk()) result = presets::write(presets::directory().getChildFile(id + ".flipshift"), document);
        if (result.wasOk()) audioProcessor.rememberPreset(document, id);
        finishPresetCommand(requestId, result);
        return;
    }
    if (action != "import" && action != "export") { finishPresetCommand(requestId, juce::Result::fail("preset.format")); return; }
    const auto exporting = action == "export";
    const auto snapshot = audioProcessor.capturePreset(audioProcessor.presetStatus()["name"].toString());
    const auto chinese = payload["language"].toString() != "en";
    const auto title = exporting ? (chinese ? juce::String::fromUTF8("导出 FlipShift 预设") : "Export FlipShift preset")
                                 : (chinese ? juce::String::fromUTF8("导入 FlipShift 预设") : "Import FlipShift preset");
    presetChooser = std::make_unique<juce::FileChooser>(title,
        exporting ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Preset.flipshift") : presets::directory(),
        exporting ? "*.flipshift" : "*.flipshift;*.json");
    const auto flags = exporting ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting
                                 : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    const juce::Component::SafePointer<OpenFADFlipShiftAudioProcessorEditor> safeThis(this);
    presetChooser->launchAsync(flags, [safeThis, requestId, exporting, snapshot](const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr) return;
        auto file = chooser.getResult();
        auto result = juce::Result::fail("preset.cancelled");
        if (file != juce::File())
        {
            if (exporting)
            {
                // Keep the exact native-dialog path, so its overwrite confirmation stays authoritative.
                if (file.getFileExtension().isEmpty()) file = file.withFileExtension(".flipshift");
                result = presets::write(file, snapshot);
            }
            else
            {
                juce::var document;
                result = presets::read(file, document, safeThis->audioProcessor.getState());
                const auto id = juce::Uuid().toString().removeCharacters("-");
                if (result.wasOk()) result = presets::write(presets::directory().getChildFile(id + ".flipshift"), document);
                if (result.wasOk()) result = safeThis->audioProcessor.applyPreset(document, id);
            }
        }
        safeThis->finishPresetCommand(requestId, result);
        juce::MessageManager::callAsync([safeThis] { if (safeThis != nullptr) safeThis->presetChooser.reset(); });
    });
}

void OpenFADFlipShiftAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xffd5d2cd));
}

void OpenFADFlipShiftAudioProcessorEditor::resized()
{
    browser.setBounds(getLocalBounds());
}

void OpenFADFlipShiftAudioProcessorEditor::visibilityChanged()
{
    syncNativeSurfaceState();
}

void OpenFADFlipShiftAudioProcessorEditor::parentHierarchyChanged()
{
    syncNativeSurfaceState();
}

void OpenFADFlipShiftAudioProcessorEditor::syncNativeSurfaceState(bool forceFrontendSync)
{
    const auto visible = isVisible();
    audioProcessor.setWaterfallConsumerActive(visible);
    const auto visibilityChanged = lastSurfaceVisible != visible;
    lastSurfaceVisible = visible;
    if (!visibilityChanged && !forceFrontendSync)
        return;

    if (!visible)
    {
        endActiveParameterGestures();
        lastAnalyzerSequence = 0;
    }

    if (!frontendReady)
        return;

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("visible", visible);
    browser.emitEventIfBrowserIsVisible("surfaceVisibility", juce::var(payload.get()));

    if (visible)
    {
        sendParameterState(true);
        sendAnalyzerFrame();
        sendPresetState(true);
        flushPresetResults();
    }
}
} // namespace openfad::flipshift
