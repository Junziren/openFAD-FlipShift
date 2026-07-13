#pragma once

#include <JuceHeader.h>
#include <complex>
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
    count
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
    bool freeze = false;
};

struct SpectralFrameMemory
{
    std::vector<std::complex<float>> frozen;
    std::vector<float> smearMagnitudes;
    std::vector<float> previousInputPhases;
    std::vector<float> outputPhases;
    bool phaseInitialised = false;
    bool freezeActive = false;
};

void applySpectralMode(const std::vector<std::complex<float>>& input,
                       std::vector<std::complex<float>>& output,
                       SpectralFrameMemory& memory,
                       const SpectralTransformParameters& params);
} // namespace openfad::flipshift
