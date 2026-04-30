#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

private:
    enum class Band
    {
        low = 0,
        mid,
        high,
        count
    };

    struct BandParameterPointers
    {
        std::atomic<float>* threshold = nullptr;
        std::atomic<float>* ratio = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* makeupGain = nullptr;
    };

    struct Compressor
    {
        void prepare (double newSampleRate, int numChannels);
        void reset();
        void setParameters (float newThresholdDb,
                            float newRatio,
                            float newAttackMs,
                            float newReleaseMs,
                            float newMakeupGainDb);
        float processSample (int channel, float inputSample);

        double sampleRate = 44100.0;
        float thresholdDb = -24.0f;
        float ratio = 4.0f;
        float attackMs = 10.0f;
        float releaseMs = 100.0f;
        float makeupGainDb = 0.0f;
        float attackCoefficient = 0.0f;
        float releaseCoefficient = 0.0f;
        float makeupGain = 1.0f;
        juce::AudioBuffer<float> envelopes;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void cacheParameterPointers();
    void updateDSPParameters();
    void ensureBufferSize (int numChannels, int numSamples);

    static constexpr auto numBands = static_cast<int> (Band::count);

    double currentSampleRate = 44100.0;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    juce::dsp::LinkwitzRileyFilter<float> lowMidSplitter;
    juce::dsp::LinkwitzRileyFilter<float> midHighSplitter;

    std::array<Compressor, numBands> compressors;

    std::atomic<float>* freqLow = nullptr;
    std::atomic<float>* freqMid = nullptr;
    std::atomic<float>* dryWet = nullptr;
    std::array<BandParameterPointers, numBands> bandParameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
