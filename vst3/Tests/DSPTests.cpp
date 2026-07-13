#include "DSP/FlipShiftEngine.h"
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <iostream>
#include <random>
#include <string>

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
    engine.prepare(testSampleRate, blockSize, 1, parameters.quality);
    parameters.mode = mode;
    parameters.mix = 1.0f;

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

void requireBypassLatency()
{
    FlipShiftEngine engine;
    EngineParameters parameters;
    parameters.quality = Quality::normal;
    parameters.bypass = true;
    engine.prepare(testSampleRate, blockSize, 1, parameters.quality);

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
    std::uniform_int_distribution<int> qualityDistribution { 0, 2 };
    std::uniform_int_distribution<int> boolDistribution { 0, 1 };
    constexpr std::array blockSizes { 32, 64, 127, 256, 511, 1024 };

    for (const auto rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        FlipShiftEngine engine;
        engine.prepare(rate, 1024, 2, Quality::normal);

        for (int iteration = 0; iteration < 120; ++iteration)
        {
            const auto currentBlockSize = blockSizes[static_cast<size_t>(random() % blockSizes.size())];
            juce::AudioBuffer<float> buffer(2, currentBlockSize);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    buffer.setSample(channel, sample, inputDistribution(random));

            EngineParameters parameters;
            parameters.mode = static_cast<SpectralMode>(modeDistribution(random));
            parameters.shiftHz = shiftDistribution(random);
            parameters.scale = scaleDistribution(random);
            parameters.pivotHz = pivotDistribution(random);
            parameters.amount = amountDistribution(random);
            parameters.widthQ = widthDistribution(random);
            parameters.mix = amountDistribution(random);
            parameters.outputGainDb = gainDistribution(random);
            parameters.quality = static_cast<Quality>(qualityDistribution(random));
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
                                  << ", iteration " << iteration << "\n";
                        std::exit(1);
                    }
                }
        }
    }

    std::cout << "Randomised mode/quality stress ok\n";
}
} // namespace

int main()
{
    EngineParameters parameters;
    parameters.quality = Quality::normal;
    parameters.amount = 1.0f;
    parameters.widthQ = 1.0f;
    parameters.outputGainDb = 0.0f;

    std::cout << "Running Shift +100...\n";
    parameters.shiftHz = 100.0f;
    requireClose("Shift +100", detectPeakHz(render(SpectralMode::shift, 440.0f, parameters)), 540.0f, 20.0f);

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
    for (auto rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (auto quality : { Quality::low, Quality::normal, Quality::high })
        {
            FlipShiftEngine engine;
            engine.prepare(rate, blockSize, 2, quality);
            if (engine.getFftSize() <= 0 || engine.getHopSize() <= 0)
            {
                std::cerr << "Quality setup failed\n";
                return 1;
            }
        }

    std::cout << "Running Freeze latch check...\n";
    requireFreezeLatch();

    std::cout << "Running latency check...\n";
    requireBypassLatency();

    std::cout << "Running randomised mode/quality stress...\n";
    requireRandomisedStress();

    std::cout << "FlipShift DSP tests passed.\n";
    return 0;
}
