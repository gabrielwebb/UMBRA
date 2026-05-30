#pragma once
#include <JuceHeader.h>
#include "DSP/AmpProcessor.h"

// ── UmbraProcessor ─────────────────────────────────────────────────────────
// AudioProcessor wrapper that exposes AmpProcessor to the DAW plugin host.
// Handles prepareToPlay / processBlock / state save-restore (XML).

class UmbraProcessor : public juce::AudioProcessor
{
public:
    UmbraProcessor();
    ~UmbraProcessor() override = default;

    // ── AudioProcessor ────────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UMBRA"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── Amp access (editor uses these) ────────────────────────────────────
    AmpProcessor&       getAmpProcessor()       noexcept { return ampProcessor; }
    AmpProcessor::Params& getMutableParams()    noexcept { return currentParams; }
    void pushParams(const AmpProcessor::Params& p);

    static juce::AudioProcessor::BusesProperties getDefaultBusesProperties();

private:
    AmpProcessor         ampProcessor;
    AmpProcessor::Params currentParams;
    juce::AudioBuffer<float> workBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UmbraProcessor)
};
