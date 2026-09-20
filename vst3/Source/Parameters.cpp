#include "Parameters.h"

namespace openfad::flipshift
{
juce::StringArray getModeNames()
{
    return {
        "Off", "Bend", "Smear", "Spread", "Harmonics", "Subharm",
        "Gate", "Zero Phase", "Shift", "Mirror", "Peak Push", "Peak x2",
        "Peak /2", "Peak Expand", "Peak Compress", "Harm Sweep",
        "Oct Stack", "Wide Oct Stack", "Comb", "Pitch Blend",
        "Spectral Scale", "Phase Ripple", "Glitch", "Pitch Map"
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

juce::StringArray getPitchRootNames()
{
    return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

juce::StringArray getPitchScaleNames()
{
    return { "Major", "Minor" };
}

ModeControlInfo getModeControlInfo(SpectralMode mode)
{
    ModeControlInfo info;
    info.shiftTooltip = "Additive frequency offset used by this spectral transform.";
    info.scaleTooltip = "Frequency ratio around the selected pivot or detected peak.";
    info.pivotTooltip = "Frequency reference used as a centre, anchor or fundamental.";
    info.amountTooltip = "Depth of the selected spectral transformation.";
    info.widthTooltip = "Bandwidth, radius or selectivity of the transformation.";

    switch (mode)
    {
        case SpectralMode::off:
            info.description = "Leaves the spectral frame unchanged; Freeze and the global Mix path still apply.";
            break;
        case SpectralMode::detune:
            info.usesShift = info.usesPivot = info.usesAmount = true;
            info.description = "Expands or contracts frequency distance around Center by a limited ratio; Amount also blends the transformed spectrum.";
            info.shiftLabel = "BEND";
            info.pivotLabel = "CENTER";
            info.shiftTooltip = "Maximum pivot-centred expansion or contraction; positive values expand and negative values contract.";
            info.pivotTooltip = "Frequency that remains fixed while surrounding frequencies bend.";
            info.amountTooltip = "Increase the bend ratio and blend toward the bent spectrum.";
            info.shiftSuffix = "%";
            break;
        case SpectralMode::smear:
            info.usesAmount = info.usesWidth = true;
            info.description = "Averages neighbouring bin magnitudes, smooths them over time, and retains each bin's current phase.";
            info.amountLabel = "DIFFUSION";
            info.widthLabel = "RADIUS";
            info.amountTooltip = "Blend toward the diffused spectrum and increase its averaging range.";
            info.widthTooltip = "Radius of neighbouring bins included in the magnitude average.";
            break;
        case SpectralMode::spread:
            info.usesShift = info.usesAmount = true;
            info.description = "Translates the left and right spectra by equal Hz offsets in opposite directions; mono input uses one translation direction.";
            info.shiftLabel = "OFFSET";
            info.shiftTooltip = "Maximum opposite Hz translation applied to the two channels.";
            break;
        case SpectralMode::harmonics:
            info.usesPivot = info.usesAmount = info.usesWidth = true;
            info.description = "Retains bands near integer multiples of Fundamental and attenuates the gaps without boosting them.";
            info.pivotLabel = "FUNDAMENTAL";
            info.amountLabel = "REJECTION";
            info.widthLabel = "BAND WIDTH";
            info.pivotTooltip = "Fundamental frequency used to construct the harmonic grid.";
            info.amountTooltip = "Increase attenuation between the retained harmonic bands.";
            info.widthTooltip = "Width of each band retained around the harmonic grid.";
            break;
        case SpectralMode::subharm:
            info.usesAmount = true;
            info.description = "Reads stepped 1x to 4x source frequencies, dividing spectral content downward by integer ratios.";
            info.amountLabel = "DIVISOR";
            info.amountTooltip = "Select the integer frequency divisor and blend it with the original spectrum.";
            break;
        case SpectralMode::gate:
            info.usesAmount = true;
            info.description = "Attenuates bins below a threshold derived from the strongest spectral peak.";
            info.amountLabel = "THRESHOLD";
            info.amountTooltip = "Raises the spectral gate threshold and increases attenuation below it.";
            break;
        case SpectralMode::robotize:
            info.usesAmount = true;
            info.description = "Sets the transformed branch to zero phase, then blends it with the original spectrum.";
            info.amountLabel = "RESET";
            info.amountTooltip = "Blend toward the zero-phase spectrum.";
            break;
        case SpectralMode::shift:
            info.usesShift = info.usesAmount = true;
            info.description = "Adds a fixed Hz offset to every frequency component without preserving harmonic ratios.";
            info.shiftLabel = "OFFSET Hz";
            info.shiftTooltip = "Frequency added to or subtracted from every spectral bin.";
            break;
        case SpectralMode::mirror:
            info.usesScale = info.usesPivot = info.usesAmount = true;
            info.description = "Reflects frequency order around Reflect Axis; larger Map Scale values compress the audible output toward the axis.";
            info.scaleLabel = "MAP SCALE";
            info.pivotLabel = "REFLECT AXIS";
            info.scaleTooltip = "Source-map slope; larger values compress output distances toward Reflect Axis.";
            info.pivotTooltip = "Frequency axis around which the spectrum is reversed.";
            break;
        case SpectralMode::peakFollow:
            info.usesPivot = info.usesAmount = true;
            info.description = "Detects the strongest bin and moves the full spectrum farther from Anchor by the peak-to-anchor offset.";
            info.pivotLabel = "ANCHOR";
            info.amountLabel = "REPEL";
            info.pivotTooltip = "Reference frequency that the detected dominant peak moves away from.";
            info.amountTooltip = "Increase the peak-relative displacement and blend toward the repelled spectrum.";
            break;
        case SpectralMode::peakOctUp:
            info.usesAmount = true;
            info.description = "Keeps the detected peak fixed and doubles every component's linear Hz distance from it.";
            info.amountLabel = "BLEND";
            info.amountTooltip = "Blend toward the fixed 2x peak-relative stretch.";
            break;
        case SpectralMode::peakOctDown:
            info.usesAmount = true;
            info.description = "Keeps the detected peak fixed and halves every component's linear Hz distance from it.";
            info.amountLabel = "BLEND";
            info.amountTooltip = "Blend toward the fixed 2:1 peak-relative compression.";
            break;
        case SpectralMode::peakHmxUp:
            info.usesScale = info.usesAmount = true;
            info.description = "Keeps the strongest peak fixed and expands linear Hz distances by the Span control plus one.";
            info.scaleLabel = "SPAN";
            info.amountLabel = "BLEND";
            info.scaleTooltip = "Expansion control; the applied peak-relative factor is Span plus one.";
            break;
        case SpectralMode::peakHmxDown:
            info.usesScale = info.usesAmount = true;
            info.description = "Keeps the strongest peak fixed and divides linear Hz distances by the Span control plus one.";
            info.scaleLabel = "SPAN";
            info.amountLabel = "BLEND";
            info.scaleTooltip = "Compression control; peak-relative distances are divided by Span plus one.";
            break;
        case SpectralMode::harmSweep:
            info.usesShift = info.usesPivot = info.usesAmount = info.usesWidth = true;
            info.description = "Retains bands near integer multiples of a fundamental transposed by Scan in cents.";
            info.shiftLabel = "SCAN";
            info.shiftSuffix = " ct";
            info.pivotLabel = "FUNDAMENTAL";
            info.widthLabel = "BAND WIDTH";
            info.shiftTooltip = "Harmonic-grid offset in cents; small values provide fine movement.";
            break;
        case SpectralMode::shepard:
            info.usesPivot = info.usesAmount = true;
            info.description = "Sums seven octave-related resamplings through a tight octave-periodic window.";
            info.pivotLabel = "OCTAVE ANCHOR";
            info.amountLabel = "OCTAVE POSITION";
            info.pivotTooltip = "Frequency anchor for the repeating octave window.";
            info.amountTooltip = "Move the seven-layer stack through one octave of resampling position.";
            break;
        case SpectralMode::shepardWide:
            info.usesPivot = info.usesAmount = true;
            info.description = "Sums seven octave-related resamplings through a broad octave-periodic window.";
            info.pivotLabel = "OCTAVE ANCHOR";
            info.amountLabel = "OCTAVE POSITION";
            info.pivotTooltip = "Frequency anchor for the repeating octave window.";
            info.amountTooltip = "Move the seven-layer stack through one octave of resampling position.";
            break;
        case SpectralMode::comb:
            info.usesShift = info.usesPivot = info.usesAmount = true;
            info.description = "Applies a cosine comb mask whose spacing uses the absolute Spacing value plus ten percent of Base.";
            info.shiftLabel = "SPACING";
            info.pivotLabel = "BASE";
            info.amountLabel = "MASK DEPTH";
            info.shiftTooltip = "Unsigned spacing contribution; positive and negative values produce the same spacing.";
            info.pivotTooltip = "Base frequency contributing ten percent of the mask spacing.";
            break;
        case SpectralMode::pitchBlend:
            info.usesScale = info.usesPivot = info.usesAmount = true;
            info.description = "Crossfades complex bins between the original and pivot-scaled spectra without phase tracking.";
            info.scaleLabel = "RATIO";
            info.pivotLabel = "PIVOT";
            info.amountLabel = "CROSSFADE";
            info.amountTooltip = "Crossfade the complex spectrum; intermediate values can produce phase cancellation.";
            break;
        case SpectralMode::pitchShift:
            info.usesScale = info.usesPivot = info.usesAmount = true;
            info.description = "Rescales frequency distance around Pivot and applies phase tracking to remapped bins.";
            info.scaleLabel = "SCALE";
            info.pivotLabel = "PIVOT";
            info.amountLabel = "BLEND";
            info.scaleTooltip = "Frequency-distance scale around Pivot; this is not a whole-spectrum pitch ratio.";
            break;
        case SpectralMode::phaseTwist:
            info.usesPivot = info.usesAmount = true;
            info.description = "Applies sinusoidal frequency-dependent phase rotation; partial blending can also alter magnitude.";
            info.pivotLabel = "RIPPLE SCALE";
            info.amountLabel = "ROTATION";
            info.pivotTooltip = "Frequency scale of the sinusoidal phase ripple; one full cycle spans about 2 pi times this value.";
            info.amountTooltip = "Increase phase rotation and blend toward the rotated spectrum.";
            break;
        case SpectralMode::glitch:
            info.usesShift = info.usesPivot = info.usesAmount = info.usesWidth = true;
            info.description = "Randomly offsets spectral content only inside the band selected by Band Center and Band Q; Density sets event probability and wet depth.";
            info.shiftLabel = "OFFSET";
            info.pivotLabel = "BAND CENTER";
            info.amountLabel = "DENSITY";
            info.widthLabel = "BAND Q";
            info.shiftTooltip = "Signed source-bin offset applied when a selected spectral region glitches.";
            info.pivotTooltip = "Centre frequency of the spectral band eligible for glitch processing.";
            info.amountTooltip = "Probability and wet depth of random glitch events inside the selected band.";
            info.widthTooltip = "Quality factor for the selected band; higher values make the band narrower.";
            break;
        case SpectralMode::pitchMap:
            info.usesAmount = info.usesPitchRoot = info.usesPitchScale = true;
            info.description = "Quantizes spectral energy toward the nearest pitch classes in the selected major or minor key.";
            info.amountLabel = "MAP DEPTH";
            info.amountTooltip = "Blend between the original spectrum and the scale-quantized pitch map.";
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
        ParameterIDs::quality, "Quality", getQualityNames(), 2,
        juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::analyzerView, "Analyzer View", getAnalyzerViewNames(), 2,
        juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::freeze, "Freeze", false,
        juce::AudioParameterBoolAttributes().withAutomatable(false)));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::pitchRoot, "Pitch Map Root", getPitchRootNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::pitchScale, "Pitch Map Scale", getPitchScaleNames(), 0));

    return { params.begin(), params.end() };
}
} // namespace openfad::flipshift
