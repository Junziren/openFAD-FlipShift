#include "PluginProcessor.h"
#include "PluginEditor.h"

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
} // namespace

OpenFADFlipShiftAudioProcessor::OpenFADFlipShiftAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    setLatencySamples(FlipShiftEngine::getLatencySamplesForQuality(Quality::normal));
}

void OpenFADFlipShiftAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    activeQuality = static_cast<Quality>(static_cast<int>(*parameters.getRawParameterValue(ParameterIDs::quality)));
    engine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels(), activeQuality);
    setLatencySamples(engine.getLatencySamples());
}

void OpenFADFlipShiftAudioProcessor::releaseResources()
{
    engine.reset();
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
    values.mix = *parameters.getRawParameterValue(ParameterIDs::mix);
    values.outputGainDb = *parameters.getRawParameterValue(ParameterIDs::outputGainDb);
    values.quality = static_cast<Quality>(static_cast<int>(*parameters.getRawParameterValue(ParameterIDs::quality)));
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
    const auto qualityChanged = values.quality != activeQuality;
    if (values.quality != activeQuality)
        activeQuality = values.quality;

    engine.process(buffer, values);

    if (qualityChanged)
        setLatencySamples(engine.getLatencySamples());
}

juce::AudioProcessorEditor* OpenFADFlipShiftAudioProcessor::createEditor()
{
    return new OpenFADFlipShiftAudioProcessorEditor(*this);
}

void OpenFADFlipShiftAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = copyCurrentParameterState(parameters).createXml())
        copyXmlToBinary(*xml, destData);
}

void OpenFADFlipShiftAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

void OpenFADFlipShiftAudioProcessor::copyAnalyzerFrames(std::vector<float>& inputDb, std::vector<float>& outputDb) const
{
    engine.copyAnalyzerFrames(inputDb, outputDb);
}
} // namespace openfad::flipshift

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new openfad::flipshift::OpenFADFlipShiftAudioProcessor();
}
