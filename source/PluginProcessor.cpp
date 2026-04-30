#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace
{
constexpr auto parameterTreeId = "Parameters";

constexpr auto freqLowId = "freqLow";
constexpr auto freqMidId = "freqMid";
constexpr auto dryWetId = "dryWet";

constexpr std::array<const char*, 3> bandPrefixes { "low", "mid", "high" };
constexpr std::array<const char*, 3> bandNames { "Low", "Mid", "High" };

juce::String makeBandParameterId (const char* prefix, const char* parameter)
{
    return juce::String (prefix) + parameter;
}

float decibelsToGain (float decibels)
{
    return juce::Decibels::decibelsToGain (decibels);
}

float calculateTimeCoefficient (float timeMs, double sampleRate)
{
    const auto timeSeconds = juce::jmax (timeMs * 0.001f, 0.000001f);
    return std::exp (-1.0f / (static_cast<float> (sampleRate) * timeSeconds));
}
} // namespace

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                    #if ! JucePlugin_IsMidiEffect
                     #if ! JucePlugin_IsSynth
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     #endif
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                    #endif
                      ),
      parameters (*this, nullptr, parameterTreeId, createParameterLayout())
{
    cacheParameterPointers();
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto frequencyRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 0.01f, 0.35f);
    auto thresholdRange = juce::NormalisableRange<float> (-60.0f, 0.0f, 0.01f);
    auto ratioRange = juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.35f);
    auto attackRange = juce::NormalisableRange<float> (1.0f, 500.0f, 0.01f, 0.35f);
    auto releaseRange = juce::NormalisableRange<float> (5.0f, 5000.0f, 0.01f, 0.35f);
    auto gainRange = juce::NormalisableRange<float> (-20.0f, 20.0f, 0.01f);
    auto mixRange = juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { freqLowId, 1 }, "Low/Mid Crossover", frequencyRange, 200.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { freqMidId, 1 }, "Mid/High Crossover", frequencyRange, 2000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    for (size_t band = 0; band < bandPrefixes.size(); ++band)
    {
        const auto prefix = bandPrefixes[band];
        const auto name = juce::String (bandNames[band]);

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeBandParameterId (prefix, "Threshold"), 1 },
            name + " Threshold", thresholdRange, -24.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeBandParameterId (prefix, "Ratio"), 1 },
            name + " Ratio", ratioRange, 4.0f,
            juce::AudioParameterFloatAttributes().withLabel (":1")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeBandParameterId (prefix, "Attack"), 1 },
            name + " Attack", attackRange, 10.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeBandParameterId (prefix, "Release"), 1 },
            name + " Release", releaseRange, 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeBandParameterId (prefix, "Makeup"), 1 },
            name + " Makeup Gain", gainRange, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { dryWetId, 1 }, "Dry/Wet Mix", mixRange, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    return { params.begin(), params.end() };
}

void PluginProcessor::cacheParameterPointers()
{
    freqLow = parameters.getRawParameterValue (freqLowId);
    freqMid = parameters.getRawParameterValue (freqMidId);
    dryWet = parameters.getRawParameterValue (dryWetId);

    for (size_t band = 0; band < bandPrefixes.size(); ++band)
    {
        const auto prefix = bandPrefixes[band];
        auto& pointers = bandParameters[band];

        pointers.threshold = parameters.getRawParameterValue (makeBandParameterId (prefix, "Threshold"));
        pointers.ratio = parameters.getRawParameterValue (makeBandParameterId (prefix, "Ratio"));
        pointers.attack = parameters.getRawParameterValue (makeBandParameterId (prefix, "Attack"));
        pointers.release = parameters.getRawParameterValue (makeBandParameterId (prefix, "Release"));
        pointers.makeupGain = parameters.getRawParameterValue (makeBandParameterId (prefix, "Makeup"));
    }
}

void PluginProcessor::Compressor::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    envelopes.setSize (numChannels, 1);
    envelopes.clear();
    setParameters (thresholdDb, ratio, attackMs, releaseMs, makeupGainDb);
}

void PluginProcessor::Compressor::reset()
{
    envelopes.clear();
}

void PluginProcessor::Compressor::setParameters (float newThresholdDb,
                                                 float newRatio,
                                                 float newAttackMs,
                                                 float newReleaseMs,
                                                 float newMakeupGainDb)
{
    thresholdDb = juce::jlimit (-60.0f, 0.0f, newThresholdDb);
    ratio = juce::jlimit (1.0f, 20.0f, newRatio);
    attackMs = juce::jlimit (1.0f, 500.0f, newAttackMs);
    releaseMs = juce::jlimit (5.0f, 5000.0f, newReleaseMs);
    makeupGainDb = juce::jlimit (-20.0f, 20.0f, newMakeupGainDb);

    attackCoefficient = calculateTimeCoefficient (attackMs, sampleRate);
    releaseCoefficient = calculateTimeCoefficient (releaseMs, sampleRate);
    makeupGain = decibelsToGain (makeupGainDb);
}

float PluginProcessor::Compressor::processSample (int channel, float inputSample)
{
    if (channel >= envelopes.getNumChannels())
        return inputSample;

    auto* envelopeData = envelopes.getWritePointer (channel);
    auto envelope = envelopeData[0];
    const auto detector = std::abs (inputSample);
    const auto coefficient = detector > envelope ? attackCoefficient : releaseCoefficient;
    envelope = coefficient * envelope + (1.0f - coefficient) * detector;
    envelopeData[0] = envelope;

    const auto levelDb = juce::Decibels::gainToDecibels (envelope, -120.0f);
    auto gainReductionDb = 0.0f;

    if (levelDb > thresholdDb)
    {
        const auto inputAboveThreshold = levelDb - thresholdDb;
        const auto outputAboveThreshold = inputAboveThreshold / ratio;
        gainReductionDb = outputAboveThreshold - inputAboveThreshold;
    }

    return inputSample * decibelsToGain (gainReductionDb) * makeupGain;
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()))
    };

    lowMidSplitter.prepare (spec);
    midHighSplitter.prepare (spec);

    for (auto& compressor : compressors)
        compressor.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()));

    ensureBufferSize (getTotalNumOutputChannels(), samplesPerBlock);
    updateDSPParameters();

    lowMidSplitter.reset();
    midHighSplitter.reset();

    for (auto& compressor : compressors)
        compressor.reset();
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::ensureBufferSize (int numChannels, int numSamples)
{
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    lowBandBuffer.setSize (numChannels, numSamples, false, false, true);
    midBandBuffer.setSize (numChannels, numSamples, false, false, true);
    highBandBuffer.setSize (numChannels, numSamples, false, false, true);
}

void PluginProcessor::updateDSPParameters()
{
    const auto nyquistLimit = static_cast<float> (currentSampleRate * 0.45);
    auto lowCutoff = juce::jlimit (20.0f, nyquistLimit, freqLow->load());
    auto highCutoff = juce::jlimit (20.0f, nyquistLimit, freqMid->load());

    if (highCutoff <= lowCutoff)
        highCutoff = juce::jmin (nyquistLimit, lowCutoff * 1.25f);

    if (highCutoff <= lowCutoff)
        lowCutoff = juce::jmax (20.0f, highCutoff * 0.8f);

    lowMidSplitter.setCutoffFrequency (lowCutoff);
    midHighSplitter.setCutoffFrequency (highCutoff);

    for (int band = 0; band < numBands; ++band)
    {
        const auto& pointers = bandParameters[static_cast<size_t> (band)];
        auto& compressor = compressors[static_cast<size_t> (band)];

        compressor.setParameters (pointers.threshold->load(),
                                  pointers.ratio->load(),
                                  pointers.attack->load(),
                                  pointers.release->load(),
                                  pointers.makeupGain->load());
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    ensureBufferSize (totalNumOutputChannels, numSamples);
    updateDSPParameters();

    dryBuffer.makeCopyOf (buffer, true);
    lowBandBuffer.clear();
    midBandBuffer.clear();
    highBandBuffer.clear();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* input = dryBuffer.getReadPointer (channel);
        auto* low = lowBandBuffer.getWritePointer (channel);
        auto* mid = midBandBuffer.getWritePointer (channel);
        auto* high = highBandBuffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto inputSample = input[sample];

            float aboveLow = 0.0f;
            lowMidSplitter.processSample (channel, inputSample, low[sample], aboveLow);
            midHighSplitter.processSample (channel, aboveLow, mid[sample], high[sample]);
        }
    }

    const std::array<juce::AudioBuffer<float>*, numBands> bandBuffers {
        &lowBandBuffer, &midBandBuffer, &highBandBuffer
    };

    for (int band = 0; band < numBands; ++band)
    {
        auto& bandBuffer = *bandBuffers[static_cast<size_t> (band)];
        auto& compressor = compressors[static_cast<size_t> (band)];

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* samples = bandBuffer.getWritePointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
                samples[sample] = compressor.processSample (channel, samples[sample]);
        }
    }

    const auto wet = juce::jlimit (0.0f, 1.0f, dryWet->load() / 100.0f);
    const auto dry = 1.0f - wet;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* drySamples = dryBuffer.getReadPointer (channel);
        const auto* low = lowBandBuffer.getReadPointer (channel);
        const auto* mid = midBandBuffer.getReadPointer (channel);
        const auto* high = highBandBuffer.getReadPointer (channel);
        auto* output = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto wetSample = low[sample] + mid[sample] + high[sample];
            output[sample] = drySamples[sample] * dry + wetSample * wet;
        }
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto state = getXmlFromBinary (data, sizeInBytes))
    {
        if (state->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*state));
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
