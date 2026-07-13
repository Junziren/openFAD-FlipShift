#include "Parameters.h"

namespace openfad::flipshift
{
juce::StringArray getModeNames()
{
    return {
        "Off", "Detune +/-", "Smear", "Spread +/-", "Harmonics", "Subharm",
        "Gate", "Robotize", "Shift", "Mirror", "Peak Follow", "Peak Oct +12",
        "Peak Oct -12", "Peak Hmx +12", "Peak Hmx -12", "Harm Sweep",
        "Shepard", "Shepard W", "Comb", "Pitch Blend", "Pitch Shift",
        "Phase Twist"
    };
}

juce::StringArray getQualityNames()
{
    return { "Low", "Normal", "High" };
}

juce::StringArray getAnalyzerViewNames()
{
    return { "Spectrum", "Waterfall", "Both" };
}

ModeControlInfo getModeControlInfo(SpectralMode mode)
{
    ModeControlInfo info;
    info.shiftTooltip = "Additive frequency offset used by this spectral transform.";
    info.scaleTooltip = "Frequency ratio around the selected pivot or detected peak.";
    info.pivotTooltip = "Frequency reference used as a centre, target or fundamental.";
    info.amountTooltip = "Depth of the selected spectral transformation.";
    info.widthTooltip = "Bandwidth, radius or selectivity of the transformation.";

    switch (mode)
    {
        case SpectralMode::off:
            info.description = "Identity spectral path. Use Mix to compare the latency-aligned dry and reconstructed signals.";
            break;
        case SpectralMode::detune:
            info.usesShift = info.usesPivot = info.usesAmount = true;
            info.description = "Compresses or expands frequencies around Pivot for a small detuning effect.";
            info.shiftLabel = "DETUNE";
            info.pivotLabel = "CENTER";
            info.shiftTooltip = "Detune depth. Positive and negative values move bins in opposite directions around Center.";
            info.pivotTooltip = "Frequency that remains fixed while surrounding frequencies are detuned.";
            break;
        case SpectralMode::smear:
            info.usesAmount = info.usesWidth = true;
            info.description = "Averages neighbouring frequency bins to soften partials and transients.";
            info.amountLabel = "SMEAR";
            info.widthLabel = "RADIUS";
            info.amountTooltip = "Blend between the original spectrum and the smeared spectrum.";
            info.widthTooltip = "Number of neighbouring bins included in the spectral average.";
            break;
        case SpectralMode::spread:
            info.usesShift = info.usesAmount = true;
            info.description = "Moves left and right spectra in opposite frequency directions to create stereo width.";
            info.shiftLabel = "SPREAD";
            info.shiftTooltip = "Maximum opposite frequency offset applied to the two channels.";
            break;
        case SpectralMode::harmonics:
            info.usesPivot = info.usesAmount = info.usesWidth = true;
            info.description = "Emphasises bins close to integer multiples of a selected fundamental.";
            info.pivotLabel = "FUNDAMENTAL";
            info.amountLabel = "SELECT";
            info.widthLabel = "HARM WIDTH";
            info.pivotTooltip = "Fundamental frequency used to construct the harmonic grid.";
            info.widthTooltip = "Width of each retained harmonic band.";
            break;
        case SpectralMode::subharm:
            info.usesAmount = true;
            info.description = "Remaps higher source bins downward to generate octave-like subharmonic content.";
            info.amountLabel = "DIVISION";
            info.amountTooltip = "Selects the subharmonic division and blends it with the original spectrum.";
            break;
        case SpectralMode::gate:
            info.usesAmount = true;
            info.description = "Attenuates bins below a threshold derived from the strongest spectral peak.";
            info.amountLabel = "THRESHOLD";
            info.amountTooltip = "Raises the spectral gate threshold and increases attenuation below it.";
            break;
        case SpectralMode::robotize:
            info.usesAmount = true;
            info.description = "Forces spectral phases toward zero for a static metallic, vocoder-like tone.";
            info.amountLabel = "ROBOT";
            break;
        case SpectralMode::shift:
            info.usesShift = info.usesAmount = true;
            info.description = "Adds a fixed Hz offset to every frequency component without preserving harmonic ratios.";
            info.shiftLabel = "SHIFT Hz";
            info.shiftTooltip = "Frequency added to or subtracted from every spectral bin.";
            break;
        case SpectralMode::mirror:
            info.usesScale = info.usesPivot = info.usesAmount = true;
            info.description = "Flips the spectrum around Pivot; Scale stretches the reflected frequency map.";
            info.scaleLabel = "MIRROR SCALE";
            info.pivotLabel = "MIRROR AXIS";
            info.pivotTooltip = "Frequency axis around which the spectrum is reversed.";
            break;
        case SpectralMode::peakFollow:
            info.usesPivot = info.usesAmount = true;
            info.description = "Detects the strongest spectral peak and moves it toward the target Pivot frequency.";
            info.pivotLabel = "TARGET";
            info.amountLabel = "FOLLOW";
            info.pivotTooltip = "Target frequency for the detected dominant peak.";
            break;
        case SpectralMode::peakOctUp:
            info.usesAmount = true;
            info.description = "Remaps frequencies around the detected peak toward one octave above it.";
            info.amountLabel = "OCTAVE MIX";
            break;
        case SpectralMode::peakOctDown:
            info.usesAmount = true;
            info.description = "Remaps frequencies around the detected peak toward one octave below it.";
            info.amountLabel = "OCTAVE MIX";
            break;
        case SpectralMode::peakHmxUp:
            info.usesScale = info.usesAmount = true;
            info.description = "Expands harmonic spacing upward around the detected spectral peak.";
            info.scaleLabel = "HMX RATIO";
            break;
        case SpectralMode::peakHmxDown:
            info.usesScale = info.usesAmount = true;
            info.description = "Compresses harmonic spacing downward around the detected spectral peak.";
            info.scaleLabel = "HMX RATIO";
            break;
        case SpectralMode::harmSweep:
            info.usesShift = info.usesPivot = info.usesAmount = info.usesWidth = true;
            info.description = "Sweeps a harmonic selection grid above or below a fundamental frequency.";
            info.shiftLabel = "SWEEP";
            info.shiftSuffix = " ct";
            info.pivotLabel = "FUNDAMENTAL";
            info.widthLabel = "SWEEP WIDTH";
            info.shiftTooltip = "Harmonic-grid offset in cents; small values provide fine movement.";
            break;
        case SpectralMode::shepard:
            info.usesPivot = info.usesAmount = true;
            info.description = "Layers octave-related spectra under a Shepard envelope centred on Pivot.";
            info.pivotLabel = "CENTER";
            info.amountLabel = "OCTAVE PHASE";
            break;
        case SpectralMode::shepardWide:
            info.usesPivot = info.usesAmount = true;
            info.description = "A wider Shepard octave stack with a broader spectral envelope.";
            info.pivotLabel = "CENTER";
            info.amountLabel = "OCTAVE PHASE";
            break;
        case SpectralMode::comb:
            info.usesShift = info.usesPivot = info.usesAmount = true;
            info.description = "Applies a repeating spectral comb mask with controllable spacing and depth.";
            info.shiftLabel = "SPACING";
            info.pivotLabel = "BASE";
            info.amountLabel = "COMB DEPTH";
            info.shiftTooltip = "Additional spacing between spectral comb teeth.";
            info.pivotTooltip = "Base value used to stabilise comb spacing near zero Shift.";
            break;
        case SpectralMode::pitchBlend:
            info.usesScale = info.usesPivot = info.usesAmount = true;
            info.description = "Blends the original spectrum with a ratio-based pitch-shifted spectrum.";
            info.scaleLabel = "PITCH RATIO";
            info.pivotLabel = "PITCH PIVOT";
            info.amountLabel = "BLEND";
            break;
        case SpectralMode::pitchShift:
            info.usesScale = info.usesPivot = info.usesAmount = true;
            info.description = "Scales frequencies around Pivot using phase-tracked spectral remapping.";
            info.scaleLabel = "PITCH RATIO";
            info.pivotLabel = "PITCH PIVOT";
            break;
        case SpectralMode::phaseTwist:
            info.usesPivot = info.usesAmount = true;
            info.description = "Adds a frequency-dependent sinusoidal phase rotation without moving magnitudes.";
            info.pivotLabel = "PHASE PERIOD";
            info.amountLabel = "TWIST";
            info.pivotTooltip = "Controls how quickly phase rotation repeats across frequency.";
            break;
        case SpectralMode::count:
            break;
    }

    return info;
}

namespace
{
juce::NormalisableRange<float> rangeWithCentre(float start, float end, float interval, float centre)
{
    juce::NormalisableRange<float> range { start, end, interval };
    range.setSkewForCentre(centre);
    return range;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::mode, "Mode", getModeNames(), 8));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::shiftHz, "Shift Hz",
        juce::NormalisableRange<float> { -5000.0f, 5000.0f, 0.01f, 0.45f, true },
        0.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::scale, "Scale",
        rangeWithCentre(0.25f, 4.0f, 0.001f, 1.0f),
        1.0f, juce::AudioParameterFloatAttributes().withLabel("x")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::pivotHz, "Pivot Hz",
        rangeWithCentre(20.0f, 20000.0f, 0.01f, 1000.0f),
        1000.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::amount, "Amount",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0001f },
        0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::widthQ, "Width/Q",
        rangeWithCentre(0.05f, 8.0f, 0.001f, 1.0f),
        1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0001f },
        0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::outputGainDb, "Output Gain",
        rangeWithCentre(-24.0f, 12.0f, 0.01f, 0.0f),
        0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::quality, "Quality", getQualityNames(), 1,
        juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::analyzerView, "Analyzer View", getAnalyzerViewNames(), 2,
        juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::freeze, "Freeze", false,
        juce::AudioParameterBoolAttributes().withAutomatable(false)));

    return { params.begin(), params.end() };
}
} // namespace openfad::flipshift
