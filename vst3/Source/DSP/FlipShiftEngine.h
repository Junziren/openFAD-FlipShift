#pragma once

#include "DSP/SpectralModes.h"
#include <JuceHeader.h>

namespace openfad::flipshift
{
enum class Quality : int
{
    low = 0,
    normal,
    high
};

struct EngineParameters
{
    SpectralMode mode = SpectralMode::shift;
    float shiftHz = 0.0f;
    float scale = 1.0f;
    float pivotHz = 1000.0f;
    float amount = 0.5f;
    float widthQ = 1.0f;
    float mix = 0.5f;
    float outputGainDb = 0.0f;
    Quality quality = Quality::normal;
    bool bypass = false;
    bool freeze = false;
};

class FlipShiftEngine
{
public:
    static int getLatencySamplesForQuality(Quality quality) noexcept;

    void prepare(double newSampleRate, int maxBlockSize, int channels, Quality quality);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const EngineParameters& parameters);

    int getLatencySamples() const noexcept { return fftSize; }
    int getFftSize() const noexcept { return fftSize; }
    int getHopSize() const noexcept { return hopSize; }

    void copyAnalyzerFrames(std::vector<float>& inputDb, std::vector<float>& outputDb) const;

private:
    struct ChannelState
    {
        std::vector<float> inputRing;
        std::vector<float> outputRing;
        std::vector<float> dryDelay;
        std::vector<float> fftData;
        std::vector<std::complex<float>> inputSpectrum;
        std::vector<std::complex<float>> outputSpectrum;
        SpectralFrameMemory memory;
        int writePosition = 0;
        int samplesUntilFrame = 0;
    };

    void configureQuality(Quality quality);
    void processFrame(ChannelState& state, const EngineParameters& parameters, int channelIndex);
    float processSample(ChannelState& state, float input, const EngineParameters& parameters, int channelIndex);
    void publishAnalyzer(const ChannelState& state);
    EngineParameters getSmoothedParameters(const EngineParameters& target);

    double sampleRate = 48000.0;
    int numChannels = 2;
    int fftOrder = 10;
    int fftSize = 1024;
    int hopSize = 256;
    Quality currentQuality = Quality::normal;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> windowBuffer;
    std::vector<ChannelState> channelStates;

    mutable juce::SpinLock analyzerLock;
    std::vector<float> analyzerInputDb;
    std::vector<float> analyzerOutputDb;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> shiftSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> scaleSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pivotSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoother;
};
} // namespace openfad::flipshift
