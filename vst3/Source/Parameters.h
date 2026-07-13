#pragma once

#include "DSP/SpectralModes.h"
#include <JuceHeader.h>

namespace openfad::flipshift
{
namespace ParameterIDs
{
static constexpr auto mode = "mode";
static constexpr auto shiftHz = "shiftHz";
static constexpr auto scale = "scale";
static constexpr auto pivotHz = "pivotHz";
static constexpr auto amount = "amount";
static constexpr auto widthQ = "widthQ";
static constexpr auto mix = "mix";
static constexpr auto outputGainDb = "outputGainDb";
static constexpr auto quality = "quality";
static constexpr auto analyzerView = "analyzerView";
static constexpr auto bypass = "bypass";
static constexpr auto freeze = "freeze";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

juce::StringArray getModeNames();
juce::StringArray getQualityNames();
juce::StringArray getAnalyzerViewNames();

struct ModeControlInfo
{
    bool usesShift = false;
    bool usesScale = false;
    bool usesPivot = false;
    bool usesAmount = false;
    bool usesWidth = false;
    juce::String description;
    juce::String shiftLabel = "SHIFT";
    juce::String scaleLabel = "SCALE";
    juce::String pivotLabel = "PIVOT";
    juce::String amountLabel = "AMOUNT";
    juce::String widthLabel = "WIDTH/Q";
    juce::String shiftTooltip;
    juce::String scaleTooltip;
    juce::String pivotTooltip;
    juce::String amountTooltip;
    juce::String widthTooltip;
    juce::String shiftSuffix = " Hz";
};

ModeControlInfo getModeControlInfo(SpectralMode mode);
} // namespace openfad::flipshift
