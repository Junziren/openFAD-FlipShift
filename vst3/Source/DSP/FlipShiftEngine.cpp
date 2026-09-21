#include "DSP/FlipShiftEngine.h"
#include <cmath>

namespace openfad::flipshift
{
namespace
{
float safeDb(float magnitude)
{
    if (!std::isfinite(magnitude))
        return -96.0f;
    return juce::jlimit(-96.0f, 12.0f, juce::Decibels::gainToDecibels(magnitude + 1.0e-8f));
}

float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
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

Quality sanitiseQuality(Quality quality) noexcept
{
    const auto index = juce::jlimit(0, 2, static_cast<int>(quality));
    return static_cast<Quality>(index);
}

EngineParameters sanitiseParameters(const EngineParameters& input, double sampleRate)
{
    auto result = input;
    result.mode = static_cast<SpectralMode>(juce::jlimit(
        0, static_cast<int>(SpectralMode::count) - 1, static_cast<int>(input.mode)));
    result.shiftHz = juce::jlimit(-5000.0f, 5000.0f, finiteOr(input.shiftHz, 0.0f));
    result.scale = juce::jlimit(0.25f, 4.0f, finiteOr(input.scale, 1.0f));
    result.pivotHz = juce::jlimit(
        20.0f, static_cast<float>(juce::jmax(20.0, sampleRate * 0.5)), finiteOr(input.pivotHz, 1000.0f));
    result.amount = juce::jlimit(0.0f, 1.0f, finiteOr(input.amount, 0.5f));
    result.widthQ = juce::jlimit(0.05f, 8.0f, finiteOr(input.widthQ, 1.0f));
    result.pitchRoot = juce::jlimit(0, 11, input.pitchRoot);
    result.pitchScale = static_cast<PitchScale>(juce::jlimit(0, 1, static_cast<int>(input.pitchScale)));
    result.mix = juce::jlimit(0.0f, 1.0f, finiteOr(input.mix, 0.5f));
    result.outputGainDb = juce::jlimit(-24.0f, 12.0f, finiteOr(input.outputGainDb, 0.0f));
    result.quality = sanitiseQuality(input.quality);
    return result;
}
} // namespace

int FlipShiftEngine::getLatencySamplesForQuality(Quality quality) noexcept
{
    return 1 << orderForQuality(quality);
}

void FlipShiftEngine::prepare(double newSampleRate,
                              int,
                              int channels,
                              const EngineParameters& initialParameters)
{
    sampleRate = std::isfinite(newSampleRate) && newSampleRate > 0.0 ? newSampleRate : 48000.0;
    numChannels = juce::jmax(1, channels);
    const auto parameters = sanitiseParameters(initialParameters, sampleRate);
    configureQuality(parameters.quality, parameters);
}

void FlipShiftEngine::configureQuality(Quality quality, const EngineParameters& initialParameters)
{
    currentQuality = sanitiseQuality(quality);
    fftOrder = orderForQuality(currentQuality);
    fftSize = getLatencySamplesForQuality(currentQuality);
    hopSize = fftSize / 4;
    ringMask = fftSize - 1;
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
        state.memory.prepare(state.inputSpectrum.size(), fftSize, hopSize);
        state.writePosition = 0;
        state.samplesUntilFrame = hopSize;
    }

    analyzerHasData.store(false, std::memory_order_release);
    analyzerFramesUntilPublish = 0;
    const auto framesPerSecond = sampleRate / static_cast<double>(juce::jmax(1, hopSize));
    analyzerPublishInterval = juce::jmax(1, static_cast<int>(std::round(framesPerSecond / 15.0)));

    const auto rampSeconds = 0.02;
    shiftSmoother.reset(sampleRate, rampSeconds);
    scaleSmoother.reset(sampleRate, rampSeconds);
    pivotSmoother.reset(sampleRate, rampSeconds);
    amountSmoother.reset(sampleRate, rampSeconds);
    widthSmoother.reset(sampleRate, rampSeconds);
    mixSmoother.reset(sampleRate, rampSeconds);
    linearGainSmoother.reset(sampleRate, rampSeconds);
    shiftSmoother.setCurrentAndTargetValue(initialParameters.shiftHz);
    scaleSmoother.setCurrentAndTargetValue(initialParameters.scale);
    pivotSmoother.setCurrentAndTargetValue(initialParameters.pivotHz);
    amountSmoother.setCurrentAndTargetValue(initialParameters.amount);
    widthSmoother.setCurrentAndTargetValue(initialParameters.widthQ);
    mixSmoother.setCurrentAndTargetValue(initialParameters.mix);
    linearGainSmoother.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(initialParameters.outputGainDb));
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
        state.memory.reset();
        state.writePosition = 0;
        state.samplesUntilFrame = hopSize;
    }

    analyzerHasData.store(false, std::memory_order_release);
    analyzerFramesUntilPublish = 0;
}

void FlipShiftEngine::process(juce::AudioBuffer<float>& buffer, const EngineParameters& parameters)
{
    const auto target = sanitiseParameters(parameters, sampleRate);
    jassert(target.quality == currentQuality);

    shiftSmoother.setTargetValue(target.shiftHz);
    scaleSmoother.setTargetValue(target.scale);
    pivotSmoother.setTargetValue(target.pivotHz);
    amountSmoother.setTargetValue(target.amount);
    widthSmoother.setTargetValue(target.widthQ);
    mixSmoother.setTargetValue(target.mix);
    linearGainSmoother.setTargetValue(juce::Decibels::decibelsToGain(target.outputGainDb));

    const auto channelsToProcess = juce::jmin(buffer.getNumChannels(), static_cast<int>(channelStates.size()));
    const auto samples = buffer.getNumSamples();
    auto* const* channelData = buffer.getArrayOfWritePointers();

    for (int sample = 0; sample < samples; ++sample)
    {
        auto smoothed = getSmoothedParameters(target);
        const auto outputGain = linearGainSmoother.getNextValue();

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto& state = channelStates[static_cast<size_t>(channel)];
            const auto processed = processSample(state, channelData[channel][sample], smoothed, channel) * outputGain;
            channelData[channel][sample] = std::isfinite(processed) ? processed : 0.0f;
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
    return values;
}

float FlipShiftEngine::processSample(ChannelState& state, float input, const EngineParameters& parameters, int channelIndex)
{
    input = std::isfinite(input) ? input : 0.0f;
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

    writePosition = (writePosition + 1) & ringMask;

    if (parameters.bypass)
        return dry;

    const auto mix = juce::jlimit(0.0f, 1.0f, parameters.mix);
    return dry * (1.0f - mix) + wet * mix;
}

void FlipShiftEngine::processFrame(ChannelState& state, const EngineParameters& parameters, int channelIndex)
{
    const auto frameStart = (state.writePosition + 1) & ringMask;

    std::fill(state.fftData.begin(), state.fftData.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
    {
        const auto ringIndex = (frameStart + i) & ringMask;
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
    transformParams.pitchRoot = parameters.pitchRoot;
    transformParams.pitchScale = parameters.pitchScale;
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
    const auto outputStart = (state.writePosition + 1) & ringMask;
    for (int i = 0; i < fftSize; ++i)
    {
        const auto ringIndex = (outputStart + i) & ringMask;
        state.outputRing[static_cast<size_t>(ringIndex)] +=
            state.fftData[static_cast<size_t>(i)] * windowBuffer[static_cast<size_t>(i)] * overlapScale;
    }

    if (channelIndex == 0 && analyzerEnabled.load(std::memory_order_relaxed))
    {
        if (analyzerFramesUntilPublish-- <= 0)
        {
            publishAnalyzer(state);
            analyzerFramesUntilPublish = analyzerPublishInterval - 1;
        }
    }
    else if (channelIndex == 0)
    {
        analyzerFramesUntilPublish = 0;
    }
}

void FlipShiftEngine::publishAnalyzer(const ChannelState& state)
{
    const auto generation = analyzerGeneration.load(std::memory_order_acquire);
    if (!analyzerEnabled.load(std::memory_order_acquire))
        return;

    auto& frame = analyzerFrames[static_cast<size_t>(analyzerWriteIndex)];
    const auto binCount = juce::jmin(static_cast<int>(state.inputSpectrum.size()), maximumAnalyzerBins);
    const auto magnitudeScale = 4.0f / static_cast<float>(juce::jmax(1, fftSize));

    for (int i = 0; i < binCount; ++i)
    {
        const auto index = static_cast<size_t>(i);
        frame.inputDb[index] = safeDb(std::abs(state.inputSpectrum[index]) * magnitudeScale);
        frame.outputDb[index] = safeDb(std::abs(state.outputSpectrum[index]) * magnitudeScale);
    }

    frame.binCount = binCount;
    frame.sequence = ++analyzerSequence;
    frame.generation = generation;

    if (!analyzerEnabled.load(std::memory_order_acquire)
        || generation != analyzerGeneration.load(std::memory_order_acquire))
        return;

    analyzerWriteIndex = analyzerReadyIndex.exchange(analyzerWriteIndex | analyzerDirtyBit,
                                                     std::memory_order_acq_rel) & analyzerIndexMask;
    analyzerHasData.store(true, std::memory_order_release);
}

void FlipShiftEngine::setAnalyzerEnabled(bool shouldBeEnabled) noexcept
{
    if (analyzerEnabled.load(std::memory_order_acquire) == shouldBeEnabled)
        return;

    analyzerGeneration.fetch_add(1, std::memory_order_acq_rel);
    analyzerEnabled.store(shouldBeEnabled, std::memory_order_release);
    invalidateAnalyzerFrames();
}

void FlipShiftEngine::invalidateAnalyzerFrames() noexcept
{
    analyzerHasData.store(false, std::memory_order_release);
}

bool FlipShiftEngine::copyAnalyzerFrames(std::vector<float>& inputDb,
                                         std::vector<float>& outputDb,
                                         std::uint64_t& sequence) const
{
    if (!analyzerEnabled.load(std::memory_order_acquire)
        || !analyzerHasData.load(std::memory_order_acquire))
        return false;

    if ((analyzerReadyIndex.load(std::memory_order_acquire) & analyzerDirtyBit) == 0)
        return false;

    analyzerReadIndex = analyzerReadyIndex.exchange(analyzerReadIndex, std::memory_order_acq_rel) & analyzerIndexMask;
    const auto& frame = analyzerFrames[static_cast<size_t>(analyzerReadIndex)];
    const auto generation = analyzerGeneration.load(std::memory_order_acquire);
    if (frame.generation != generation
        || frame.sequence == static_cast<std::uint32_t>(sequence)
        || !analyzerEnabled.load(std::memory_order_acquire))
        return false;

    const auto count = juce::jlimit(0, maximumAnalyzerBins, frame.binCount);
    inputDb.assign(frame.inputDb.begin(), frame.inputDb.begin() + count);
    outputDb.assign(frame.outputDb.begin(), frame.outputDb.begin() + count);

    if (frame.generation != analyzerGeneration.load(std::memory_order_acquire)
        || !analyzerEnabled.load(std::memory_order_acquire))
        return false;

    sequence = frame.sequence;
    return true;
}
} // namespace openfad::flipshift
