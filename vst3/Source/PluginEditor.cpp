#include "PluginEditor.h"
#include <array>

namespace openfad::flipshift
{
namespace
{
const auto background = juce::Colour(0xff101416);
const auto panel = juce::Colour(0xff171d20);
const auto line = juce::Colour(0xff343b3f);
const auto text = juce::Colour(0xffdbe3e3);
const auto muted = juce::Colour(0xff879092);
const auto mint = juce::Colour(0xff54ddb7);
const auto amber = juce::Colour(0xffefad3e);

juce::String formatFrequency(float frequency)
{
    if (frequency >= 1000.0f)
        return juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k";
    return juce::String(juce::roundToInt(frequency));
}
} // namespace

SpectrumDisplay::SpectrumDisplay(OpenFADFlipShiftAudioProcessor& processor)
    : audioProcessor(processor)
{
    startTimerHz(60);
}

SpectrumDisplay::~SpectrumDisplay()
{
    stopTimer();
}

void SpectrumDisplay::timerCallback()
{
    audioProcessor.copyAnalyzerFrames(inputDb, outputDb);

    auto& state = audioProcessor.getState();
    analyzerView = static_cast<int>(*state.getRawParameterValue(ParameterIDs::analyzerView));
    mode = static_cast<SpectralMode>(static_cast<int>(*state.getRawParameterValue(ParameterIDs::mode)));
    pivotHz = *state.getRawParameterValue(ParameterIDs::pivotHz);
    shiftHz = *state.getRawParameterValue(ParameterIDs::shiftHz);
    amount = *state.getRawParameterValue(ParameterIDs::amount);
    sampleRate = static_cast<float>(audioProcessor.getSampleRate() > 0.0 ? audioProcessor.getSampleRate() : 48000.0);

    updateSpectrogramImage();
    repaint();
}

void SpectrumDisplay::resized()
{
    spectrogramImage = {};
}

juce::Rectangle<int> SpectrumDisplay::getSpectrogramArea() const
{
    auto bounds = getLocalBounds();
    if (analyzerView == 0)
        return {};
    if (analyzerView == 1)
        return bounds;
    return bounds.removeFromTop(juce::roundToInt(static_cast<float>(bounds.getHeight()) * 0.70f));
}

juce::Colour SpectrumDisplay::colourForDb(float db) const
{
    const auto energy = juce::jlimit(0.0f, 1.0f, (db + 96.0f) / 84.0f);
    const auto low = juce::Colour(0xff1d1234);
    const auto middle = juce::Colour(0xff2fb875);
    const auto high = juce::Colour(0xffffef7a);
    return energy < 0.58f
        ? low.interpolatedWith(middle, energy / 0.58f)
        : middle.interpolatedWith(high, (energy - 0.58f) / 0.42f);
}

void SpectrumDisplay::updateSpectrogramImage()
{
    const auto area = getSpectrogramArea();
    if (area.isEmpty() || outputDb.size() < 2)
        return;

    if (spectrogramImage.isNull()
        || spectrogramImage.getWidth() != area.getWidth()
        || spectrogramImage.getHeight() != area.getHeight())
    {
        spectrogramImage = juce::Image(juce::Image::RGB, area.getWidth(), area.getHeight(), true);
        juce::Graphics clearGraphics(spectrogramImage);
        clearGraphics.fillAll(background);
    }

    const auto width = spectrogramImage.getWidth();
    const auto height = spectrogramImage.getHeight();
    if (width <= 1 || height <= 1)
        return;

    spectrogramImage.moveImageSection(0, 0, 1, 0, width - 1, height);
    juce::Graphics imageGraphics(spectrogramImage);

    for (int y = 0; y < height; ++y)
    {
        const auto displayPosition = static_cast<float>(height - 1 - y) / static_cast<float>(height - 1);
        const auto linearPosition = (std::pow(10.0f, displayPosition * 2.0f) - 1.0f) / 99.0f;
        const auto binPosition = linearPosition * static_cast<float>(outputDb.size() - 1);
        const auto lowBin = juce::jlimit(0, static_cast<int>(outputDb.size() - 1), static_cast<int>(binPosition));
        const auto highBin = juce::jmin(lowBin + 1, static_cast<int>(outputDb.size() - 1));
        const auto fraction = binPosition - static_cast<float>(lowBin);
        const auto db = juce::jmap(fraction, outputDb[static_cast<size_t>(lowBin)], outputDb[static_cast<size_t>(highBin)]);
        imageGraphics.setColour(colourForDb(db));
        imageGraphics.fillRect(width - 1, y, 1, 1);
    }
}

float SpectrumDisplay::xForBin(int bin, int count, float width) const
{
    if (count <= 1)
        return 0.0f;

    const auto norm = static_cast<float>(bin) / static_cast<float>(count - 1);
    return std::log10(1.0f + norm * 99.0f) / 2.0f * width;
}

float SpectrumDisplay::positionForFrequency(float frequency, float extent) const
{
    const auto nyquist = juce::jmax(1.0f, sampleRate * 0.5f);
    const auto norm = juce::jlimit(0.0f, 1.0f, frequency / nyquist);
    return std::log10(1.0f + norm * 99.0f) / 2.0f * extent;
}

float SpectrumDisplay::yForDb(float db, float height) const
{
    return juce::jmap(juce::jlimit(-96.0f, 0.0f, db), -96.0f, 0.0f, height, 0.0f);
}

void SpectrumDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    juce::Rectangle<float> spectrumArea;
    juce::Rectangle<float> spectrogramArea;

    if (analyzerView == 0)
    {
        spectrumArea = bounds;
    }
    else if (analyzerView == 1)
    {
        spectrogramArea = bounds;
    }
    else
    {
        auto split = bounds;
        spectrogramArea = split.removeFromTop(bounds.getHeight() * 0.70f);
        split.removeFromTop(4.0f);
        spectrumArea = split;
    }

    g.fillAll(background);

    if (!spectrogramArea.isEmpty() && !spectrogramImage.isNull())
    {
        g.drawImage(spectrogramImage, spectrogramArea);
    }

    const std::array<float, 6> frequencyGrid { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f, 20000.0f };

    if (!spectrogramArea.isEmpty())
    {
        g.setColour(line.withAlpha(0.65f));
        for (int i = 0; i <= 8; ++i)
        {
            const auto x = spectrogramArea.getX() + spectrogramArea.getWidth() * static_cast<float>(i) / 8.0f;
            g.drawVerticalLine(static_cast<int>(x), spectrogramArea.getY(), spectrogramArea.getBottom());
        }

        g.setFont(juce::FontOptions(10.0f));
        for (const auto frequency : frequencyGrid)
        {
            if (frequency >= sampleRate * 0.5f)
                continue;
            const auto y = spectrogramArea.getBottom() - positionForFrequency(frequency, spectrogramArea.getHeight());
            g.setColour(line.withAlpha(0.8f));
            g.drawHorizontalLine(static_cast<int>(y), spectrogramArea.getX(), spectrogramArea.getRight());
            g.setColour(text.withAlpha(0.72f));
            g.drawText(formatFrequency(frequency), static_cast<int>(spectrogramArea.getRight() - 42.0f),
                       static_cast<int>(y - 13.0f), 36, 12, juce::Justification::right);
        }

        const auto pivotY = spectrogramArea.getBottom() - positionForFrequency(pivotHz, spectrogramArea.getHeight());
        g.setColour(amber);
        g.drawHorizontalLine(static_cast<int>(pivotY), spectrogramArea.getX(), spectrogramArea.getRight());
        const auto pivotLabel = mode == SpectralMode::mirror ? "MIRROR " : "PIVOT ";
        g.drawText(juce::String(pivotLabel) + formatFrequency(pivotHz), 10, static_cast<int>(pivotY - 17.0f), 120, 15,
                   juce::Justification::left);

        if (mode == SpectralMode::shift || mode == SpectralMode::spread || mode == SpectralMode::detune)
        {
            const auto destination = juce::jlimit(0.0f, sampleRate * 0.5f, pivotHz + shiftHz * amount);
            const auto destinationY = spectrogramArea.getBottom()
                - positionForFrequency(destination, spectrogramArea.getHeight());
            g.setColour(mint.withAlpha(0.78f));
            g.drawHorizontalLine(static_cast<int>(destinationY), spectrogramArea.getX(), spectrogramArea.getRight());
            const auto guideX = spectrogramArea.getRight() - 50.0f;
            g.drawVerticalLine(static_cast<int>(guideX), juce::jmin(pivotY, destinationY), juce::jmax(pivotY, destinationY));
        }
    }

    auto drawCurve = [&](const std::vector<float>& values, juce::Colour colour, float thickness)
    {
        if (values.size() < 2)
            return;

        juce::Path path;
        for (int i = 0; i < static_cast<int>(values.size()); ++i)
        {
            const auto x = spectrumArea.getX() + xForBin(i, static_cast<int>(values.size()), spectrumArea.getWidth());
            const auto y = spectrumArea.getY() + yForDb(values[static_cast<size_t>(i)], spectrumArea.getHeight());
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(thickness));
    };

    if (!spectrumArea.isEmpty())
    {
        g.setColour(panel);
        g.fillRect(spectrumArea);
        g.setColour(line.withAlpha(0.7f));
        for (const auto frequency : frequencyGrid)
        {
            if (frequency >= sampleRate * 0.5f)
                continue;
            const auto x = spectrumArea.getX() + positionForFrequency(frequency, spectrumArea.getWidth());
            g.drawVerticalLine(static_cast<int>(x), spectrumArea.getY(), spectrumArea.getBottom());
        }
        for (const auto db : { -72.0f, -48.0f, -24.0f })
        {
            const auto y = spectrumArea.getY() + yForDb(db, spectrumArea.getHeight());
            g.drawHorizontalLine(static_cast<int>(y), spectrumArea.getX(), spectrumArea.getRight());
        }

        drawCurve(inputDb, muted.withAlpha(0.75f), 1.2f);
        drawCurve(outputDb, mint, 1.8f);

        g.setFont(juce::FontOptions(10.0f));
        g.setColour(muted);
        g.drawText("INPUT", 10, static_cast<int>(spectrumArea.getY() + 6.0f), 48, 14, juce::Justification::left);
        g.setColour(mint);
        g.drawText("OUTPUT", 58, static_cast<int>(spectrumArea.getY() + 6.0f), 58, 14, juce::Justification::left);
    }
}

OpenFADFlipShiftAudioProcessorEditor::OpenFADFlipShiftAudioProcessorEditor(OpenFADFlipShiftAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor), spectrum(processor)
{
    setSize(980, 520);

    modeBox.addItemList(getModeNames(), 1);
    qualityBox.addItemList(getQualityNames(), 1);
    analyzerBox.addItemList(getAnalyzerViewNames(), 1);
    styleCombo(modeBox);
    styleCombo(qualityBox);
    styleCombo(analyzerBox);

    styleSlider(shiftSlider, " Hz");
    styleSlider(scaleSlider, "x");
    styleSlider(pivotSlider, " Hz");
    styleSlider(amountSlider);
    styleSlider(widthSlider);
    styleSlider(mixSlider);
    styleSlider(gainSlider, " dB");

    shiftSlider.setDoubleClickReturnValue(true, 0.0);
    scaleSlider.setDoubleClickReturnValue(true, 1.0);
    pivotSlider.setDoubleClickReturnValue(true, 1000.0);
    amountSlider.setDoubleClickReturnValue(true, 0.5);
    widthSlider.setDoubleClickReturnValue(true, 1.0);
    mixSlider.setDoubleClickReturnValue(true, 0.5);
    gainSlider.setDoubleClickReturnValue(true, 0.0);

    shiftSlider.setMouseDragSensitivity(320);
    scaleSlider.setMouseDragSensitivity(300);
    pivotSlider.setMouseDragSensitivity(360);
    amountSlider.setMouseDragSensitivity(260);
    widthSlider.setMouseDragSensitivity(300);
    mixSlider.setMouseDragSensitivity(260);
    gainSlider.setMouseDragSensitivity(280);

    amountSlider.textFromValueFunction = [](double value) { return juce::String(juce::roundToInt(value * 100.0)) + " %"; };
    amountSlider.valueFromTextFunction = [](const juce::String& value) { return value.getDoubleValue() / 100.0; };
    mixSlider.textFromValueFunction = [](double value) { return juce::String(juce::roundToInt(value * 100.0)) + " %"; };
    mixSlider.valueFromTextFunction = [](const juce::String& value) { return value.getDoubleValue() / 100.0; };

    addLabeled(modeBox, modeLabel, "MODE");
    addLabeled(qualityBox, qualityLabel, "QUALITY");
    addLabeled(analyzerBox, analyzerLabel, "ANALYZER");
    addLabeled(shiftSlider, shiftLabel, "SHIFT");
    addLabeled(scaleSlider, scaleLabel, "SCALE");
    addLabeled(pivotSlider, pivotLabel, "PIVOT");
    addLabeled(amountSlider, amountLabel, "AMOUNT");
    addLabeled(widthSlider, widthLabel, "WIDTH/Q");
    addLabeled(mixSlider, mixLabel, "MIX");
    addLabeled(gainSlider, gainLabel, "GAIN");

    qualityLabel.setTooltip("FFT quality. Low uses 512 samples, Normal 1024 and High 2048; higher quality increases latency and frequency resolution.");
    qualityBox.setTooltip(qualityLabel.getTooltip());
    analyzerLabel.setTooltip("Select the real-time curve view, scrolling time-frequency waterfall, or both together.");
    analyzerBox.setTooltip(analyzerLabel.getTooltip());
    mixLabel.setTooltip("Blends the latency-aligned dry signal with the processed spectral signal.");
    mixSlider.setTooltip(mixLabel.getTooltip());
    gainLabel.setTooltip("Final output level after dry/wet mixing.");
    gainSlider.setTooltip(gainLabel.getTooltip());

    bypassButton.setButtonText("BYPASS");
    freezeButton.setButtonText("FREEZE");
    bypassButton.setColour(juce::ToggleButton::textColourId, text);
    freezeButton.setColour(juce::ToggleButton::textColourId, text);
    bypassButton.setTooltip("Returns the latency-aligned dry signal while keeping host timing stable.");
    freezeButton.setTooltip("Captures the current spectral frame and holds it until Freeze is released.");
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(freezeButton);
    addAndMakeVisible(spectrum);

    hoverHelp.setReadOnly(true);
    hoverHelp.setMultiLine(true, true);
    hoverHelp.setScrollbarsShown(false);
    hoverHelp.setCaretVisible(false);
    hoverHelp.setInterceptsMouseClicks(false, false);
    hoverHelp.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xee111719));
    hoverHelp.setColour(juce::TextEditor::outlineColourId, amber.withAlpha(0.75f));
    hoverHelp.setColour(juce::TextEditor::textColourId, text);
    addAndMakeVisible(hoverHelp);
    hoverHelp.setVisible(false);

    auto& state = audioProcessor.getState();
    modeAttachment = std::make_unique<ComboAttachment>(state, ParameterIDs::mode, modeBox);
    qualityAttachment = std::make_unique<ComboAttachment>(state, ParameterIDs::quality, qualityBox);
    analyzerAttachment = std::make_unique<ComboAttachment>(state, ParameterIDs::analyzerView, analyzerBox);
    shiftAttachment = std::make_unique<Attachment>(state, ParameterIDs::shiftHz, shiftSlider);
    scaleAttachment = std::make_unique<Attachment>(state, ParameterIDs::scale, scaleSlider);
    pivotAttachment = std::make_unique<Attachment>(state, ParameterIDs::pivotHz, pivotSlider);
    amountAttachment = std::make_unique<Attachment>(state, ParameterIDs::amount, amountSlider);
    widthAttachment = std::make_unique<Attachment>(state, ParameterIDs::widthQ, widthSlider);
    mixAttachment = std::make_unique<Attachment>(state, ParameterIDs::mix, mixSlider);
    gainAttachment = std::make_unique<Attachment>(state, ParameterIDs::outputGainDb, gainSlider);
    bypassAttachment = std::make_unique<ButtonAttachment>(state, ParameterIDs::bypass, bypassButton);
    freezeAttachment = std::make_unique<ButtonAttachment>(state, ParameterIDs::freeze, freezeButton);

    modeBox.onChange = [this] { updateModeControls(); };
    updateModeControls();
    startTimerHz(20);
}

OpenFADFlipShiftAudioProcessorEditor::~OpenFADFlipShiftAudioProcessorEditor()
{
    stopTimer();
    modeBox.onChange = nullptr;
    hoverHelp.setVisible(false);
}

void OpenFADFlipShiftAudioProcessorEditor::styleSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::degreesToRadians(225.0f), juce::degreesToRadians(495.0f), true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 17);
    slider.setTextValueSuffix(suffix);
    slider.setScrollWheelEnabled(false);
    slider.setColour(juce::Slider::rotarySliderFillColourId, mint);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff3d464a));
    slider.setColour(juce::Slider::thumbColourId, amber);
    slider.setColour(juce::Slider::textBoxTextColourId, text);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, panel);
}

void OpenFADFlipShiftAudioProcessorEditor::styleCombo(juce::ComboBox& comboBox)
{
    comboBox.setJustificationType(juce::Justification::centredLeft);
    comboBox.setColour(juce::ComboBox::backgroundColourId, panel);
    comboBox.setColour(juce::ComboBox::outlineColourId, line);
    comboBox.setColour(juce::ComboBox::textColourId, text);
    comboBox.setColour(juce::ComboBox::arrowColourId, mint);
}

void OpenFADFlipShiftAudioProcessorEditor::setSliderAvailability(juce::Slider& slider,
                                                                 juce::Label& label,
                                                                 bool enabled,
                                                                 const juce::String& labelText,
                                                                 const juce::String& tooltip)
{
    const auto effectiveTooltip = enabled
        ? tooltip
        : "This parameter is not used by the currently selected Spectral mode.";
    label.setText(labelText, juce::dontSendNotification);
    label.setTooltip(effectiveTooltip);
    slider.setTooltip(effectiveTooltip);
    slider.setEnabled(enabled);
    label.setAlpha(enabled ? 1.0f : 0.38f);
    slider.setAlpha(enabled ? 1.0f : 0.32f);
}

void OpenFADFlipShiftAudioProcessorEditor::timerCallback()
{
    const auto screenPosition = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
    const auto localPosition = getLocalPoint(nullptr, screenPosition);
    if (!getLocalBounds().contains(localPosition))
    {
        hoverHelp.setVisible(false);
        displayedHelpText.clear();
        return;
    }

    for (auto* component = getComponentAt(localPosition); component != nullptr && component != this;
         component = component->getParentComponent())
    {
        if (auto* tooltipClient = dynamic_cast<juce::TooltipClient*>(component))
        {
            const auto tooltip = tooltipClient->getTooltip();
            if (tooltip.isNotEmpty())
            {
                if (tooltip != displayedHelpText)
                {
                    displayedHelpText = tooltip;
                    hoverHelp.setText(tooltip, false);
                }
                hoverHelp.setVisible(true);
                hoverHelp.toFront(false);
                return;
            }
        }
    }
    hoverHelp.setVisible(false);
    displayedHelpText.clear();
}

void OpenFADFlipShiftAudioProcessorEditor::updateModeControls()
{
    auto modeIndex = modeBox.getSelectedItemIndex();
    if (modeIndex < 0)
        modeIndex = static_cast<int>(*audioProcessor.getState().getRawParameterValue(ParameterIDs::mode));

    modeIndex = juce::jlimit(0, static_cast<int>(SpectralMode::count) - 1, modeIndex);
    const auto selectedMode = static_cast<SpectralMode>(modeIndex);
    const auto info = getModeControlInfo(selectedMode);
    const auto modeName = getModeNames()[modeIndex];
    const auto modeTooltip = modeName + ": " + info.description;
    modeLabel.setTooltip(modeTooltip);
    modeBox.setTooltip(modeTooltip);

    shiftSlider.setTextValueSuffix(info.shiftSuffix);
    setSliderAvailability(shiftSlider, shiftLabel, info.usesShift, info.shiftLabel, info.shiftTooltip);
    setSliderAvailability(scaleSlider, scaleLabel, info.usesScale, info.scaleLabel, info.scaleTooltip);
    setSliderAvailability(pivotSlider, pivotLabel, info.usesPivot, info.pivotLabel, info.pivotTooltip);
    setSliderAvailability(amountSlider, amountLabel, info.usesAmount, info.amountLabel, info.amountTooltip);
    setSliderAvailability(widthSlider, widthLabel, info.usesWidth, info.widthLabel, info.widthTooltip);
}

void OpenFADFlipShiftAudioProcessorEditor::addLabeled(juce::Component& control, juce::Label& label, const juce::String& labelText)
{
    label.setText(labelText, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, muted);
    label.setFont(juce::FontOptions(10.0f));
    addAndMakeVisible(label);
    addAndMakeVisible(control);
}

void OpenFADFlipShiftAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(line);
    g.drawRect(getLocalBounds(), 1);
    g.setColour(mint);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("openFAD FLIPSHIFT", 16, 8, 260, 28, juce::Justification::left);
    g.setColour(muted);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("SPECTRAL FLIP / SHIFT / SCALE", getWidth() - 240, 11, 220, 22, juce::Justification::right);
}

void OpenFADFlipShiftAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(34);
    const auto spectrumBounds = area.removeFromTop(300);
    spectrum.setBounds(spectrumBounds);
    hoverHelp.setBounds(spectrumBounds.reduced(10).removeFromTop(52));
    auto controls = area.reduced(0, 8);

    auto placeCombo = [](juce::Rectangle<int> slot, juce::Label& label, juce::Component& control)
    {
        label.setBounds(slot.removeFromTop(13));
        control.setBounds(slot.removeFromTop(26));
    };
    auto placeSlider = [](juce::Rectangle<int> slot, juce::Label& label, juce::Slider& slider)
    {
        label.setBounds(slot.removeFromTop(14));
        slider.setBounds(slot);
    };

    constexpr auto modeWidth = 144;
    constexpr auto rightWidth = 116;
    placeCombo(controls.removeFromLeft(modeWidth).reduced(4, 0), modeLabel, modeBox);

    auto right = controls.removeFromRight(rightWidth).reduced(4, 0);
    const auto sliderWidth = controls.getWidth() / 7;
    placeSlider(controls.removeFromLeft(sliderWidth).reduced(3, 0), shiftLabel, shiftSlider);
    placeSlider(controls.removeFromLeft(sliderWidth).reduced(3, 0), scaleLabel, scaleSlider);
    placeSlider(controls.removeFromLeft(sliderWidth).reduced(3, 0), pivotLabel, pivotSlider);
    placeSlider(controls.removeFromLeft(sliderWidth).reduced(3, 0), amountLabel, amountSlider);
    placeSlider(controls.removeFromLeft(sliderWidth).reduced(3, 0), widthLabel, widthSlider);
    placeSlider(controls.removeFromLeft(sliderWidth).reduced(3, 0), mixLabel, mixSlider);
    placeSlider(controls.reduced(3, 0), gainLabel, gainSlider);

    placeCombo(right.removeFromTop(42), qualityLabel, qualityBox);
    placeCombo(right.removeFromTop(42), analyzerLabel, analyzerBox);
    bypassButton.setBounds(right.removeFromTop(26));
    freezeButton.setBounds(right.removeFromTop(26));
}
} // namespace openfad::flipshift
