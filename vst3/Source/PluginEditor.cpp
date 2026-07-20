#include "PluginEditor.h"
#include "BinaryData.h"
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
        result.add(juce::jmap(fraction, values[static_cast<size_t>(low)], values[static_cast<size_t>(high)]));
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
    browser.goToURL(juce::WebBrowserComponent::getResourceProviderRoot() + "index.html?v=7");
    startTimerHz(30);
}

OpenFADFlipShiftAudioProcessorEditor::~OpenFADFlipShiftAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.setAnalyzerConsumerActive(false);
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
        })
        .withEventListener("parameterGesture", [this](const juce::var& payload)
        {
            handleParameterEvent(payload);
        })
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

    static const juce::StringArray allowedResources { "index.html", "styles.css", "app.js" };
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

    constexpr auto displayPoints = 192;
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("input", makeLogResampledArray(analyzerInputScratch, displayPoints));
    payload->setProperty("output", makeLogResampledArray(analyzerOutputScratch, displayPoints));
    browser.emitEventIfBrowserIsVisible("analyzerFrame", juce::var(payload.get()));
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
    }
}
} // namespace openfad::flipshift
