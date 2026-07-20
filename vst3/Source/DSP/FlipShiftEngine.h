#pragma once

#include "DSP/SpectralModes.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>

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
    int pitchRoot = 0;
    PitchScale pitchScale = PitchScale::major;
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

    void prepare(double newSampleRate, int maxBlockSize, int channels, const EngineParameters& initialParameters);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const EngineParameters& parameters);

    int getLatencySamples() const noexcept { return fftSize; }
    int getFftSize() const noexcept { return fftSize; }
    int getHopSize() const noexcept { return hopSize; }

    void setAnalyzerEnabled(bool shouldBeEnabled) noexcept;
    void invalidateAnalyzerFrames() noexcept;
    bool copyAnalyzerFrames(std::vector<float>& inputDb,
                            std::vector<float>& outputDb,
                            std::uint64_t& sequence) const;

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

    static constexpr int maximumFftSize = 2048;
    static constexpr int maximumAnalyzerBins = maximumFftSize / 2 + 1;
    static constexpr int analyzerBufferCount = 3;
    static_assert(std::atomic<int>::is_always_lock_free);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    struct AnalyzerFrame
    {
        std::array<float, maximumAnalyzerBins> inputDb {};
        std::array<float, maximumAnalyzerBins> outputDb {};
        int binCount = 0;
        std::uint32_t sequence = 0;
        std::uint32_t generation = 0;
    };

    void configureQuality(Quality quality, const EngineParameters& initialParameters);
    void processFrame(ChannelState& state, const EngineParameters& parameters, int channelIndex);
    float processSample(ChannelState& state, float input, const EngineParameters& parameters, int channelIndex);
    void publishAnalyzer(const ChannelState& state);
    EngineParameters getSmoothedParameters(const EngineParameters& target);

    double sampleRate = 48000.0;
    int numChannels = 2;
    int fftOrder = 10;
    int fftSize = 1024;
    int hopSize = 256;
    int ringMask = 1023;
    Quality currentQuality = Quality::normal;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> windowBuffer;
    std::vector<ChannelState> channelStates;

    std::array<AnalyzerFrame, analyzerBufferCount> analyzerFrames {};
    int analyzerWriteIndex = 0;
    mutable std::atomic<int> analyzerReadyIndex { 1 };
    mutable int analyzerReadIndex = 2;
    std::atomic<std::uint32_t> analyzerPublishedSequence { 0 };
    std::atomic<std::uint32_t> analyzerGeneration { 1 };
    std::atomic<bool> analyzerEnabled { false };
    std::atomic<bool> analyzerHasData { false };
    std::uint32_t analyzerSequence = 0;
    int analyzerFramesUntilPublish = 0;
    int analyzerPublishInterval = 1;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> shiftSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> scaleSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pivotSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> linearGainSmoother;
};
} // namespace openfad::flipshift
