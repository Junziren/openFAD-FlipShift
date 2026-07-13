#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

namespace openfad::flipshift
{
class SpectrumDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumDisplay(OpenFADFlipShiftAudioProcessor& processor);
    ~SpectrumDisplay() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    juce::Rectangle<int> getSpectrogramArea() const;
    void updateSpectrogramImage();
    juce::Colour colourForDb(float db) const;
    float xForBin(int bin, int count, float width) const;
    float positionForFrequency(float frequency, float extent) const;
    float yForDb(float db, float height) const;

    OpenFADFlipShiftAudioProcessor& audioProcessor;
    std::vector<float> inputDb;
    std::vector<float> outputDb;
    juce::Image spectrogramImage;
    int analyzerView = 2;
    SpectralMode mode = SpectralMode::shift;
    float pivotHz = 1000.0f;
    float shiftHz = 0.0f;
    float amount = 0.5f;
    float sampleRate = 48000.0f;
};

class OpenFADFlipShiftAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OpenFADFlipShiftAudioProcessorEditor(OpenFADFlipShiftAudioProcessor&);
    ~OpenFADFlipShiftAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void styleSlider(juce::Slider& slider, const juce::String& suffix = {});
    void styleCombo(juce::ComboBox& comboBox);
    void addLabeled(juce::Component& control, juce::Label& label, const juce::String& text);
    void updateModeControls();
    void timerCallback() override;
    void setSliderAvailability(juce::Slider& slider, juce::Label& label, bool enabled,
                               const juce::String& labelText, const juce::String& tooltip);

    OpenFADFlipShiftAudioProcessor& audioProcessor;
    SpectrumDisplay spectrum;
    juce::TextEditor hoverHelp;
    juce::String displayedHelpText;

    juce::ComboBox modeBox;
    juce::ComboBox qualityBox;
    juce::ComboBox analyzerBox;
    juce::Slider shiftSlider;
    juce::Slider scaleSlider;
    juce::Slider pivotSlider;
    juce::Slider amountSlider;
    juce::Slider widthSlider;
    juce::Slider mixSlider;
    juce::Slider gainSlider;
    juce::ToggleButton bypassButton;
    juce::ToggleButton freezeButton;

    juce::Label modeLabel;
    juce::Label qualityLabel;
    juce::Label analyzerLabel;
    juce::Label shiftLabel;
    juce::Label scaleLabel;
    juce::Label pivotLabel;
    juce::Label amountLabel;
    juce::Label widthLabel;
    juce::Label mixLabel;
    juce::Label gainLabel;

    std::unique_ptr<ComboAttachment> modeAttachment;
    std::unique_ptr<ComboAttachment> qualityAttachment;
    std::unique_ptr<ComboAttachment> analyzerAttachment;
    std::unique_ptr<Attachment> shiftAttachment;
    std::unique_ptr<Attachment> scaleAttachment;
    std::unique_ptr<Attachment> pivotAttachment;
    std::unique_ptr<Attachment> amountAttachment;
    std::unique_ptr<Attachment> widthAttachment;
    std::unique_ptr<Attachment> mixAttachment;
    std::unique_ptr<Attachment> gainAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenFADFlipShiftAudioProcessorEditor)
};
} // namespace openfad::flipshift
