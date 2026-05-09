#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
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
    void timerCallback() override;
    void openAudioFile();
    void chooseRecordingFile();
    void updateStatusText();

    PluginProcessor& processorRef;

    juce::Label titleLabel;
    juce::TextButton openButton { "Open Audio" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton recordButton { "Record WAV" };
    juce::Label statusLabel;
    std::array<juce::GroupComponent, 3> bandGroups;
    std::array<Control, 18> controls;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
