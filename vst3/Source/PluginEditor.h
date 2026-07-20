#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>

namespace openfad::flipshift
{
class OpenFADFlipShiftAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit OpenFADFlipShiftAudioProcessorEditor(OpenFADFlipShiftAudioProcessor&);
    ~OpenFADFlipShiftAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    using Resource = juce::WebBrowserComponent::Resource;
    using ResourceResult = std::optional<Resource>;

    juce::WebBrowserComponent::Options makeBrowserOptions();
    static ResourceResult getResource(const juce::String& url);
    static juce::String getMimeType(const juce::String& path);

    void handleParameterEvent(const juce::var& payload);
    void endActiveParameterGestures();
    void syncNativeSurfaceState(bool forceFrontendSync = false);
    void timerCallback() override;
    void sendParameterState(bool force);
    void sendAnalyzerFrame();

    OpenFADFlipShiftAudioProcessor& audioProcessor;
    bool frontendReady = false;
    bool lastSurfaceVisible = false;
    int analyzerTick = 0;
    std::array<float, 14> lastParameterValues {};
    std::vector<float> analyzerInputScratch;
    std::vector<float> analyzerOutputScratch;
    std::uint64_t lastAnalyzerSequence = 0;
    juce::Array<juce::RangedAudioParameter*> activeParameterGestures;
    juce::WebBrowserComponent browser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenFADFlipShiftAudioProcessorEditor)
};
} // namespace openfad::flipshift
