#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace openfad::flipshift
{
// Display-only output analysis. The audio thread only captures bounded snapshots;
// the editor/message thread performs the two 8192-point FFTs.
class WaterfallAnalyzer
{
public:
    static constexpr int fftSize = 8192;
    static constexpr int bins = fftSize / 2 + 1;
    static_assert(std::atomic<int>::is_always_lock_free);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);

    WaterfallAnalyzer() : storage(std::make_unique<Storage>()), fft(13)
    {
        juce::dsp::WindowingFunction<float>::fillWindowingTables(
            window.data(), fftSize, juce::dsp::WindowingFunction<float>::hann, false);
        float sum = 0;
        for (const auto value : window) sum += value;
        magnitudeScale = 2.0f / sum;
    }

    void prepare(double sampleRate) noexcept
    {
        hop.store(juce::jmax(1, static_cast<int>(sampleRate / 30.0)), std::memory_order_relaxed);
        invalidate();
    }

    void invalidate() noexcept { generation.fetch_add(1, std::memory_order_release); }

    void setEnabled(bool value) noexcept
    {
        if (enabled.exchange(value, std::memory_order_acq_rel) != value) invalidate();
    }

    void push(const juce::AudioBuffer<float>& audio) noexcept
    {
        if (!enabled.load(std::memory_order_acquire) || audio.getNumChannels() == 0) return;
        const auto currentGeneration = generation.load(std::memory_order_acquire);
        if (producerGeneration != currentGeneration)
        {
            producerGeneration = currentGeneration;
            position = collected = sincePublish = 0;
        }
        const auto* left = audio.getReadPointer(0);
        const auto* right = audio.getReadPointer(juce::jmin(1, audio.getNumChannels() - 1));
        const auto interval = hop.load(std::memory_order_relaxed);
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto clean = [](float value) {
                return std::isfinite(value) ? juce::jlimit(-32.0f, 32.0f, value) : 0.0f;
            };
            storage->ring[0][static_cast<size_t>(position)] = clean(left[sample]);
            storage->ring[1][static_cast<size_t>(position)] = clean(right[sample]);
            position = (position + 1) & (fftSize - 1);
            collected = juce::jmin(fftSize, collected + 1);
            if (++sincePublish < interval || collected < fftSize) continue;
            sincePublish = 0;
            auto& frame = storage->frames[static_cast<size_t>(writeIndex)];
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto& ring = storage->ring[static_cast<size_t>(channel)];
                auto& target = frame.audio[static_cast<size_t>(channel)];
                std::copy(ring.begin() + position, ring.end(), target.begin());
                std::copy(ring.begin(), ring.begin() + position, target.begin() + fftSize - position);
            }
            frame.generation = currentGeneration;
            frame.sequence = ++producerSequence;
            writeIndex = readyIndex.exchange(writeIndex | dirtyBit, std::memory_order_acq_rel) & indexMask;
        }
    }

    // Single consumer (the editor message thread). Returned dB is stereo power,
    // so opposite-polarity left/right signals do not cancel in the display.
    bool read(std::vector<float>& output)
    {
        if (!enabled.load(std::memory_order_acquire)) return false;
        if ((readyIndex.load(std::memory_order_acquire) & dirtyBit) == 0) return false;
        readIndex = readyIndex.exchange(readIndex, std::memory_order_acq_rel) & indexMask;
        const auto& frame = storage->frames[static_cast<size_t>(readIndex)];
        if (frame.generation != generation.load(std::memory_order_acquire)) return false;
        for (int channel = 0; channel < 2; ++channel)
        {
            auto& scratch = storage->fftScratch[static_cast<size_t>(channel)];
            std::fill(scratch.begin(), scratch.end(), 0.0f);
            for (int i = 0; i < fftSize; ++i)
                scratch[static_cast<size_t>(i)] = frame.audio[static_cast<size_t>(channel)][static_cast<size_t>(i)] * window[static_cast<size_t>(i)];
            fft.performFrequencyOnlyForwardTransform(scratch.data(), true);
        }
        if (frame.generation != generation.load(std::memory_order_acquire)) return false;
        output.resize(bins);
        for (int bin = 0; bin < bins; ++bin)
        {
            const auto left = storage->fftScratch[0][static_cast<size_t>(bin)] * magnitudeScale;
            const auto right = storage->fftScratch[1][static_cast<size_t>(bin)] * magnitudeScale;
            const auto power = 0.5f * (left * left + right * right);
            output[static_cast<size_t>(bin)] = juce::jlimit(-96.0f, 12.0f, 10.0f * std::log10(juce::jmax(power, 1.0e-10f)));
        }
        return true;
    }

private:
    static constexpr int dirtyBit = 4, indexMask = 3;
    struct Frame
    {
        std::array<std::array<float, fftSize>, 2> audio {};
        std::uint32_t generation = 0, sequence = 0;
    };
    struct Storage
    {
        std::array<std::array<float, fftSize>, 2> ring {};
        std::array<Frame, 3> frames {};
        std::array<std::array<float, fftSize * 2>, 2> fftScratch {};
    };
    std::unique_ptr<Storage> storage;
    juce::dsp::FFT fft;
    std::array<float, fftSize> window {};
    float magnitudeScale = 1.0f;
    std::atomic<bool> enabled { false };
    std::atomic<int> hop { 1600 }, readyIndex { 1 };
    std::atomic<std::uint32_t> generation { 1 };
    int writeIndex = 0, readIndex = 2, position = 0, collected = 0, sincePublish = 0;
    std::uint32_t producerGeneration = 0, producerSequence = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaterfallAnalyzer)
};
}
