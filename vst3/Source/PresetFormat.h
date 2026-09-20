#pragma once
#include "Parameters.h"
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace openfad::flipshift::presets
{
inline constexpr std::array<const char*, 11> ids {
    ParameterIDs::mode, ParameterIDs::shiftHz, ParameterIDs::scale,
    ParameterIDs::pivotHz, ParameterIDs::amount, ParameterIDs::widthQ,
    ParameterIDs::mix, ParameterIDs::outputGainDb, ParameterIDs::quality,
    ParameterIDs::pitchRoot, ParameterIDs::pitchScale
};

inline juce::var capture(juce::AudioProcessorValueTreeState& state, const juce::String& name, bool defaults = false)
{
    auto* values = new juce::DynamicObject();
    for (const auto* id : ids)
    {
        const auto* parameter = state.getParameter(id);
        values->setProperty(id, parameter->convertFrom0to1(defaults ? parameter->getDefaultValue() : parameter->getValue()));
    }
    auto* document = new juce::DynamicObject();
    document->setProperty("format", "openFAD.FlipShift.Preset");
    document->setProperty("version", 1);
    document->setProperty("pluginVersion", "0.1.0");
    document->setProperty("name", name);
    document->setProperty("parameters", juce::var(values));
    return juce::var(document);
}

inline juce::Result validate(const juce::var& document, juce::AudioProcessorValueTreeState& state)
{
    if (!document.isObject() || document["format"].toString() != "openFAD.FlipShift.Preset")
        return juce::Result::fail("preset.format");
    if (!document["version"].isInt() || static_cast<int>(document["version"]) != 1)
        return juce::Result::fail("preset.version");
    if (!document["name"].isString() || document["name"].toString().trim().isEmpty()
        || document["name"].toString().length() > 120 || !document["parameters"].isObject())
        return juce::Result::fail("preset.format");
    const auto& values = document["parameters"];
    for (const auto* id : ids)
    {
        const auto& value = values[id];
        if (!value.isInt() && !value.isInt64() && !value.isDouble())
            return juce::Result::fail("preset.values");
        const auto number = static_cast<double>(value);
        const auto* parameter = state.getParameter(id);
        const auto& range = parameter->getNormalisableRange();
        if (!std::isfinite(number) || number < range.start || number > range.end
            || (dynamic_cast<const juce::AudioParameterChoice*>(parameter) != nullptr && std::floor(number) != number))
            return juce::Result::fail("preset.values");
    }
    return juce::Result::ok();
}

inline bool matches(const juce::var& baseline, juce::AudioProcessorValueTreeState& state)
{
    if (!baseline.isObject()) return false;
    for (const auto* id : ids)
    {
        const auto* parameter = state.getParameter(id);
        const auto current = parameter->convertFrom0to1(parameter->getValue());
        const auto expected = static_cast<float>(baseline["parameters"][id]);
        if (std::abs(current - expected) > juce::jmax(0.00001f, parameter->getNormalisableRange().interval * 0.51f))
            return false;
    }
    return true;
}

inline juce::File directory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("UnpureBloom").getChildFile("FlipShift").getChildFile("Presets");
}

inline bool validId(const juce::String& id)
{
    return id.length() == 32 && id.containsOnly("0123456789abcdef");
}

inline juce::Result read(const juce::File& file, juce::var& document, juce::AudioProcessorValueTreeState& state)
{
    if (!file.existsAsFile() || file.getSize() > 1024 * 1024)
        return juce::Result::fail("preset.read");
    const auto result = juce::JSON::parse(file.loadFileAsString(), document);
    return result.failed() ? juce::Result::fail("preset.format") : validate(document, state);
}

inline juce::Result write(const juce::File& file, const juce::var& document)
{
    if (file.getParentDirectory().createDirectory().failed()) return juce::Result::fail("preset.write");
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(juce::JSON::toString(document), false, false, "\n")
        || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("preset.write");
    return juce::Result::ok();
}
}
