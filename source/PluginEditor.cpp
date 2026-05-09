#include "PluginEditor.h"

namespace
{
constexpr std::array<const char*, 3> bandNames { "Low Band", "Mid Band", "High Band" };
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    titleLabel.setText ("Multiband Compressor", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    openButton.onClick = [this] { openAudioFile(); };
    playButton.onClick = [this] { processorRef.playLoadedFile(); updateStatusText(); };
    stopButton.onClick = [this] { processorRef.stopLoadedFile(); updateStatusText(); };
    recordButton.onClick = [this] {
        if (processorRef.isRecording())
            processorRef.stopRecording();
        else
            chooseRecordingFile();

        updateStatusText();
    };

    for (auto* button : { &openButton, &playButton, &stopButton, &recordButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff252a30));
        button->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff8b2f2f));
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible (*button);
    }

    statusLabel.setText ("No file loaded", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd6dde3));
    addAndMakeVisible (statusLabel);

    for (size_t i = 0; i < bandGroups.size(); ++i)
    {
        bandGroups[i].setText (bandNames[i]);
        bandGroups[i].setColour (juce::GroupComponent::outlineColourId, juce::Colours::darkgrey);
        bandGroups[i].setColour (juce::GroupComponent::textColourId, juce::Colours::white);
        addAndMakeVisible (bandGroups[i]);
    }

    size_t index = 0;
    addControl (index++, "Low Xover", "freqLow");
    addControl (index++, "Mid Xover", "freqMid");
    addControl (index++, "Dry / Wet", "dryWet");

    addControl (index++, "Threshold", "lowThreshold");
    addControl (index++, "Ratio", "lowRatio");
    addControl (index++, "Attack", "lowAttack");
    addControl (index++, "Release", "lowRelease");
    addControl (index++, "Makeup", "lowMakeup");

    addControl (index++, "Threshold", "midThreshold");
    addControl (index++, "Ratio", "midRatio");
    addControl (index++, "Attack", "midAttack");
    addControl (index++, "Release", "midRelease");
    addControl (index++, "Makeup", "midMakeup");

    addControl (index++, "Threshold", "highThreshold");
    addControl (index++, "Ratio", "highRatio");
    addControl (index++, "Attack", "highAttack");
    addControl (index++, "Release", "highRelease");
    addControl (index++, "Makeup", "highMakeup");

    setSize (1120, 640);
    startTimerHz (8);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::configureSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 22);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfff2b84b));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff33363b));
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void PluginEditor::addControl (size_t index, const juce::String& labelText, const juce::String& parameterId)
{
    auto& control = controls[index];

    configureSlider (control.slider);
    addAndMakeVisible (control.slider);

    control.label.setText (labelText, juce::dontSendNotification);
    control.label.setJustificationType (juce::Justification::centred);
    control.label.setColour (juce::Label::textColourId, juce::Colours::white);
    control.label.attachToComponent (&control.slider, false);
    addAndMakeVisible (control.label);

    attachments.push_back (std::make_unique<SliderAttachment> (processorRef.parameters, parameterId, control.slider));
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111417));

    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient (juce::Colour (0xff1f2a2e), bounds.getTopLeft(),
                                   juce::Colour (0xff101113), bounds.getBottomRight(), false);
    g.setGradientFill (gradient);
    g.fillRect (bounds);
}

void PluginEditor::timerCallback()
{
    updateStatusText();
}

void PluginEditor::openAudioFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choose an audio file to process",
                                                       juce::File{},
                                                       "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg");

    const auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    const juce::Component::SafePointer<PluginEditor> safeThis (this);

    fileChooser->launchAsync (browserFlags, [safeThis] (const juce::FileChooser& chooser) {
        if (safeThis == nullptr)
            return;

        const auto file = chooser.getResult();

        if (file.existsAsFile())
        {
            const auto loaded = safeThis->processorRef.loadAudioFile (file);
            safeThis->statusLabel.setText (loaded ? "Loaded: " + file.getFileName()
                                                  : "Could not load: " + file.getFileName(),
                                           juce::dontSendNotification);
        }
    });
}

void PluginEditor::chooseRecordingFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Save processed output as WAV",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                           .getChildFile ("Multiband Compressor Output.wav"),
                                                       "*.wav");

    const auto browserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
    const juce::Component::SafePointer<PluginEditor> safeThis (this);

    fileChooser->launchAsync (browserFlags, [safeThis] (const juce::FileChooser& chooser) {
        if (safeThis == nullptr)
            return;

        const auto file = chooser.getResult();

        if (file != juce::File{})
        {
            const auto recording = safeThis->processorRef.startRecordingToFile (file);
            safeThis->statusLabel.setText (recording ? "Recording to: " + file.withFileExtension (".wav").getFileName()
                                                     : "Could not start recording",
                                           juce::dontSendNotification);
        }
    });
}

void PluginEditor::updateStatusText()
{
    auto status = processorRef.getLoadedFileName();

    if (processorRef.isPlayingLoadedFile())
        status += " | Playing";

    if (processorRef.isRecording())
        status += " | Recording";

    statusLabel.setText (status, juce::dontSendNotification);
    recordButton.setButtonText (processorRef.isRecording() ? "Stop Rec" : "Record WAV");
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    titleLabel.setBounds (area.removeFromTop (42));
    area.removeFromTop (10);

    auto transportArea = area.removeFromTop (44);
    openButton.setBounds (transportArea.removeFromLeft (130).reduced (4));
    playButton.setBounds (transportArea.removeFromLeft (90).reduced (4));
    stopButton.setBounds (transportArea.removeFromLeft (90).reduced (4));
    recordButton.setBounds (transportArea.removeFromLeft (130).reduced (4));
    statusLabel.setBounds (transportArea.reduced (10, 4));
    area.removeFromTop (8);

    auto crossoverArea = area.removeFromTop (145);
    const auto topControlWidth = crossoverArea.getWidth() / 3;

    for (int i = 0; i < 3; ++i)
    {
        auto slot = crossoverArea.removeFromLeft (topControlWidth).reduced (20, 8);
        controls[static_cast<size_t> (i)].slider.setBounds (slot);
    }

    area.removeFromTop (12);

    const auto groupWidth = area.getWidth() / 3;
    constexpr std::array<std::array<size_t, 5>, 3> bandControlIndices {{
        {{ 3, 4, 5, 6, 7 }},
        {{ 8, 9, 10, 11, 12 }},
        {{ 13, 14, 15, 16, 17 }}
    }};

    for (int band = 0; band < 3; ++band)
    {
        auto groupArea = area.removeFromLeft (groupWidth).reduced (8);
        bandGroups[static_cast<size_t> (band)].setBounds (groupArea);

        auto inner = groupArea.reduced (18, 34);
        const auto rowHeight = inner.getHeight() / 2;

        auto firstRow = inner.removeFromTop (rowHeight);
        auto secondRow = inner;

        for (int i = 0; i < 3; ++i)
        {
            auto slot = firstRow.removeFromLeft (firstRow.getWidth() / (3 - i)).reduced (8, 10);
            controls[bandControlIndices[static_cast<size_t> (band)][static_cast<size_t> (i)]].slider.setBounds (slot);
        }

        for (int i = 3; i < 5; ++i)
        {
            auto slot = secondRow.removeFromLeft (secondRow.getWidth() / (5 - i)).reduced (22, 10);
            controls[bandControlIndices[static_cast<size_t> (band)][static_cast<size_t> (i)]].slider.setBounds (slot);
        }
    }
}
