#include "DSP/SpectralModes.h"

namespace openfad::flipshift
{
namespace
{
using Complex = std::complex<float>;

float clamp01(float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float binHz(int bin, int binCount, float sampleRate)
{
    return static_cast<float>(bin) * (sampleRate * 0.5f) / static_cast<float>(juce::jmax(1, binCount - 1));
}

Complex sampleAtFrequency(const std::vector<Complex>& spectrum, float frequency, float sampleRate)
{
    if (spectrum.empty() || frequency < 0.0f || frequency > sampleRate * 0.5f)
        return {};

    const auto position = frequency / (sampleRate * 0.5f) * static_cast<float>(spectrum.size() - 1);
    const auto low = juce::jlimit(0, static_cast<int>(spectrum.size() - 1), static_cast<int>(std::floor(position)));
    const auto high = juce::jlimit(0, static_cast<int>(spectrum.size() - 1), low + 1);
    const auto alpha = position - static_cast<float>(low);
    return spectrum[static_cast<size_t>(low)] * (1.0f - alpha) + spectrum[static_cast<size_t>(high)] * alpha;
}

int findPeakBin(const std::vector<Complex>& spectrum)
{
    auto peakBin = 1;
    auto peakMagnitude = 0.0f;

    for (int i = 1; i < static_cast<int>(spectrum.size()); ++i)
    {
        const auto mag = std::abs(spectrum[static_cast<size_t>(i)]);
        if (mag > peakMagnitude)
        {
            peakMagnitude = mag;
            peakBin = i;
        }
    }

    return peakBin;
}

float harmonicWeight(float frequency, float fundamental, float widthHz)
{
    if (fundamental <= 1.0f)
        return 1.0f;

    const auto harmonic = juce::jmax(1.0f, std::round(frequency / fundamental));
    const auto target = harmonic * fundamental;
    const auto distance = std::abs(frequency - target);
    return std::exp(-(distance * distance) / juce::jmax(1.0f, 2.0f * widthHz * widthHz));
}

float octaveWindow(float frequency, float pivot, float wide)
{
    if (frequency <= 1.0f || pivot <= 1.0f)
        return 0.0f;

    const auto oct = std::log2(frequency / pivot);
    const auto phase = oct - std::floor(oct);
    const auto raised = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
    return std::pow(raised, juce::jmap(wide, 2.0f, 0.65f));
}

Complex withMagnitudePhase(float magnitude, float phase)
{
    return std::polar(magnitude, phase);
}

float phaseOf(Complex value)
{
    return std::atan2(value.imag(), value.real());
}

float wrapPhase(float phase)
{
    while (phase > juce::MathConstants<float>::pi)
        phase -= juce::MathConstants<float>::twoPi;
    while (phase < -juce::MathConstants<float>::pi)
        phase += juce::MathConstants<float>::twoPi;
    return phase;
}

bool isFrequencyRemapMode(SpectralMode mode)
{
    switch (mode)
    {
        case SpectralMode::shift:
        case SpectralMode::detune:
        case SpectralMode::spread:
        case SpectralMode::mirror:
        case SpectralMode::pitchShift:
        case SpectralMode::peakOctUp:
        case SpectralMode::peakOctDown:
        case SpectralMode::peakHmxUp:
        case SpectralMode::peakHmxDown:
        case SpectralMode::peakFollow:
        case SpectralMode::subharm:
            return true;
        default:
            return false;
    }
}
} // namespace

void applySpectralMode(const std::vector<Complex>& input,
                       std::vector<Complex>& output,
                       SpectralFrameMemory& memory,
                       const SpectralTransformParameters& params)
{
    const auto binCount = static_cast<int>(input.size());
    output.assign(input.size(), {});

    if (binCount == 0)
        return;

    if (memory.frozen.size() != input.size())
        memory.frozen.assign(input.size(), {});
    if (memory.smearMagnitudes.size() != input.size())
        memory.smearMagnitudes.assign(input.size(), 0.0f);
    if (memory.previousInputPhases.size() != input.size())
    {
        memory.previousInputPhases.assign(input.size(), 0.0f);
        memory.outputPhases.assign(input.size(), 0.0f);
        memory.phaseInitialised = false;
    }

    const auto amount = clamp01(params.amount);
    const auto scale = juce::jlimit(0.05f, 8.0f, params.scale);
    const auto pivot = juce::jlimit(20.0f, params.sampleRate * 0.5f, params.pivotHz);
    const auto q = juce::jlimit(0.05f, 8.0f, params.widthQ);
    const auto peakBin = findPeakBin(input);
    const auto peakHz = binHz(peakBin, binCount, params.sampleRate);
    const auto stereoSign = params.numChannels > 1 && params.channelIndex == 1 ? -1.0f : 1.0f;
    const auto mode = params.mode;

    if (params.freeze && !memory.freezeActive)
        memory.frozen = input;

    memory.freezeActive = params.freeze;

    const auto& source = params.freeze ? memory.frozen : input;

    for (int bin = 0; bin < binCount; ++bin)
    {
        const auto frequency = binHz(bin, binCount, params.sampleRate);
        auto srcFrequency = frequency;
        auto remapMode = isFrequencyRemapMode(mode);
        auto wet = source[static_cast<size_t>(bin)];

        switch (mode)
        {
            case SpectralMode::off:
                wet = source[static_cast<size_t>(bin)];
                break;

            case SpectralMode::shift:
                srcFrequency = frequency - params.shiftHz * amount;
                wet = sampleAtFrequency(source, frequency - params.shiftHz * amount, params.sampleRate);
                break;

            case SpectralMode::detune:
                srcFrequency = pivot + (frequency - pivot) / (1.0f + params.shiftHz * 0.00005f * amount);
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::spread:
                srcFrequency = frequency - params.shiftHz * amount * stereoSign;
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::mirror:
                srcFrequency = pivot - (frequency - pivot) * scale;
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::pitchShift:
                srcFrequency = pivot + (frequency - pivot) / scale;
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::pitchBlend:
            {
                const auto shifted = sampleAtFrequency(source, pivot + (frequency - pivot) / scale, params.sampleRate);
                wet = source[static_cast<size_t>(bin)] * (1.0f - amount) + shifted * amount;
                break;
            }

            case SpectralMode::peakOctUp:
                srcFrequency = peakHz + (frequency - peakHz) * 0.5f;
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::peakOctDown:
                srcFrequency = peakHz + (frequency - peakHz) * 2.0f;
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::peakHmxUp:
                srcFrequency = peakHz + (frequency - peakHz) / juce::jmax(1.0f, scale + 1.0f);
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::peakHmxDown:
                srcFrequency = peakHz + (frequency - peakHz) * juce::jmax(1.0f, scale + 1.0f);
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::peakFollow:
                srcFrequency = frequency - (peakHz - pivot) * amount;
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;

            case SpectralMode::smear:
            {
                const auto radius = juce::jlimit(1, binCount / 4, static_cast<int>(8.0f + amount * q * 44.0f));
                auto mag = 0.0f;
                auto phase = phaseOf(source[static_cast<size_t>(bin)]);
                auto count = 0;
                for (auto i = juce::jmax(0, bin - radius); i <= juce::jmin(binCount - 1, bin + radius); ++i)
                {
                    mag += std::abs(source[static_cast<size_t>(i)]);
                    ++count;
                }
                mag /= static_cast<float>(juce::jmax(1, count));
                memory.smearMagnitudes[static_cast<size_t>(bin)] =
                    memory.smearMagnitudes[static_cast<size_t>(bin)] * 0.82f + mag * 0.18f;
                wet = withMagnitudePhase(memory.smearMagnitudes[static_cast<size_t>(bin)], phase);
                break;
            }

            case SpectralMode::harmonics:
            {
                const auto width = juce::jmax(4.0f, pivot * (0.015f + (1.0f - amount) * 0.1f) * q);
                wet *= juce::jmap(harmonicWeight(frequency, pivot, width), 1.0f - amount, 1.0f);
                break;
            }

            case SpectralMode::subharm:
            {
                srcFrequency = frequency * (1.0f + std::floor(amount * 3.0f));
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                wet *= 0.85f;
                break;
            }

            case SpectralMode::gate:
            {
                const auto peakMag = std::abs(source[static_cast<size_t>(peakBin)]);
                const auto threshold = peakMag * amount * 0.8f;
                wet = std::abs(wet) >= threshold ? wet : wet * (1.0f - amount);
                break;
            }

            case SpectralMode::robotize:
                wet = withMagnitudePhase(std::abs(wet), 0.0f);
                break;

            case SpectralMode::harmSweep:
            {
                const auto movingFundamental = pivot * std::pow(2.0f, params.shiftHz / 1200.0f);
                const auto width = juce::jmax(6.0f, movingFundamental * 0.04f * q);
                wet *= harmonicWeight(frequency, movingFundamental, width);
                break;
            }

            case SpectralMode::shepard:
            case SpectralMode::shepardWide:
            {
                const auto wide = mode == SpectralMode::shepardWide ? 1.0f : 0.0f;
                auto sum = Complex {};
                for (int octave = -3; octave <= 3; ++octave)
                {
                    const auto ratio = std::pow(2.0f, static_cast<float>(octave) + amount);
                    const auto candidate = frequency / ratio;
                    sum += sampleAtFrequency(source, candidate, params.sampleRate) * octaveWindow(candidate, pivot, wide);
                }
                wet = sum * 0.35f;
                break;
            }

            case SpectralMode::comb:
            {
                const auto spacing = juce::jmax(20.0f, std::abs(params.shiftHz) + pivot * 0.1f);
                const auto phase = std::fmod(frequency, spacing) / spacing;
                const auto mask = 0.5f + 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
                wet *= juce::jmap(mask, 1.0f - amount, 1.0f);
                break;
            }

            case SpectralMode::phaseTwist:
            {
                const auto phase = phaseOf(wet) + amount * juce::MathConstants<float>::twoPi * std::sin(frequency / juce::jmax(20.0f, pivot));
                wet = withMagnitudePhase(std::abs(wet), phase);
                break;
            }

            case SpectralMode::count:
                break;
        }

        if (mode != SpectralMode::pitchBlend && mode != SpectralMode::off)
            wet = source[static_cast<size_t>(bin)] * (1.0f - amount) + wet * amount;

        if (remapMode && std::abs(wet) > 0.0f)
        {
            const auto sourcePosition = juce::jlimit(
                0.0f, static_cast<float>(binCount - 1),
                srcFrequency / (params.sampleRate * 0.5f) * static_cast<float>(binCount - 1));
            const auto sourceBin = juce::jlimit(0, binCount - 1, static_cast<int>(std::round(sourcePosition)));
            const auto inputPhase = phaseOf(source[static_cast<size_t>(sourceBin)]);
            const auto expectedSource = juce::MathConstants<float>::twoPi
                * static_cast<float>(sourceBin * params.hopSize)
                / static_cast<float>(juce::jmax(1, params.fftSize));
            const auto expectedDestination = juce::MathConstants<float>::twoPi
                * static_cast<float>(bin * params.hopSize)
                / static_cast<float>(juce::jmax(1, params.fftSize));

            if (!memory.phaseInitialised)
                memory.outputPhases[static_cast<size_t>(bin)] = phaseOf(wet);
            else
                memory.outputPhases[static_cast<size_t>(bin)] += expectedDestination
                    + wrapPhase(inputPhase - memory.previousInputPhases[static_cast<size_t>(sourceBin)] - expectedSource);

            wet = withMagnitudePhase(std::abs(wet), memory.outputPhases[static_cast<size_t>(bin)]);
        }

        output[static_cast<size_t>(bin)] = wet;
    }

    output.front() = { output.front().real(), 0.0f };
    output.back() = { output.back().real(), 0.0f };

    for (int bin = 0; bin < binCount; ++bin)
        memory.previousInputPhases[static_cast<size_t>(bin)] = phaseOf(source[static_cast<size_t>(bin)]);
    memory.phaseInitialised = true;
}
} // namespace openfad::flipshift
