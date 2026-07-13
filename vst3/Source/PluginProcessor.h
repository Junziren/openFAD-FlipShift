#pragma once

#include "DSP/FlipShiftEngine.h"
#include "Parameters.h"
#include <JuceHeader.h>

namespace openfad::flipshift
{
class OpenFADFlipShiftAudioProcessor final : public juce::AudioProcessor
{
public:
    OpenFADFlipShiftAudioProcessor();
    ~OpenFADFlipShiftAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return parameters.getParameter(ParameterIDs::bypass);
    }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return parameters; }
    const juce::AudioProcessorValueTreeState& getState() const noexcept { return parameters; }

    void copyAnalyzerFrames(std::vector<float>& inputDb, std::vector<float>& outputDb) const;

private:
    EngineParameters readParameters() const;

    juce::AudioProcessorValueTreeState parameters;
    FlipShiftEngine engine;
    Quality activeQuality = Quality::normal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenFADFlipShiftAudioProcessor)
};
} // namespace openfad::flipshift
