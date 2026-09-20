#include "DSP/FlipShiftEngine.h"
#include "DSP/WaterfallAnalyzer.h"
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <thread>

using namespace openfad::flipshift;

namespace
{
constexpr auto testSampleRate = 48000.0;
constexpr auto blockSize = 128;
constexpr auto seconds = 2.0;

float sine(float frequency, int sample)
{
    return std::sin(juce::MathConstants<float>::twoPi * frequency * static_cast<float>(sample) / static_cast<float>(testSampleRate));
}

float detectPeakHz(const std::vector<float>& samples)
{
    const auto fftOrder = 15;
    const auto fftSize = 1 << fftOrder;
    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann, false);
    std::vector<float> data(static_cast<size_t>(fftSize * 2), 0.0f);

    const auto offset = juce::jmax(0, static_cast<int>(samples.size()) - fftSize);
    for (int i = 0; i < fftSize && offset + i < static_cast<int>(samples.size()); ++i)
        data[static_cast<size_t>(i)] = samples[static_cast<size_t>(offset + i)];

    window.multiplyWithWindowingTable(data.data(), fftSize);
    fft.performRealOnlyForwardTransform(data.data(), true);

    auto peakBin = 1;
    auto peakMagnitude = 0.0f;
    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        const auto real = data[static_cast<size_t>(bin * 2)];
        const auto imag = data[static_cast<size_t>(bin * 2 + 1)];
        const auto magnitude = std::hypot(real, imag);
        if (magnitude > peakMagnitude)
        {
            peakMagnitude = magnitude;
            peakBin = bin;
        }
    }

    return static_cast<float>(peakBin) * static_cast<float>(testSampleRate) / static_cast<float>(fftSize);
}

std::vector<float> render(SpectralMode mode, float inputHz, EngineParameters parameters)
{
    FlipShiftEngine engine;
    parameters.mode = mode;
    parameters.mix = 1.0f;
    engine.prepare(testSampleRate, blockSize, 1, parameters);

    const auto totalSamples = static_cast<int>(seconds * testSampleRate);
    std::vector<float> output(static_cast<size_t>(totalSamples), 0.0f);
    juce::AudioBuffer<float> buffer(1, blockSize);

    auto generated = 0;
    while (generated < totalSamples)
    {
        const auto count = juce::jmin(blockSize, totalSamples - generated);
        buffer.setSize(1, count, false, false, true);
        auto* data = buffer.getWritePointer(0);
        for (int i = 0; i < count; ++i)
            data[i] = sine(inputHz, generated + i) * 0.25f;

        engine.process(buffer, parameters);

        for (int i = 0; i < count; ++i)
            output[static_cast<size_t>(generated + i)] = buffer.getSample(0, i);

        generated += count;
    }

    return output;
}

void requireClose(const char* name, float actual, float expected, float tolerance)
{
    if (std::abs(actual - expected) > tolerance)
    {
        std::cerr << name << " failed: expected " << expected << " Hz, got " << actual << " Hz\n";
        std::exit(1);
    }

    std::cout << name << " ok: " << actual << " Hz\n";
}

void requireFinite(const char* name, const std::vector<float>& data)
{
    auto peak = 0.0f;
    for (const auto sample : data)
    {
        if (!std::isfinite(sample))
        {
            std::cerr << name << " failed: non-finite sample\n";
            std::exit(1);
        }
        peak = juce::jmax(peak, std::abs(sample));
    }

    if (peak > 4.0f)
    {
        std::cerr << name << " failed: excessive peak " << peak << "\n";
        std::exit(1);
    }

    std::cout << name << " ok: peak " << peak << "\n";
}

void requireUnityGain()
{
    EngineParameters parameters;
    parameters.quality = Quality::normal;
    parameters.amount = 1.0f;
    const auto output = render(SpectralMode::off, 440.0f, parameters);
    const auto start = output.size() / 2;
    auto sumSquares = 0.0;

    for (auto i = start; i < output.size(); ++i)
        sumSquares += static_cast<double>(output[i]) * static_cast<double>(output[i]);

    const auto rms = std::sqrt(sumSquares / static_cast<double>(output.size() - start));
    const auto expected = 0.25 / std::sqrt(2.0);
    if (std::abs(rms - expected) > 0.015)
    {
        std::cerr << "Unity gain failed: expected RMS " << expected << ", got " << rms << "\n";
        std::exit(1);
    }

    std::cout << "Unity gain ok: RMS " << rms << "\n";
}

void requireFreezeLatch()
{
    constexpr auto binCount = 513;
    std::vector<std::complex<float>> first(binCount);
    std::vector<std::complex<float>> second(binCount);
    std::vector<std::complex<float>> output;
    SpectralFrameMemory memory;
    SpectralTransformParameters parameters;
    parameters.mode = SpectralMode::off;
    parameters.freeze = true;
    memory.prepare(first.size(), parameters.fftSize, parameters.hopSize);
    output.resize(first.size());

    first[10] = { 1.0f, 0.0f };
    second[20] = { 1.0f, 0.0f };

    applySpectralMode(first, output, memory, parameters);
    applySpectralMode(second, output, memory, parameters);
    if (std::abs(output[10]) < 0.99f || std::abs(output[20]) > 0.01f)
    {
        std::cerr << "Freeze latch failed: frozen frame changed while Freeze was held\n";
        std::exit(1);
    }

    parameters.freeze = false;
    applySpectralMode(second, output, memory, parameters);
    if (std::abs(output[20]) < 0.99f)
    {
        std::cerr << "Freeze release failed: live frame did not resume\n";
        std::exit(1);
    }

    std::cout << "Freeze latch ok\n";
}

float renderPitchMapPeak(float inputHz, int root, PitchScale scale)
{
    constexpr auto fftSize = 8192;
    constexpr auto hopSize = fftSize / 4;
    constexpr auto binCount = fftSize / 2 + 1;
    std::vector<std::complex<float>> input(binCount);
    std::vector<std::complex<float>> output(binCount);
    SpectralFrameMemory memory;
    SpectralTransformParameters parameters;
    parameters.mode = SpectralMode::pitchMap;
    parameters.amount = 1.0f;
    parameters.sampleRate = static_cast<float>(testSampleRate);
    parameters.fftSize = fftSize;
    parameters.hopSize = hopSize;
    parameters.pitchRoot = root;
    parameters.pitchScale = scale;
    memory.prepare(input.size(), fftSize, hopSize);

    const auto sourceBin = juce::jlimit(
        1, binCount - 2,
        static_cast<int>(std::round(inputHz * static_cast<float>(fftSize)
                                    / static_cast<float>(testSampleRate))));
    input[static_cast<size_t>(sourceBin)] = { 1.0f, 0.0f };
    applySpectralMode(input, output, memory, parameters);

    const auto peak = std::max_element(output.begin() + 1, output.end() - 1,
        [](const auto left, const auto right) { return std::norm(left) < std::norm(right); });
    const auto peakBin = static_cast<int>(std::distance(output.begin(), peak));
    return static_cast<float>(peakBin) * static_cast<float>(testSampleRate) / static_cast<float>(fftSize);
}

void requirePitchMapQuantisation()
{
    const auto cMajorE = renderPitchMapPeak(329.63f, 0, PitchScale::major);
    const auto cMinorE = renderPitchMapPeak(329.63f, 0, PitchScale::minor);
    const auto cMajorFs = renderPitchMapPeak(369.99f, 0, PitchScale::major);
    const auto gMajorFs = renderPitchMapPeak(369.99f, 7, PitchScale::major);

    requireClose("Pitch Map C major keeps E", cMajorE, 329.63f, 7.0f);
    requireClose("Pitch Map C minor maps E to Eb", cMinorE, 311.13f, 7.0f);
    requireClose("Pitch Map C major maps F# to F", cMajorFs, 349.23f, 7.0f);
    requireClose("Pitch Map G major keeps F#", gMajorFs, 369.99f, 7.0f);

    constexpr auto fftSize = 8192;
    constexpr auto hopSize = fftSize / 4;
    constexpr auto binCount = fftSize / 2 + 1;
    std::vector<std::complex<float>> input(binCount);
    std::vector<std::complex<float>> output(binCount);
    SpectralFrameMemory memory;
    memory.prepare(input.size(), fftSize, hopSize);
    const auto sourceBin = static_cast<int>(std::round(369.99f * static_cast<float>(fftSize)
                                                        / static_cast<float>(testSampleRate)));
    input[static_cast<size_t>(sourceBin)] = { 1.0f, 0.0f };

    SpectralTransformParameters parameters;
    parameters.mode = SpectralMode::pitchMap;
    parameters.amount = 1.0f;
    parameters.sampleRate = static_cast<float>(testSampleRate);
    parameters.fftSize = fftSize;
    parameters.hopSize = hopSize;
    const auto* lowBinCache = memory.pitchMapLowBins.data();
    const auto* highBinCache = memory.pitchMapHighBins.data();
    const auto* weightCache = memory.pitchMapHighWeights.data();

    for (int root = 0; root < 12; ++root)
    {
        parameters.pitchRoot = root;
        parameters.pitchScale = root % 2 == 0 ? PitchScale::major : PitchScale::minor;
        applySpectralMode(input, output, memory, parameters);
        if (memory.pitchMapLowBins.data() != lowBinCache
            || memory.pitchMapHighBins.data() != highBinCache
            || memory.pitchMapHighWeights.data() != weightCache)
        {
            std::cerr << "Pitch Map failed: cache storage changed during audio processing\n";
            std::exit(1);
        }
    }

    const auto getOutputPeak = [&output]
    {
        const auto peak = std::max_element(output.begin() + 1, output.end() - 1,
            [](const auto left, const auto right) { return std::norm(left) < std::norm(right); });
        return static_cast<float>(std::distance(output.begin(), peak))
            * static_cast<float>(testSampleRate) / static_cast<float>(fftSize);
    };
    parameters.pitchRoot = 0;
    parameters.pitchScale = PitchScale::major;
    applySpectralMode(input, output, memory, parameters);
    const auto cachedCMajorFs = getOutputPeak();
    parameters.pitchRoot = 7;
    applySpectralMode(input, output, memory, parameters);
    const auto cachedGMajorFs = getOutputPeak();
    requireClose("Pitch Map cache refresh C major", cachedCMajorFs, 349.23f, 7.0f);
    requireClose("Pitch Map cache refresh G major", cachedGMajorFs, 369.99f, 7.0f);

    if (std::abs(cMajorE - cMinorE) < 10.0f || std::abs(cMajorFs - gMajorFs) < 10.0f)
    {
        std::cerr << "Pitch Map failed: root or major/minor selection did not change the mapping\n";
        std::exit(1);
    }

    std::cout << "Pitch Map root/scale quantisation ok\n";
}

void requireDeterministicGlitch()
{
    constexpr auto binCount = 513;
    std::vector<std::complex<float>> input(binCount);
    std::vector<std::complex<float>> left(binCount);
    std::vector<std::complex<float>> right(binCount);
    std::vector<std::complex<float>> replay(binCount);
    std::vector<float> firstPattern(binCount);
    for (int bin = 0; bin < binCount; ++bin)
    {
        const auto magnitude = 0.05f + static_cast<float>((bin * 17) % 31) * 0.003f;
        input[static_cast<size_t>(bin)] = std::polar(magnitude, static_cast<float>(bin) * 0.071f);
    }
    input.front() = { input.front().real(), 0.0f };
    input.back() = { input.back().real(), 0.0f };

    SpectralFrameMemory leftMemory;
    SpectralFrameMemory rightMemory;
    SpectralFrameMemory replayMemory;
    leftMemory.prepare(input.size(), 1024, 256);
    rightMemory.prepare(input.size(), 1024, 256);
    replayMemory.prepare(input.size(), 1024, 256);

    SpectralTransformParameters parameters;
    parameters.mode = SpectralMode::glitch;
    parameters.amount = 1.0f;
    parameters.shiftHz = 650.0f;
    parameters.pivotHz = 4000.0f;
    parameters.widthQ = 2.0f;
    parameters.channelIndex = 0;

    SpectralFrameMemory zeroDepthMemory;
    zeroDepthMemory.prepare(input.size(), 1024, 256);
    parameters.amount = 0.0f;
    applySpectralMode(input, replay, zeroDepthMemory, parameters);
    for (int bin = 0; bin < binCount; ++bin)
        if (std::abs(replay[static_cast<size_t>(bin)] - input[static_cast<size_t>(bin)]) > 1.0e-7f)
        {
            std::cerr << "Glitch failed: zero depth was not spectrally neutral\n";
            std::exit(1);
        }
    parameters.amount = 1.0f;

    auto changedInsideSelection = false;
    auto patternChangedAcrossEpochs = false;
    for (int frame = 0; frame < 18; ++frame)
    {
        parameters.channelIndex = 0;
        applySpectralMode(input, left, leftMemory, parameters);
        applySpectralMode(input, replay, replayMemory, parameters);
        parameters.channelIndex = 1;
        applySpectralMode(input, right, rightMemory, parameters);

        for (int bin = 0; bin < binCount; ++bin)
        {
            const auto frequency = static_cast<float>(bin) * 48000.0f / 1024.0f;
            const auto index = static_cast<size_t>(bin);
            if (std::abs(left[index] - replay[index]) > 1.0e-6f
                || std::abs(left[index] - right[index]) > 1.0e-6f)
            {
                std::cerr << "Glitch failed: deterministic replay or stereo mapping diverged\n";
                std::exit(1);
            }

            if (frequency < 3000.0f || frequency > 5000.0f)
            {
                if (std::abs(left[index] - input[index]) > 1.0e-5f)
                {
                    std::cerr << "Glitch failed: bins outside the selected band were transformed\n";
                    std::exit(1);
                }
            }
            else
            {
                const auto magnitude = std::abs(left[index]);
                if (frame == 0)
                    firstPattern[index] = magnitude;
                else if (std::abs(magnitude - firstPattern[index]) > 1.0e-4f)
                    patternChangedAcrossEpochs = true;

                if (std::abs(magnitude - std::abs(input[index])) > 1.0e-4f)
                    changedInsideSelection = true;
            }
        }
    }

    if (!changedInsideSelection)
    {
        std::cerr << "Glitch failed: selected spectrum never changed\n";
        std::exit(1);
    }

    if (!patternChangedAcrossEpochs)
    {
        std::cerr << "Glitch failed: selected spectrum did not change across hash epochs\n";
        std::exit(1);
    }

    std::cout << "Deterministic stereo-coherent multi-epoch selected-band Glitch ok\n";
}

void requireBypassLatency()
{
    FlipShiftEngine engine;
    EngineParameters parameters;
    parameters.quality = Quality::normal;
    parameters.bypass = true;
    engine.prepare(testSampleRate, blockSize, 1, parameters);

    const auto expectedLatency = FlipShiftEngine::getLatencySamplesForQuality(parameters.quality);
    const auto totalSamples = expectedLatency + blockSize;
    juce::AudioBuffer<float> buffer(1, blockSize);
    auto firstAudibleSample = -1;

    for (int generated = 0; generated < totalSamples; generated += blockSize)
    {
        const auto count = juce::jmin(blockSize, totalSamples - generated);
        buffer.setSize(1, count, false, false, true);
        buffer.clear();
        if (generated == 0)
            buffer.setSample(0, 0, 1.0f);

        engine.process(buffer, parameters);
        for (int i = 0; i < count; ++i)
            if (firstAudibleSample < 0 && std::abs(buffer.getSample(0, i)) > 0.5f)
                firstAudibleSample = generated + i;
    }

    if (engine.getLatencySamples() != expectedLatency || firstAudibleSample != expectedLatency)
    {
        std::cerr << "Latency failed: reported " << engine.getLatencySamples()
                  << ", impulse appeared at " << firstAudibleSample
                  << ", expected " << expectedLatency << "\n";
        std::exit(1);
    }

    std::cout << "Latency ok: " << expectedLatency << " samples\n";
}

void requireRandomisedStress()
{
    std::mt19937 random { 0x1dfb91u };
    std::uniform_real_distribution<float> inputDistribution { -0.25f, 0.25f };
    std::uniform_real_distribution<float> shiftDistribution { -5000.0f, 5000.0f };
    std::uniform_real_distribution<float> scaleDistribution { 0.25f, 4.0f };
    std::uniform_real_distribution<float> pivotDistribution { 20.0f, 20000.0f };
    std::uniform_real_distribution<float> amountDistribution { 0.0f, 1.0f };
    std::uniform_real_distribution<float> widthDistribution { 0.05f, 8.0f };
    std::uniform_real_distribution<float> gainDistribution { -24.0f, 12.0f };
    std::uniform_int_distribution<int> modeDistribution { 0, static_cast<int>(SpectralMode::count) - 1 };
    std::uniform_int_distribution<int> boolDistribution { 0, 1 };
    constexpr std::array blockSizes { 0, 1, 7, 31, 64, 127, 128, 511, 1024, 4096 };

    for (const auto rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (const auto quality : { Quality::low, Quality::normal, Quality::high })
        {
            EngineParameters parameters;
            parameters.quality = quality;
            FlipShiftEngine engine;
            engine.prepare(rate, 4096, 2, parameters);

            for (int iteration = 0; iteration < 40; ++iteration)
            {
                const auto currentBlockSize = blockSizes[static_cast<size_t>(random() % blockSizes.size())];
                juce::AudioBuffer<float> buffer(2, currentBlockSize);
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                        buffer.setSample(channel, sample, inputDistribution(random));

                parameters.mode = static_cast<SpectralMode>(modeDistribution(random));
                parameters.shiftHz = shiftDistribution(random);
                parameters.scale = scaleDistribution(random);
                parameters.pivotHz = pivotDistribution(random);
                parameters.amount = amountDistribution(random);
                parameters.widthQ = widthDistribution(random);
                parameters.mix = amountDistribution(random);
                parameters.outputGainDb = gainDistribution(random);
                parameters.bypass = boolDistribution(random) != 0;
                parameters.freeze = boolDistribution(random) != 0;
                engine.process(buffer, parameters);

                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    {
                        const auto value = buffer.getSample(channel, sample);
                        if (!std::isfinite(value) || std::abs(value) > 64.0f)
                        {
                            std::cerr << "Random stress failed at rate " << rate
                                      << ", quality " << static_cast<int>(quality)
                                      << ", iteration " << iteration << "\n";
                            std::exit(1);
                        }
                    }
            }
        }

    std::cout << "Randomised mode/quality stress ok\n";
}

void requireNonFiniteIsolation()
{
    EngineParameters parameters;
    parameters.quality = Quality::normal;
    parameters.shiftHz = std::numeric_limits<float>::quiet_NaN();
    parameters.scale = std::numeric_limits<float>::infinity();
    parameters.pivotHz = -std::numeric_limits<float>::infinity();
    parameters.amount = std::numeric_limits<float>::quiet_NaN();
    parameters.widthQ = std::numeric_limits<float>::infinity();
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.outputGainDb = std::numeric_limits<float>::infinity();

    FlipShiftEngine engine;
    engine.prepare(std::numeric_limits<double>::infinity(), blockSize, 2, parameters);
    juce::AudioBuffer<float> buffer(2, blockSize);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample,
                sample % 2 == 0 ? std::numeric_limits<float>::quiet_NaN()
                                : std::numeric_limits<float>::infinity());

    engine.process(buffer, parameters);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(buffer.getSample(channel, sample)))
            {
                std::cerr << "Non-finite isolation failed\n";
                std::exit(1);
            }

    std::cout << "Non-finite input and parameter isolation ok\n";
}

struct AnalyzerSnapshot
{
    std::vector<float> inputDb;
    std::vector<float> outputDb;
    std::uint64_t sequence = 0;
};

float getMeterDb(const std::vector<float>& values)
{
    return values.empty() ? -96.0f : *std::max_element(values.begin(), values.end());
}

float getAnalyzerPeakHz(const std::vector<float>& values, int fftSize)
{
    if (values.empty() || fftSize <= 0)
        return 0.0f;

    const auto peak = std::max_element(values.begin(), values.end());
    const auto peakBin = static_cast<int>(std::distance(values.begin(), peak));
    return static_cast<float>(peakBin) * static_cast<float>(testSampleRate) / static_cast<float>(fftSize);
}

AnalyzerSnapshot renderAnalyzerSnapshot(FlipShiftEngine& engine,
                                        const EngineParameters& parameters,
                                        float frequency,
                                        float amplitude,
                                        int& generatedSamples,
                                        std::uint64_t previousSequence)
{
    juce::AudioBuffer<float> buffer(1, blockSize);

    // Cover more than one publish interval and one complete FFT window so the
    // returned slice represents the requested signal rather than startup data.
    for (int block = 0; block < 48; ++block)
    {
        auto* data = buffer.getWritePointer(0);
        for (int sample = 0; sample < blockSize; ++sample)
            data[sample] = sine(frequency, generatedSamples + sample) * amplitude;

        engine.process(buffer, parameters);
        generatedSamples += blockSize;
    }

    AnalyzerSnapshot snapshot;
    snapshot.sequence = previousSequence;
    if (!engine.copyAnalyzerFrames(snapshot.inputDb, snapshot.outputDb, snapshot.sequence))
    {
        std::cerr << "Analyzer lifecycle failed: no native frame was published\n";
        std::exit(1);
    }

    return snapshot;
}

void requireUsableAnalyzerSnapshot(const char* stage,
                                   const AnalyzerSnapshot& snapshot,
                                   int expectedBins,
                                   float expectedPeakHz)
{
    if (snapshot.sequence == 0
        || snapshot.inputDb.size() != static_cast<size_t>(expectedBins)
        || snapshot.outputDb.size() != snapshot.inputDb.size())
    {
        std::cerr << stage << " failed: invalid native analyzer frame shape\n";
        std::exit(1);
    }

    auto inputFloor = 12.0f;
    auto outputFloor = 12.0f;
    for (size_t index = 0; index < snapshot.inputDb.size(); ++index)
    {
        const auto input = snapshot.inputDb[index];
        const auto output = snapshot.outputDb[index];
        if (!std::isfinite(input) || !std::isfinite(output)
            || input < -96.001f || input > 12.001f
            || output < -96.001f || output > 12.001f)
        {
            std::cerr << stage << " failed: analyzer contains an invalid dB value\n";
            std::exit(1);
        }

        inputFloor = juce::jmin(inputFloor, input);
        outputFloor = juce::jmin(outputFloor, output);
    }

    const auto inputMeterDb = getMeterDb(snapshot.inputDb);
    const auto outputMeterDb = getMeterDb(snapshot.outputDb);
    if (inputMeterDb < -36.0f || outputMeterDb < -36.0f
        || inputMeterDb - inputFloor < 24.0f
        || outputMeterDb - outputFloor < 24.0f)
    {
        std::cerr << stage << " failed: frame has no drawable spectrum or usable dB meter level\n";
        std::exit(1);
    }

    const auto inputPeakHz = getAnalyzerPeakHz(snapshot.inputDb, (expectedBins - 1) * 2);
    const auto outputPeakHz = getAnalyzerPeakHz(snapshot.outputDb, (expectedBins - 1) * 2);
    if (std::abs(inputPeakHz - expectedPeakHz) > 70.0f
        || std::abs(outputPeakHz - expectedPeakHz) > 70.0f
        || std::abs(inputMeterDb - outputMeterDb) > 0.05f)
    {
        std::cerr << stage << " failed: spectrum peak or input/output meter data is incorrect\n";
        std::exit(1);
    }
}

void requireAnalyzerSnapshot()
{
    EngineParameters parameters;
    parameters.mode = SpectralMode::off;
    parameters.quality = Quality::normal;
    parameters.mix = 1.0f;

    FlipShiftEngine engine;
    engine.prepare(testSampleRate, blockSize, 1, parameters);
    juce::AudioBuffer<float> buffer(1, blockSize);
    std::vector<float> input;
    std::vector<float> output;
    std::uint64_t sequence = 0;
    auto generatedSamples = 0;

    for (int block = 0; block < 16; ++block)
    {
        auto* data = buffer.getWritePointer(0);
        for (int sample = 0; sample < blockSize; ++sample)
            data[sample] = sine(220.0f, generatedSamples + sample) * 0.25f;
        engine.process(buffer, parameters);
        generatedSamples += blockSize;
    }

    if (engine.copyAnalyzerFrames(input, output, sequence))
    {
        std::cerr << "Analyzer lifecycle failed: data was published before an editor consumer attached\n";
        std::exit(1);
    }

    // Editor construction, uiReady, visibilityChanged, parentHierarchyChanged,
    // and the timer may all request the same active state.
    engine.setAnalyzerEnabled(true);
    engine.setAnalyzerEnabled(true);
    const auto first = renderAnalyzerSnapshot(engine, parameters, 440.0f, 0.25f, generatedSamples, sequence);
    requireUsableAnalyzerSnapshot("Analyzer first visible history slice", first, 513, 440.0f);

    const auto second = renderAnalyzerSnapshot(engine, parameters, 880.0f, 0.125f, generatedSamples, first.sequence);
    requireUsableAnalyzerSnapshot("Analyzer second visible history slice", second, 513, 880.0f);
    const auto firstInputMeterDb = getMeterDb(first.inputDb);
    const auto secondInputMeterDb = getMeterDb(second.inputDb);
    if (second.sequence <= first.sequence
        || secondInputMeterDb >= firstInputMeterDb - 4.5f
        || secondInputMeterDb <= firstInputMeterDb - 7.5f)
    {
        std::cerr << "Analyzer history failed: sequence, frequency, or 6 dB meter change was not preserved\n";
        std::exit(1);
    }

    sequence = second.sequence;
    engine.setAnalyzerEnabled(false);
    engine.setAnalyzerEnabled(false);
    for (int block = 0; block < 48; ++block)
    {
        auto* data = buffer.getWritePointer(0);
        for (int sample = 0; sample < blockSize; ++sample)
            data[sample] = sine(1760.0f, generatedSamples + sample) * 0.2f;
        engine.process(buffer, parameters);
        generatedSamples += blockSize;
    }

    if (engine.copyAnalyzerFrames(input, output, sequence))
    {
        std::cerr << "Analyzer lifecycle failed: hidden/destroyed editor still exposed a frame\n";
        std::exit(1);
    }

    engine.setAnalyzerEnabled(true);
    engine.setAnalyzerEnabled(true);
    if (engine.copyAnalyzerFrames(input, output, sequence))
    {
        std::cerr << "Analyzer lifecycle failed: reopened editor received a stale frame\n";
        std::exit(1);
    }

    const auto reopened = renderAnalyzerSnapshot(engine, parameters, 1760.0f, 0.2f, generatedSamples, sequence);
    requireUsableAnalyzerSnapshot("Analyzer reopened-editor history slice", reopened, 513, 1760.0f);
    if (reopened.sequence <= sequence)
    {
        std::cerr << "Analyzer lifecycle failed: reopened editor did not receive a fresh sequence\n";
        std::exit(1);
    }

    std::cout << "Native analyzer waterfall/meter lifecycle ok: sequences "
              << first.sequence << ", " << second.sequence << ", " << reopened.sequence
              << "; meters " << firstInputMeterDb << " dB, " << secondInputMeterDb << " dB\n";
}

void requireNewModesInAnalyzer()
{
    const auto checkMode = [](const char* name, EngineParameters parameters, float frequency)
    {
        FlipShiftEngine engine;
        engine.prepare(testSampleRate, blockSize, 1, parameters);
        engine.setAnalyzerEnabled(true);

        auto generatedSamples = 0;
        const auto snapshot = renderAnalyzerSnapshot(
            engine, parameters, frequency, 0.25f, generatedSamples, 0);
        const auto inputPeak = getAnalyzerPeakHz(snapshot.inputDb, engine.getFftSize());
        const auto outputPeak = getAnalyzerPeakHz(snapshot.outputDb, engine.getFftSize());
        auto maximumDbDifference = 0.0f;
        for (size_t bin = 0; bin < snapshot.inputDb.size(); ++bin)
            maximumDbDifference = juce::jmax(
                maximumDbDifference,
                std::abs(snapshot.inputDb[bin] - snapshot.outputDb[bin]));

        if (getMeterDb(snapshot.outputDb) < -48.0f
            || maximumDbDifference < 6.0f
            || std::abs(inputPeak - outputPeak) < 20.0f)
        {
            std::cerr << name << " analyzer failed: native output spectrum did not show the transform\n";
            std::exit(1);
        }

        std::cout << name << " native analyzer output ok: "
                  << inputPeak << " Hz -> " << outputPeak << " Hz\n";
    };

    EngineParameters glitch;
    glitch.mode = SpectralMode::glitch;
    glitch.amount = 1.0f;
    glitch.shiftHz = 650.0f;
    glitch.pivotHz = 4000.0f;
    glitch.widthQ = 2.0f;
    glitch.mix = 1.0f;
    glitch.quality = Quality::normal;
    checkMode("Glitch", glitch, 4000.0f);

    EngineParameters pitchMap;
    pitchMap.mode = SpectralMode::pitchMap;
    pitchMap.amount = 1.0f;
    pitchMap.mix = 1.0f;
    pitchMap.pitchRoot = 0;
    pitchMap.pitchScale = PitchScale::minor;
    pitchMap.quality = Quality::high;
    checkMode("Pitch Map", pitchMap, 1318.51f);
}

void requireConcurrentAnalyzerSnapshots()
{
    EngineParameters parameters;
    parameters.mode = SpectralMode::off;
    parameters.quality = Quality::normal;
    parameters.mix = 1.0f;

    FlipShiftEngine engine;
    engine.prepare(testSampleRate, blockSize, 1, parameters);
    engine.setAnalyzerEnabled(true);

    std::atomic<bool> producerFinished { false };
    std::atomic<bool> failed { false };
    std::atomic<int> snapshotsRead { 0 };

    std::thread producer([&]
    {
        juce::AudioBuffer<float> buffer(1, blockSize);
        auto observed = 0;
        for (int block = 0; block < 3000 && !failed.load(std::memory_order_relaxed); ++block)
        {
            auto* data = buffer.getWritePointer(0);
            for (int sample = 0; sample < blockSize; ++sample)
                data[sample] = sine(440.0f, block * blockSize + sample) * 0.25f;
            engine.process(buffer, parameters);
            // Give the consumer a guaranteed observation opportunity every 256
            // blocks. A loaded CI runner may otherwise schedule the entire
            // offline producer before the consumer gets ten time slices.
            if ((block + 1) % 256 == 0)
            {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (snapshotsRead.load(std::memory_order_acquire) == observed
                       && !failed.load(std::memory_order_acquire))
                {
                    if (std::chrono::steady_clock::now() >= deadline)
                    {
                        std::cerr << "Analyzer consumer made no progress\n";
                        failed.store(true, std::memory_order_release);
                        break;
                    }
                    std::this_thread::yield();
                }
                observed = snapshotsRead.load(std::memory_order_acquire);
            }
        }
        producerFinished.store(true, std::memory_order_release);
    });

    std::thread consumer([&]
    {
        std::vector<float> input;
        std::vector<float> output;
        std::uint64_t sequence = 0;
        auto idlePasses = 0;

        while (!producerFinished.load(std::memory_order_acquire) || idlePasses < 2000)
        {
            const auto previousSequence = sequence;
            if (!engine.copyAnalyzerFrames(input, output, sequence))
            {
                ++idlePasses;
                std::this_thread::yield();
                continue;
            }

            idlePasses = 0;
            if (input.size() != 513 || output.size() != input.size() || sequence <= previousSequence)
            {
                std::cerr << "Analyzer snapshot shape/sequence: " << input.size() << ", "
                          << output.size() << ", " << previousSequence << " -> " << sequence << '\n';
                failed.store(true, std::memory_order_release);
                break;
            }

            for (size_t index = 0; index < input.size(); ++index)
            {
                if (!std::isfinite(input[index]) || !std::isfinite(output[index])
                    || std::abs(input[index] - output[index]) > 1.0e-6f)
                {
                    std::cerr << "Analyzer snapshot bin " << index << ": "
                              << input[index] << " / " << output[index] << '\n';
                    failed.store(true, std::memory_order_release);
                    break;
                }
            }

            snapshotsRead.fetch_add(1, std::memory_order_relaxed);
            if (failed.load(std::memory_order_acquire))
                break;
        }
    });

    producer.join();
    consumer.join();

    if (failed.load(std::memory_order_acquire) || snapshotsRead.load(std::memory_order_relaxed) < 10)
    {
        std::cerr << "Concurrent analyzer snapshot stress failed, snapshots="
                  << snapshotsRead.load() << '\n';
        std::exit(1);
    }

    std::cout << "Concurrent analyzer snapshot stress ok\n";
}

void requirePhaseStability()
{
    constexpr auto binCount = 513;
    std::vector<std::complex<float>> input(binCount);
    std::vector<std::complex<float>> output(binCount);
    for (int bin = 0; bin < binCount; ++bin)
        input[static_cast<size_t>(bin)] = std::polar(0.1f, static_cast<float>(bin) * 0.013f);

    SpectralFrameMemory memory;
    SpectralTransformParameters parameters;
    parameters.mode = SpectralMode::pitchShift;
    parameters.scale = 1.37f;
    parameters.amount = 1.0f;
    memory.prepare(input.size(), parameters.fftSize, parameters.hopSize);

    for (int frame = 0; frame < 20000; ++frame)
        applySpectralMode(input, output, memory, parameters);

    for (const auto phase : memory.outputPhases)
        if (!std::isfinite(phase) || std::abs(phase) > juce::MathConstants<float>::pi + 1.0e-4f)
        {
            std::cerr << "Phase stability failed\n";
            std::exit(1);
        }

    std::cout << "Long-running phase stability ok\n";
}
void requireWaterfallDetail()
{
    WaterfallAnalyzer analyzer;
    analyzer.prepare(48000.0);
    analyzer.setEnabled(true);
    juce::AudioBuffer<float> audio(2, WaterfallAnalyzer::fftSize);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto phase = juce::MathConstants<float>::twoPi * static_cast<float>(i) / WaterfallAnalyzer::fftSize;
        const auto value = 0.25f * (std::sin(64 * phase) + std::sin(68 * phase));
        audio.setSample(0, i, value);
        audio.setSample(1, i, -value);
    }
    const auto original = audio.getSample(0, 100);
    analyzer.push(audio);
    std::vector<float> spectrum;
    const auto check = [](bool ok, const char* message) {
        if (!ok) { std::cerr << "Waterfall: " << message << '\n'; std::exit(1); }
    };
    check(analyzer.read(spectrum), "missing frame");
    check(spectrum.size() == WaterfallAnalyzer::bins, "wrong resolution");
    check(spectrum[64] > -13 && spectrum[68] > -13, "anti-phase stereo lost");
    check(spectrum[66] < std::min(spectrum[64], spectrum[68]) - 35, "nearby tones not resolved");
    check(audio.getSample(0, 100) == original, "capture modified audio");
    check(!analyzer.read(spectrum), "duplicate frame");
    analyzer.push(audio);
    analyzer.invalidate();
    check(!analyzer.read(spectrum), "stale reset frame");
    juce::AudioBuffer<float> shortBlock(2, 128);
    shortBlock.clear();
    analyzer.push(shortBlock);
    check(!analyzer.read(spectrum), "partial window published");
    analyzer.setEnabled(false);
    analyzer.push(audio);
    check(!analyzer.read(spectrum), "disabled frame published");
    analyzer.setEnabled(true);
    audio.clear();
    audio.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    analyzer.push(audio);
    check(analyzer.read(spectrum), "missing clean silence");
    for (const auto value : spectrum) check(value == -96.0f, "non-finite or silence contamination");
    std::atomic<bool> done { false };
    std::thread producer([&] {
        for (int i = 0; i < 2000; ++i) analyzer.push(audio);
        done.store(true, std::memory_order_release);
    });
    while (!done.load(std::memory_order_acquire))
        if (analyzer.read(spectrum))
            for (const auto value : spectrum) check(value == -96.0f, "torn concurrent snapshot");
    producer.join();
    std::cout << "8192-point waterfall: close tones, stereo, reset, finite and concurrent snapshots passed\n";
}
} // namespace

int main()
{
    requireWaterfallDetail();
    EngineParameters parameters;
    parameters.quality = Quality::normal;
    parameters.amount = 1.0f;
    parameters.widthQ = 1.0f;
    parameters.outputGainDb = 0.0f;

    std::cout << "Running Shift +100...\n";
    parameters.shiftHz = 100.0f;
    requireClose("Shift +100", detectPeakHz(render(SpectralMode::shift, 440.0f, parameters)), 540.0f, 20.0f);

    std::cout << "Running Shift -100...\n";
    parameters.shiftHz = -100.0f;
    requireClose("Shift -100", detectPeakHz(render(SpectralMode::shift, 440.0f, parameters)), 340.0f, 20.0f);

    std::cout << "Running Scale 0.5x...\n";
    parameters.shiftHz = 0.0f;
    parameters.scale = 0.5f;
    parameters.pivotHz = 20.0f;
    requireClose("Scale 0.5x", detectPeakHz(render(SpectralMode::pitchShift, 440.0f, parameters)), 220.0f, 25.0f);

    std::cout << "Running Scale 2x...\n";
    parameters.scale = 2.0f;
    requireClose("Scale 2x", detectPeakHz(render(SpectralMode::pitchShift, 440.0f, parameters)), 880.0f, 35.0f);

    std::cout << "Running Mirror...\n";
    parameters.scale = 1.0f;
    parameters.pivotHz = 1000.0f;
    requireClose("Mirror around 1k", detectPeakHz(render(SpectralMode::mirror, 440.0f, parameters)), 1560.0f, 80.0f);

    std::cout << "Running unity-gain reconstruction check...\n";
    requireUnityGain();

    std::cout << "Running bounded checks for every spectral mode...\n";
    for (auto modeIndex = 0; modeIndex < static_cast<int>(SpectralMode::count); ++modeIndex)
    {
        const auto name = std::string("Spectral mode ") + std::to_string(modeIndex);
        requireFinite(name.c_str(), render(static_cast<SpectralMode>(modeIndex), 440.0f, parameters));
    }

    std::cout << "Running sample-rate/quality setup checks...\n";
    for (auto rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (auto quality : { Quality::low, Quality::normal, Quality::high })
        {
            FlipShiftEngine engine;
            EngineParameters setupParameters;
            setupParameters.quality = quality;
            engine.prepare(rate, blockSize, 2, setupParameters);
            if (engine.getFftSize() <= 0 || engine.getHopSize() <= 0)
            {
                std::cerr << "Quality setup failed\n";
                return 1;
            }
        }

    std::cout << "Running Freeze latch check...\n";
    requireFreezeLatch();

    std::cout << "Running Glitch determinism and band-selection checks...\n";
    requireDeterministicGlitch();

    std::cout << "Running Pitch Map root/scale checks...\n";
    requirePitchMapQuantisation();

    std::cout << "Running latency check...\n";
    requireBypassLatency();

    std::cout << "Running non-finite isolation check...\n";
    requireNonFiniteIsolation();

    std::cout << "Running analyzer snapshot check...\n";
    requireAnalyzerSnapshot();

    std::cout << "Running Glitch/Pitch Map analyzer output checks...\n";
    requireNewModesInAnalyzer();

    std::cout << "Running concurrent analyzer snapshot stress...\n";
    requireConcurrentAnalyzerSnapshots();

    std::cout << "Running long-running phase check...\n";
    requirePhaseStability();

    std::cout << "Running randomised mode/quality stress...\n";
    requireRandomisedStress();

    std::cout << "FlipShift DSP tests passed.\n";
    return 0;
}
