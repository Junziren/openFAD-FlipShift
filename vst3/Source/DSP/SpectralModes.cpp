#include "DSP/SpectralModes.h"

#include <cstdint>
#include <limits>

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
    if (spectrum.size() <= 1)
        return 0;

    auto peakBin = 1;
    auto peakMagnitude = 0.0f;

    for (int i = 1; i < static_cast<int>(spectrum.size()); ++i)
    {
        const auto magnitude = std::abs(spectrum[static_cast<size_t>(i)]);
        if (magnitude > peakMagnitude)
        {
            peakMagnitude = magnitude;
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

float wrapPhase(float phase) noexcept
{
    if (!std::isfinite(phase))
        return 0.0f;

    const auto twoPi = juce::MathConstants<float>::twoPi;
    const auto pi = juce::MathConstants<float>::pi;
    phase -= twoPi * std::floor((phase + pi) / twoPi);
    if (phase <= -pi)
        phase += twoPi;
    return phase;
}

std::uint64_t mixHash(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

float unitHash(std::uint64_t value) noexcept
{
    constexpr auto inverse24BitRange = 1.0f / 16777216.0f;
    return static_cast<float>((mixHash(value) >> 40U) & 0x00ffffffULL) * inverse24BitRange;
}

int positiveModulo12(int value) noexcept
{
    const auto remainder = value % 12;
    return remainder < 0 ? remainder + 12 : remainder;
}

bool isScaleNote(int midiNote, int root, PitchScale scale) noexcept
{
    constexpr bool majorNotes[12] {
        true, false, true, false, true, true,
        false, true, false, true, false, true
    };
    constexpr bool minorNotes[12] {
        true, false, true, true, false, true,
        false, true, true, false, true, false
    };

    const auto degree = positiveModulo12(midiNote - root);
    return scale == PitchScale::minor ? minorNotes[degree] : majorNotes[degree];
}

float quantiseFrequencyToScale(float frequency, int root, PitchScale scale) noexcept
{
    if (!(frequency > 0.0f) || !std::isfinite(frequency))
        return 0.0f;

    const auto midi = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    const auto centreNote = static_cast<int>(std::floor(midi));
    auto bestNote = centreNote;
    auto bestDistance = std::numeric_limits<float>::max();

    // Diatonic scales never leave more than two semitones between neighbours.
    // The wider bounded search also handles exact octave/root boundaries.
    for (int candidate = centreNote - 6; candidate <= centreNote + 6; ++candidate)
    {
        if (!isScaleNote(candidate, root, scale))
            continue;

        const auto distance = std::abs(midi - static_cast<float>(candidate));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestNote = candidate;
        }
    }

    return 440.0f * std::pow(2.0f, (static_cast<float>(bestNote) - 69.0f) / 12.0f);
}

void addMappedEnergy(SpectralFrameMemory& memory,
                     int destinationBin,
                     int sourceBin,
                     float energy) noexcept
{
    if (energy <= 0.0f)
        return;

    const auto destination = static_cast<size_t>(destinationBin);
    memory.mappedEnergies[destination] += energy;
    if (energy > memory.mappedDominantEnergies[destination])
    {
        memory.mappedDominantEnergies[destination] = energy;
        memory.mappedSourceBins[destination] = sourceBin;
    }
}

void updatePitchMapCache(SpectralFrameMemory& memory,
                         int binCount,
                         float sampleRate,
                         int root,
                         PitchScale scale) noexcept
{
    if (memory.pitchMapCacheValid
        && memory.cachedPitchRoot == root
        && memory.cachedPitchScale == scale
        && memory.cachedPitchSampleRate == sampleRate)
        return;

    const auto nyquist = sampleRate * 0.5f;
    for (int sourceBin = 0; sourceBin < binCount; ++sourceBin)
    {
        const auto frequency = binHz(sourceBin, binCount, sampleRate);
        const auto mappedFrequency = juce::jlimit(
            0.0f, nyquist, quantiseFrequencyToScale(frequency, root, scale));
        const auto destinationPosition = nyquist > 0.0f
            ? mappedFrequency / nyquist * static_cast<float>(binCount - 1)
            : 0.0f;
        const auto lowBin = juce::jlimit(0, binCount - 1, static_cast<int>(std::floor(destinationPosition)));
        const auto index = static_cast<size_t>(sourceBin);
        memory.pitchMapLowBins[index] = lowBin;
        memory.pitchMapHighBins[index] = juce::jlimit(0, binCount - 1, lowBin + 1);
        memory.pitchMapHighWeights[index] = destinationPosition - static_cast<float>(lowBin);
    }

    memory.cachedPitchRoot = root;
    memory.cachedPitchScale = scale;
    memory.cachedPitchSampleRate = sampleRate;
    memory.phaseInitialised = false;
    memory.pitchMapCacheValid = true;
}

void applyPitchMap(const std::vector<Complex>& source,
                   std::vector<Complex>& output,
                   SpectralFrameMemory& memory,
                   const SpectralTransformParameters& params,
                   float amount) noexcept
{
    const auto binCount = static_cast<int>(source.size());
    const auto root = juce::jlimit(0, 11, params.pitchRoot);
    const auto scale = static_cast<PitchScale>(juce::jlimit(
        0, 1, static_cast<int>(params.pitchScale)));

    updatePitchMapCache(memory, binCount, params.sampleRate, root, scale);

    std::fill(memory.mappedEnergies.begin(), memory.mappedEnergies.end(), 0.0f);
    std::fill(memory.mappedDominantEnergies.begin(), memory.mappedDominantEnergies.end(), 0.0f);
    std::fill(memory.mappedSourceBins.begin(), memory.mappedSourceBins.end(), -1);

    memory.mappedEnergies.front() = std::norm(source.front());
    memory.mappedDominantEnergies.front() = memory.mappedEnergies.front();
    memory.mappedSourceBins.front() = 0;

    for (int sourceBin = 1; sourceBin < binCount - 1; ++sourceBin)
    {
        const auto sourceIndex = static_cast<size_t>(sourceBin);
        const auto lowBin = memory.pitchMapLowBins[sourceIndex];
        const auto highBin = memory.pitchMapHighBins[sourceIndex];
        const auto highWeight = memory.pitchMapHighWeights[sourceIndex];
        const auto energy = std::norm(source[static_cast<size_t>(sourceBin)]);

        addMappedEnergy(memory, lowBin, sourceBin, energy * (1.0f - highWeight));
        addMappedEnergy(memory, highBin, sourceBin, energy * highWeight);
    }

    const auto nyquistEnergy = std::norm(source.back());
    memory.mappedEnergies.back() += nyquistEnergy;
    if (nyquistEnergy > memory.mappedDominantEnergies.back())
    {
        memory.mappedDominantEnergies.back() = nyquistEnergy;
        memory.mappedSourceBins.back() = binCount - 1;
    }

    for (int destinationBin = 0; destinationBin < binCount; ++destinationBin)
    {
        const auto destination = static_cast<size_t>(destinationBin);
        const auto sourceBin = memory.mappedSourceBins[destination];
        auto wet = Complex {};

        if (sourceBin >= 0 && memory.mappedEnergies[destination] > 0.0f)
        {
            const auto sourceIndex = static_cast<size_t>(sourceBin);
            const auto inputPhase = phaseOf(source[sourceIndex]);

            if (!memory.phaseInitialised)
                memory.outputPhases[destination] = wrapPhase(inputPhase);
            else
                memory.outputPhases[destination] = wrapPhase(
                    memory.outputPhases[destination]
                    + memory.phaseAdvances[destination]
                    + wrapPhase(inputPhase
                        - memory.previousInputPhases[sourceIndex]
                        - memory.phaseAdvances[sourceIndex]));

            wet = withMagnitudePhase(
                std::sqrt(memory.mappedEnergies[destination]),
                memory.outputPhases[destination]);
        }

        output[destination] = source[destination] * (1.0f - amount) + wet * amount;
    }

    output.front() = { output.front().real(), 0.0f };
    output.back() = { output.back().real(), 0.0f };

    for (int bin = 0; bin < binCount; ++bin)
        memory.previousInputPhases[static_cast<size_t>(bin)] = phaseOf(source[static_cast<size_t>(bin)]);
    memory.phaseInitialised = true;
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
        case SpectralMode::glitch:
        case SpectralMode::pitchMap:
            return true;
        default:
            return false;
    }
}

bool usesPeakFrequency(SpectralMode mode) noexcept
{
    switch (mode)
    {
        case SpectralMode::peakFollow:
        case SpectralMode::peakOctUp:
        case SpectralMode::peakOctDown:
        case SpectralMode::peakHmxUp:
        case SpectralMode::peakHmxDown:
            return true;
        default:
            return false;
    }
}
} // namespace

void SpectralFrameMemory::prepare(std::size_t binCount, int fftSize, int hopSize)
{
    preparedFftSize = juce::jmax(1, fftSize);
    preparedHopSize = juce::jmax(0, hopSize);

    frozen.resize(binCount);
    smearMagnitudes.resize(binCount);
    smearPrefixSums.resize(binCount + 1);
    previousInputPhases.resize(binCount);
    outputPhases.resize(binCount);
    phaseAdvances.resize(binCount);
    mappedEnergies.resize(binCount);
    mappedDominantEnergies.resize(binCount);
    mappedSourceBins.resize(binCount);
    pitchMapLowBins.resize(binCount);
    pitchMapHighBins.resize(binCount);
    pitchMapHighWeights.resize(binCount);

    for (std::size_t bin = 0; bin < binCount; ++bin)
    {
        const auto phaseAdvance = juce::MathConstants<float>::twoPi
            * static_cast<float>(static_cast<int>(bin) * preparedHopSize)
            / static_cast<float>(preparedFftSize);
        phaseAdvances[bin] = wrapPhase(phaseAdvance);
    }

    reset();
}

void SpectralFrameMemory::reset() noexcept
{
    std::fill(frozen.begin(), frozen.end(), Complex {});
    std::fill(smearMagnitudes.begin(), smearMagnitudes.end(), 0.0f);
    std::fill(smearPrefixSums.begin(), smearPrefixSums.end(), 0.0);
    std::fill(previousInputPhases.begin(), previousInputPhases.end(), 0.0f);
    std::fill(outputPhases.begin(), outputPhases.end(), 0.0f);
    std::fill(mappedEnergies.begin(), mappedEnergies.end(), 0.0f);
    std::fill(mappedDominantEnergies.begin(), mappedDominantEnergies.end(), 0.0f);
    std::fill(mappedSourceBins.begin(), mappedSourceBins.end(), -1);
    std::fill(pitchMapLowBins.begin(), pitchMapLowBins.end(), 0);
    std::fill(pitchMapHighBins.begin(), pitchMapHighBins.end(), 0);
    std::fill(pitchMapHighWeights.begin(), pitchMapHighWeights.end(), 0.0f);
    phaseInitialised = false;
    freezeActive = false;
    pitchMapCacheValid = false;
    cachedPitchRoot = -1;
    cachedPitchScale = PitchScale::major;
    cachedPitchSampleRate = 0.0f;
    frameIndex = 0;
}

bool SpectralFrameMemory::isPreparedFor(std::size_t binCount, int fftSize, int hopSize) const noexcept
{
    return preparedFftSize == juce::jmax(1, fftSize)
        && preparedHopSize == juce::jmax(0, hopSize)
        && frozen.size() == binCount
        && smearMagnitudes.size() == binCount
        && smearPrefixSums.size() == binCount + 1
        && previousInputPhases.size() == binCount
        && outputPhases.size() == binCount
        && phaseAdvances.size() == binCount
        && mappedEnergies.size() == binCount
        && mappedDominantEnergies.size() == binCount
        && mappedSourceBins.size() == binCount
        && pitchMapLowBins.size() == binCount
        && pitchMapHighBins.size() == binCount
        && pitchMapHighWeights.size() == binCount;
}

void applySpectralMode(const std::vector<Complex>& input,
                       std::vector<Complex>& output,
                       SpectralFrameMemory& memory,
                       const SpectralTransformParameters& params)
{
    const auto binCount = static_cast<int>(input.size());

    if (binCount == 0)
    {
        output.clear();
        return;
    }

    if (output.size() != input.size() || !memory.isPreparedFor(input.size(), params.fftSize, params.hopSize))
    {
        jassertfalse;
        if (output.size() == input.size())
            std::copy(input.begin(), input.end(), output.begin());
        return;
    }

    const auto amount = clamp01(params.amount);
    const auto scale = juce::jlimit(0.05f, 8.0f, params.scale);
    const auto pivot = juce::jlimit(20.0f, params.sampleRate * 0.5f, params.pivotHz);
    const auto q = juce::jlimit(0.05f, 8.0f, params.widthQ);
    const auto stereoSign = params.numChannels > 1 && params.channelIndex == 1 ? -1.0f : 1.0f;
    const auto mode = params.mode;
    const auto remapMode = isFrequencyRemapMode(mode);

    if (params.freeze && !memory.freezeActive)
        std::copy(input.begin(), input.end(), memory.frozen.begin());

    memory.freezeActive = params.freeze;

    const auto& source = params.freeze ? memory.frozen : input;
    if (amount <= 0.0f)
    {
        std::copy(source.begin(), source.end(), output.begin());
        memory.phaseInitialised = false;
        ++memory.frameIndex;
        return;
    }

    const auto needsPeakFrequency = usesPeakFrequency(mode);
    const auto needsPeak = needsPeakFrequency || mode == SpectralMode::gate;
    const auto peakBin = needsPeak ? findPeakBin(source) : 0;
    const auto peakHz = needsPeakFrequency ? binHz(peakBin, binCount, params.sampleRate) : 0.0f;
    const auto gatePeakMagnitude = mode == SpectralMode::gate
        ? std::abs(source[static_cast<size_t>(peakBin)]) : 0.0f;

    if (mode == SpectralMode::pitchMap)
    {
        applyPitchMap(source, output, memory, params, amount);
        ++memory.frameIndex;
        return;
    }

    const auto nyquist = params.sampleRate * 0.5f;
    const auto frequencyResolution = nyquist / static_cast<float>(juce::jmax(1, binCount - 1));
    const auto glitchBandwidth = juce::jlimit(
        frequencyResolution * 4.0f, nyquist, pivot / q);
    const auto glitchLow = juce::jmax(frequencyResolution, pivot - glitchBandwidth * 0.5f);
    const auto glitchHigh = juce::jmin(nyquist, pivot + glitchBandwidth * 0.5f);
    const auto glitchChunkWidth = juce::jmax(
        frequencyResolution * 2.0f, glitchBandwidth / 12.0f);
    const auto framesPerSecond = params.sampleRate / static_cast<float>(juce::jmax(1, params.hopSize));
    const auto glitchHoldFrames = juce::jlimit(
        1, 64, static_cast<int>(std::round(framesPerSecond * juce::jmap(amount, 0.11f, 0.025f))));
    const auto glitchEpoch = memory.frameIndex / static_cast<std::uint64_t>(glitchHoldFrames);

    auto smearRadius = 0;
    if (mode == SpectralMode::smear)
    {
        // One magnitude pass replaces a neighbourhood scan for every output bin.
        smearRadius = juce::jlimit(
            1, juce::jmax(1, binCount / 4), static_cast<int>(8.0f + amount * q * 44.0f));
        memory.smearPrefixSums[0] = 0.0;
        for (int bin = 0; bin < binCount; ++bin)
            memory.smearPrefixSums[static_cast<size_t>(bin + 1)] =
                memory.smearPrefixSums[static_cast<size_t>(bin)]
                + std::abs(source[static_cast<size_t>(bin)]);
    }

    for (int bin = 0; bin < binCount; ++bin)
    {
        const auto frequency = binHz(bin, binCount, params.sampleRate);
        auto srcFrequency = frequency;
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
                const auto firstBin = juce::jmax(0, bin - smearRadius);
                const auto lastBin = juce::jmin(binCount - 1, bin + smearRadius);
                const auto magnitudeSum = memory.smearPrefixSums[static_cast<size_t>(lastBin + 1)]
                    - memory.smearPrefixSums[static_cast<size_t>(firstBin)];
                const auto mag = static_cast<float>(
                    magnitudeSum / static_cast<double>(lastBin - firstBin + 1));
                const auto phase = phaseOf(source[static_cast<size_t>(bin)]);
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
                const auto threshold = gatePeakMagnitude * amount * 0.8f;
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

            case SpectralMode::glitch:
            {
                if (frequency < glitchLow || frequency > glitchHigh)
                    break;

                const auto chunk = static_cast<std::uint64_t>(
                    juce::jmax(0, static_cast<int>((frequency - glitchLow) / glitchChunkWidth)));
                const auto seed = glitchEpoch * 0xd1342543de82ef95ULL
                    + chunk * 0x9e3779b97f4a7c15ULL;
                const auto trigger = unitHash(seed);
                if (trigger > 0.12f + amount * 0.88f)
                    break;

                const auto randomOffset = unitHash(seed ^ 0xa24baed4963ee407ULL) * 2.0f - 1.0f;
                const auto displacement = params.shiftHz
                    + randomOffset * glitchBandwidth * (0.12f + amount * 0.28f);
                srcFrequency = frequency - displacement;
                wet = sampleAtFrequency(source, srcFrequency, params.sampleRate);
                break;
            }

            case SpectralMode::pitchMap:
            case SpectralMode::count:
                break;
        }

        if (mode != SpectralMode::pitchBlend && mode != SpectralMode::off)
            wet = source[static_cast<size_t>(bin)] * (1.0f - amount) + wet * amount;

        const auto remappedMagnitude = remapMode ? std::abs(wet) : 0.0f;
        if (remappedMagnitude > 0.0f)
        {
            const auto sourcePosition = juce::jlimit(
                0.0f, static_cast<float>(binCount - 1),
                srcFrequency / (params.sampleRate * 0.5f) * static_cast<float>(binCount - 1));
            const auto sourceBin = juce::jlimit(0, binCount - 1, static_cast<int>(std::round(sourcePosition)));
            const auto inputPhase = phaseOf(source[static_cast<size_t>(sourceBin)]);
            const auto expectedSource = memory.phaseAdvances[static_cast<size_t>(sourceBin)];
            const auto expectedDestination = memory.phaseAdvances[static_cast<size_t>(bin)];

            if (!memory.phaseInitialised)
                memory.outputPhases[static_cast<size_t>(bin)] = wrapPhase(phaseOf(wet));
            else
                memory.outputPhases[static_cast<size_t>(bin)] = wrapPhase(
                    memory.outputPhases[static_cast<size_t>(bin)] + expectedDestination
                    + wrapPhase(inputPhase - memory.previousInputPhases[static_cast<size_t>(sourceBin)] - expectedSource));

            wet = withMagnitudePhase(remappedMagnitude, memory.outputPhases[static_cast<size_t>(bin)]);
        }

        output[static_cast<size_t>(bin)] = wet;
    }

    output.front() = { output.front().real(), 0.0f };
    output.back() = { output.back().real(), 0.0f };

    if (remapMode)
    {
        for (int bin = 0; bin < binCount; ++bin)
            memory.previousInputPhases[static_cast<size_t>(bin)] = phaseOf(source[static_cast<size_t>(bin)]);
        memory.phaseInitialised = true;
    }
    else
    {
        memory.phaseInitialised = false;
    }

    ++memory.frameIndex;
}
} // namespace openfad::flipshift
