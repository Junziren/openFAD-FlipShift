#pragma once

#include <JuceHeader.h>
#include <cstddef>
#include <complex>
#include <cstdint>
#include <vector>

namespace openfad::flipshift
{
enum class SpectralMode : int
{
    off = 0,
    detune,
    smear,
    spread,
    harmonics,
    subharm,
    gate,
    robotize,
    shift,
    mirror,
    peakFollow,
    peakOctUp,
    peakOctDown,
    peakHmxUp,
    peakHmxDown,
    harmSweep,
    shepard,
    shepardWide,
    comb,
    pitchBlend,
    pitchShift,
    phaseTwist,
    glitch,
    pitchMap,
    count
};

enum class PitchScale : int
{
    major = 0,
    minor
};

struct SpectralTransformParameters
{
    SpectralMode mode = SpectralMode::shift;
    float shiftHz = 0.0f;
    float scale = 1.0f;
    float pivotHz = 1000.0f;
    float amount = 0.75f;
    float widthQ = 1.0f;
    float sampleRate = 48000.0f;
    int fftSize = 1024;
    int hopSize = 256;
    int channelIndex = 0;
    int numChannels = 2;
    int pitchRoot = 0;
    PitchScale pitchScale = PitchScale::major;
    bool freeze = false;
};

struct SpectralFrameMemory
{
    // prepare() owns allocation; reset() only clears runtime history.
    void prepare(std::size_t binCount, int fftSize, int hopSize);
    void reset() noexcept;
    bool isPreparedFor(std::size_t binCount, int fftSize, int hopSize) const noexcept;

    std::vector<std::complex<float>> frozen;
    std::vector<float> smearMagnitudes;
    std::vector<double> smearPrefixSums;
    std::vector<float> previousInputPhases;
    std::vector<float> outputPhases;
    std::vector<float> phaseAdvances;
    std::vector<float> mappedEnergies;
    std::vector<float> mappedDominantEnergies;
    std::vector<int> mappedSourceBins;
    std::vector<int> pitchMapLowBins;
    std::vector<int> pitchMapHighBins;
    std::vector<float> pitchMapHighWeights;
    bool phaseInitialised = false;
    bool freezeActive = false;
    bool pitchMapCacheValid = false;
    int preparedFftSize = 0;
    int preparedHopSize = 0;
    int cachedPitchRoot = -1;
    PitchScale cachedPitchScale = PitchScale::major;
    float cachedPitchSampleRate = 0.0f;
    std::uint64_t frameIndex = 0;
};

void applySpectralMode(const std::vector<std::complex<float>>& input,
                       std::vector<std::complex<float>>& output,
                       SpectralFrameMemory& memory,
                       const SpectralTransformParameters& params);
} // namespace openfad::flipshift
