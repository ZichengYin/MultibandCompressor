#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Control
    {
        juce::Slider slider;
        juce::Label label;
    };

    void addControl (size_t index, const juce::String& labelText, const juce::String& parameterId);
    static void configureSlider (juce::Slider& slider);

    PluginProcessor& processorRef;

    juce::Label titleLabel;
    std::array<juce::GroupComponent, 3> bandGroups;
    std::array<Control, 18> controls;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
