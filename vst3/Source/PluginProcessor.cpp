#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetFormat.h"
#include <cmath>

namespace openfad::flipshift
{
namespace
{
juce::ValueTree copyCurrentParameterState(juce::AudioProcessorValueTreeState& parameters)
{
    auto snapshot = parameters.copyState();

    // Some hosts set a parameter directly without sending a listener callback.
    // Read each parameter's current atomic value so saved state cannot lag behind.
    for (auto child : snapshot)
    {
        const auto parameterID = child.getProperty("id").toString();
        if (auto* parameter = parameters.getParameter(parameterID))
            child.setProperty("value", parameter->convertFrom0to1(parameter->getValue()), nullptr);
    }

    return snapshot;
}

Quality qualityFromValue(float value) noexcept
{
    const auto finiteValue = std::isfinite(value) ? value : static_cast<float>(Quality::high);
    return static_cast<Quality>(juce::jlimit(0, 2, static_cast<int>(std::round(finiteValue))));
}
} // namespace

OpenFADFlipShiftAudioProcessor::OpenFADFlipShiftAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    setLatencySamples(FlipShiftEngine::getLatencySamplesForQuality(Quality::high));
    parameters.addParameterListener(ParameterIDs::quality, this);
    presetBaseline = initialPreset();
}

OpenFADFlipShiftAudioProcessor::~OpenFADFlipShiftAudioProcessor()
{
    cancelPendingUpdate();
    parameters.removeParameterListener(ParameterIDs::quality, this);
}

void OpenFADFlipShiftAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock audioCallbackGuard(getCallbackLock());
    const juce::ScopedLock configurationLock(engineConfigurationLock);
    auto values = readParameters();
    preparedSampleRate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
    preparedBlockSize = juce::jmax(1, samplesPerBlock);
    preparedChannels = juce::jmax(1, getTotalNumOutputChannels());
    engine.prepare(preparedSampleRate, preparedBlockSize, preparedChannels, values);
    waterfallAnalyzer.prepare(preparedSampleRate);
    activeQuality.store(static_cast<int>(values.quality), std::memory_order_release);
    enginePrepared.store(true, std::memory_order_release);
    setLatencySamples(engine.getLatencySamples());
}

void OpenFADFlipShiftAudioProcessor::releaseResources()
{
    const juce::ScopedLock audioCallbackGuard(getCallbackLock());
    const juce::ScopedLock configurationLock(engineConfigurationLock);
    enginePrepared.store(false, std::memory_order_release);
    engine.reset();
    waterfallAnalyzer.invalidate();
}

void OpenFADFlipShiftAudioProcessor::reset()
{
    const juce::ScopedLock audioCallbackGuard(getCallbackLock());
    const juce::ScopedLock configurationLock(engineConfigurationLock);
    engine.reset();
    waterfallAnalyzer.invalidate();
}

bool OpenFADFlipShiftAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    return mainIn == mainOut && (mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo());
}

EngineParameters OpenFADFlipShiftAudioProcessor::readParameters() const
{
    EngineParameters values;
    values.mode = static_cast<SpectralMode>(static_cast<int>(*parameters.getRawParameterValue(ParameterIDs::mode)));
    values.shiftHz = *parameters.getRawParameterValue(ParameterIDs::shiftHz);
    values.scale = *parameters.getRawParameterValue(ParameterIDs::scale);
    values.pivotHz = *parameters.getRawParameterValue(ParameterIDs::pivotHz);
    values.amount = *parameters.getRawParameterValue(ParameterIDs::amount);
    values.widthQ = *parameters.getRawParameterValue(ParameterIDs::widthQ);
    values.pitchRoot = static_cast<int>(*parameters.getRawParameterValue(ParameterIDs::pitchRoot));
    values.pitchScale = static_cast<PitchScale>(static_cast<int>(*parameters.getRawParameterValue(ParameterIDs::pitchScale)));
    values.mix = *parameters.getRawParameterValue(ParameterIDs::mix);
    values.outputGainDb = *parameters.getRawParameterValue(ParameterIDs::outputGainDb);
    values.quality = qualityFromValue(*parameters.getRawParameterValue(ParameterIDs::quality));
    values.bypass = *parameters.getRawParameterValue(ParameterIDs::bypass) > 0.5f;
    values.freeze = *parameters.getRawParameterValue(ParameterIDs::freeze) > 0.5f;
    return values;
}

void OpenFADFlipShiftAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto values = readParameters();
    values.quality = static_cast<Quality>(activeQuality.load(std::memory_order_acquire));

    engine.process(buffer, values);
    waterfallAnalyzer.push(buffer);
}

juce::AudioProcessorEditor* OpenFADFlipShiftAudioProcessor::createEditor()
{
    return new OpenFADFlipShiftAudioProcessorEditor(*this);
}

void OpenFADFlipShiftAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto snapshot = copyCurrentParameterState(parameters);
    {
        const juce::ScopedLock lock(presetMetadataLock);
        snapshot.setProperty("flipshiftPreset", juce::JSON::toString(presetBaseline, true), nullptr);
        snapshot.setProperty("flipshiftPresetId", currentPresetId, nullptr);
    }
    if (auto xml = snapshot.createXml())
        copyXmlToBinary(*xml, destData);
}

void OpenFADFlipShiftAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            auto baseline = juce::JSON::parse(restored.getProperty("flipshiftPreset").toString());
            const auto id = restored.getProperty("flipshiftPresetId").toString();
            {
                const juce::ScopedLock lock(presetMetadataLock);
                presetBaseline = presets::validate(baseline, parameters).wasOk() ? baseline : initialPreset();
                currentPresetId = id == "init" || presets::validId(id) ? id : "init";
            }
            parameters.replaceState(restored);
        }
}


juce::var OpenFADFlipShiftAudioProcessor::capturePreset(const juce::String& name)
{
    return presets::capture(parameters, name);
}

juce::var OpenFADFlipShiftAudioProcessor::initialPreset()
{
    return presets::capture(parameters, "Init", true);
}

void OpenFADFlipShiftAudioProcessor::rememberPreset(const juce::var& document, const juce::String& id)
{
    {
        const juce::ScopedLock lock(presetMetadataLock);
        presetBaseline = document.clone();
        currentPresetId = id;
    }
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

juce::Result OpenFADFlipShiftAudioProcessor::applyPreset(const juce::var& document, const juce::String& id)
{
    const auto result = presets::validate(document, parameters);
    if (result.failed()) return result;
    // All fields have been checked before any host parameter is touched.
    for (const auto* parameterId : presets::ids)
    {
        auto* parameter = parameters.getParameter(parameterId);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(document["parameters"][parameterId])));
        parameter->endChangeGesture();
    }
    parameters.getParameter(ParameterIDs::freeze)->setValueNotifyingHost(0.0f);
    rememberPreset(capturePreset(document["name"].toString()), id);
    return juce::Result::ok();
}

juce::var OpenFADFlipShiftAudioProcessor::presetStatus()
{
    const juce::ScopedLock lock(presetMetadataLock);
    auto* status = new juce::DynamicObject();
    status->setProperty("id", currentPresetId);
    status->setProperty("name", presetBaseline["name"]);
    status->setProperty("dirty", !presets::matches(presetBaseline, parameters));
    return juce::var(status);
}

void OpenFADFlipShiftAudioProcessor::memoryWarningReceived()
{
    engine.invalidateAnalyzerFrames();
    waterfallAnalyzer.invalidate();
}

void OpenFADFlipShiftAudioProcessor::setAnalyzerConsumerActive(bool active) noexcept
{
    engine.setAnalyzerEnabled(active);
}

bool OpenFADFlipShiftAudioProcessor::copyAnalyzerFrames(std::vector<float>& inputDb,
                                                        std::vector<float>& outputDb,
                                                        std::uint64_t& sequence) const
{
    return engine.copyAnalyzerFrames(inputDb, outputDb, sequence);
}

void OpenFADFlipShiftAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == ParameterIDs::quality)
        triggerAsyncUpdate();
}

void OpenFADFlipShiftAudioProcessor::handleAsyncUpdate()
{
    if (!enginePrepared.load(std::memory_order_acquire))
        return;

    const auto requestedQuality = qualityFromValue(*parameters.getRawParameterValue(ParameterIDs::quality));
    if (static_cast<int>(requestedQuality) == activeQuality.load(std::memory_order_acquire))
        return;

    applyPendingQualityChange(requestedQuality);
}

void OpenFADFlipShiftAudioProcessor::applyPendingQualityChange(Quality requestedQuality)
{
    if (!enginePrepared.load(std::memory_order_acquire)
        || static_cast<int>(requestedQuality) == activeQuality.load(std::memory_order_acquire))
        return;

    auto values = readParameters();
    values.quality = requestedQuality;

    const auto wasSuspended = isSuspended();
    if (!wasSuspended)
        suspendProcessing(true);

    auto qualityChanged = false;
    auto updatedLatencySamples = getLatencySamples();
    {
        const juce::ScopedLock audioCallbackGuard(getCallbackLock());
        const juce::ScopedLock configurationLock(engineConfigurationLock);
        if (enginePrepared.load(std::memory_order_acquire))
        {
            engine.prepare(preparedSampleRate, preparedBlockSize, preparedChannels, values);
            activeQuality.store(static_cast<int>(requestedQuality), std::memory_order_release);
            updatedLatencySamples = engine.getLatencySamples();
            qualityChanged = true;
        }
    }

    if (qualityChanged)
        setLatencySamples(updatedLatencySamples);
    if (!wasSuspended)
        suspendProcessing(false);
}
} // namespace openfad::flipshift

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new openfad::flipshift::OpenFADFlipShiftAudioProcessor();
}
