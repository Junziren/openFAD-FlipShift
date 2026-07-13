#include "DSP/FlipShiftEngine.h"

namespace openfad::flipshift
{
namespace
{
float safeDb(float magnitude)
{
    return juce::jlimit(-96.0f, 12.0f, juce::Decibels::gainToDecibels(magnitude + 1.0e-8f));
}

int orderForQuality(Quality quality)
{
    switch (quality)
    {
        case Quality::low: return 9;
        case Quality::normal: return 10;
        case Quality::high: return 11;
    }

    return 10;
}
} // namespace

int FlipShiftEngine::getLatencySamplesForQuality(Quality quality) noexcept
{
    return 1 << orderForQuality(quality);
}

void FlipShiftEngine::prepare(double newSampleRate, int, int channels, Quality quality)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    numChannels = juce::jmax(1, channels);
    configureQuality(quality);
}

void FlipShiftEngine::configureQuality(Quality quality)
{
    currentQuality = quality;
    fftOrder = orderForQuality(quality);
    fftSize = getLatencySamplesForQuality(quality);
    hopSize = fftSize / 4;
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    windowBuffer.assign(static_cast<size_t>(fftSize), 0.0f);
    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        windowBuffer.data(), static_cast<size_t>(fftSize),
        juce::dsp::WindowingFunction<float>::hann, false);

    channelStates.assign(static_cast<size_t>(numChannels), {});
    for (auto& state : channelStates)
    {
        state.inputRing.assign(static_cast<size_t>(fftSize), 0.0f);
        state.outputRing.assign(static_cast<size_t>(fftSize), 0.0f);
        state.dryDelay.assign(static_cast<size_t>(fftSize), 0.0f);
        state.fftData.assign(static_cast<size_t>(fftSize * 2), 0.0f);
        state.inputSpectrum.assign(static_cast<size_t>(fftSize / 2 + 1), {});
        state.outputSpectrum.assign(static_cast<size_t>(fftSize / 2 + 1), {});
        state.memory = {};
        state.writePosition = 0;
        state.samplesUntilFrame = hopSize;
    }

    {
        const juce::SpinLock::ScopedLockType lock(analyzerLock);
        analyzerInputDb.assign(static_cast<size_t>(fftSize / 2 + 1), -96.0f);
        analyzerOutputDb.assign(static_cast<size_t>(fftSize / 2 + 1), -96.0f);
    }

    const auto rampSeconds = 0.02;
    shiftSmoother.reset(sampleRate, rampSeconds);
    scaleSmoother.reset(sampleRate, rampSeconds);
    pivotSmoother.reset(sampleRate, rampSeconds);
    amountSmoother.reset(sampleRate, rampSeconds);
    widthSmoother.reset(sampleRate, rampSeconds);
    mixSmoother.reset(sampleRate, rampSeconds);
    gainSmoother.reset(sampleRate, rampSeconds);
    shiftSmoother.setCurrentAndTargetValue(0.0f);
    scaleSmoother.setCurrentAndTargetValue(1.0f);
    pivotSmoother.setCurrentAndTargetValue(1000.0f);
    amountSmoother.setCurrentAndTargetValue(0.5f);
    widthSmoother.setCurrentAndTargetValue(1.0f);
    mixSmoother.setCurrentAndTargetValue(0.5f);
    gainSmoother.setCurrentAndTargetValue(0.0f);
}

void FlipShiftEngine::reset()
{
    for (auto& state : channelStates)
    {
        std::fill(state.inputRing.begin(), state.inputRing.end(), 0.0f);
        std::fill(state.outputRing.begin(), state.outputRing.end(), 0.0f);
        std::fill(state.dryDelay.begin(), state.dryDelay.end(), 0.0f);
        std::fill(state.fftData.begin(), state.fftData.end(), 0.0f);
        std::fill(state.inputSpectrum.begin(), state.inputSpectrum.end(), std::complex<float> {});
        std::fill(state.outputSpectrum.begin(), state.outputSpectrum.end(), std::complex<float> {});
        state.memory = {};
        state.writePosition = 0;
        state.samplesUntilFrame = hopSize;
    }
}

void FlipShiftEngine::process(juce::AudioBuffer<float>& buffer, const EngineParameters& parameters)
{
    if (parameters.quality != currentQuality)
        configureQuality(parameters.quality);

    shiftSmoother.setTargetValue(parameters.shiftHz);
    scaleSmoother.setTargetValue(parameters.scale);
    pivotSmoother.setTargetValue(parameters.pivotHz);
    amountSmoother.setTargetValue(parameters.amount);
    widthSmoother.setTargetValue(parameters.widthQ);
    mixSmoother.setTargetValue(parameters.mix);
    gainSmoother.setTargetValue(parameters.outputGainDb);

    const auto channelsToProcess = juce::jmin(buffer.getNumChannels(), static_cast<int>(channelStates.size()));
    const auto samples = buffer.getNumSamples();

    for (int sample = 0; sample < samples; ++sample)
    {
        auto smoothed = getSmoothedParameters(parameters);
        const auto outputGain = juce::Decibels::decibelsToGain(smoothed.outputGainDb);

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            auto& state = channelStates[static_cast<size_t>(channel)];
            data[sample] = processSample(state, data[sample], smoothed, channel) * outputGain;
        }
    }

    for (int channel = channelsToProcess; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, samples);
}

EngineParameters FlipShiftEngine::getSmoothedParameters(const EngineParameters& target)
{
    auto values = target;
    values.shiftHz = shiftSmoother.getNextValue();
    values.scale = scaleSmoother.getNextValue();
    values.pivotHz = pivotSmoother.getNextValue();
    values.amount = amountSmoother.getNextValue();
    values.widthQ = widthSmoother.getNextValue();
    values.mix = mixSmoother.getNextValue();
    values.outputGainDb = gainSmoother.getNextValue();
    return values;
}

float FlipShiftEngine::processSample(ChannelState& state, float input, const EngineParameters& parameters, int channelIndex)
{
    auto& writePosition = state.writePosition;
    const auto wet = state.outputRing[static_cast<size_t>(writePosition)];
    const auto dry = state.dryDelay[static_cast<size_t>(writePosition)];

    state.outputRing[static_cast<size_t>(writePosition)] = 0.0f;
    state.dryDelay[static_cast<size_t>(writePosition)] = input;
    state.inputRing[static_cast<size_t>(writePosition)] = input;

    if (--state.samplesUntilFrame <= 0)
    {
        processFrame(state, parameters, channelIndex);
        state.samplesUntilFrame = hopSize;
    }

    writePosition = (writePosition + 1) % fftSize;

    if (parameters.bypass)
        return dry;

    const auto mix = juce::jlimit(0.0f, 1.0f, parameters.mix);
    return dry * (1.0f - mix) + wet * mix;
}

void FlipShiftEngine::processFrame(ChannelState& state, const EngineParameters& parameters, int channelIndex)
{
    const auto frameStart = (state.writePosition + 1) % fftSize;

    std::fill(state.fftData.begin(), state.fftData.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
    {
        const auto ringIndex = (frameStart + i) % fftSize;
        state.fftData[static_cast<size_t>(i)] = state.inputRing[static_cast<size_t>(ringIndex)] * windowBuffer[static_cast<size_t>(i)];
    }

    fft->performRealOnlyForwardTransform(state.fftData.data(), true);

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        state.inputSpectrum[static_cast<size_t>(bin)] =
            { state.fftData[static_cast<size_t>(bin * 2)], state.fftData[static_cast<size_t>(bin * 2 + 1)] };
    }

    SpectralTransformParameters transformParams;
    transformParams.mode = parameters.mode;
    transformParams.shiftHz = parameters.shiftHz;
    transformParams.scale = parameters.scale;
    transformParams.pivotHz = parameters.pivotHz;
    transformParams.amount = parameters.amount;
    transformParams.widthQ = parameters.widthQ;
    transformParams.sampleRate = static_cast<float>(sampleRate);
    transformParams.fftSize = fftSize;
    transformParams.hopSize = hopSize;
    transformParams.channelIndex = channelIndex;
    transformParams.numChannels = numChannels;
    transformParams.freeze = parameters.freeze;

    applySpectralMode(state.inputSpectrum, state.outputSpectrum, state.memory, transformParams);

    std::fill(state.fftData.begin(), state.fftData.end(), 0.0f);
    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        state.fftData[static_cast<size_t>(bin * 2)] = state.outputSpectrum[static_cast<size_t>(bin)].real();
        state.fftData[static_cast<size_t>(bin * 2 + 1)] = state.outputSpectrum[static_cast<size_t>(bin)].imag();
    }

    fft->performRealOnlyInverseTransform(state.fftData.data());

    // JUCE normalises the inverse FFT. Four-times-overlapped Hann windows sum
    // to 1.5 after analysis/synthesis windowing, so 2/3 restores unity gain.
    constexpr auto overlapScale = 2.0f / 3.0f;
    const auto outputStart = (state.writePosition + 1) % fftSize;
    for (int i = 0; i < fftSize; ++i)
    {
        const auto ringIndex = (outputStart + i) % fftSize;
        state.outputRing[static_cast<size_t>(ringIndex)] +=
            state.fftData[static_cast<size_t>(i)] * windowBuffer[static_cast<size_t>(i)] * overlapScale;
    }

    if (channelIndex == 0)
        publishAnalyzer(state);
}

void FlipShiftEngine::publishAnalyzer(const ChannelState& state)
{
    const juce::SpinLock::ScopedLockType lock(analyzerLock);
    const auto binCount = state.inputSpectrum.size();
    const auto magnitudeScale = 4.0f / static_cast<float>(juce::jmax(1, fftSize));
    analyzerInputDb.resize(binCount);
    analyzerOutputDb.resize(binCount);

    for (size_t i = 0; i < binCount; ++i)
    {
        analyzerInputDb[i] = safeDb(std::abs(state.inputSpectrum[i]) * magnitudeScale);
        analyzerOutputDb[i] = safeDb(std::abs(state.outputSpectrum[i]) * magnitudeScale);
    }
}

void FlipShiftEngine::copyAnalyzerFrames(std::vector<float>& inputDb, std::vector<float>& outputDb) const
{
    const juce::SpinLock::ScopedLockType lock(analyzerLock);
    inputDb = analyzerInputDb;
    outputDb = analyzerOutputDb;
}
} // namespace openfad::flipshift
