#pragma once
#include <JuceHeader.h>

class AmpProcessor
{
public:
    enum class Channel { Clean, Crunch, Lead };

    struct Params
    {
        Channel channel  = Channel::Lead;
        float gate       = 0.0f;
        float gain       = 0.5f;
        float bass       = 0.5f;
        float mid        = 0.5f;
        float treble     = 0.5f;
        float presence   = 0.5f;
        float volume     = 0.5f;
        // FX (used by TWINKLE and any preset that wants shimmer)
        float reverb     = 0.0f;   // 0-1 wet amount
        float chorus     = 0.0f;   // 0-1 mix
        float delay      = 0.0f;   // 0-1 mix (fixed 350 ms)
    };

    void prepare(double sampleRate, int blockSize);
    void process(juce::AudioBuffer<float>& buffer);
    void setParams(const Params& p);
    void loadIR(const juce::File& file);
    void clearIR();
    bool  hasIR()          const noexcept { return irLoaded.load(); }
    float getOutputLevel() const noexcept { return outputLevel.load(); }

private:
    void updateFilters();
    void resetFilterStates();

    float applyGateSample(float x)    noexcept;
    float processCleanSample(float x)  noexcept;
    float processCrunchSample(float x) noexcept;
    float processLeadSample(float x)   noexcept;

    static float waveshape(float x) noexcept
    {
        return x >= 0.0f ? 1.0f - std::exp(-x)
                         : -1.0f + std::exp(x * 0.8f);
    }
    static float hardClip(float x, float t) noexcept
    {
        return juce::jlimit(-t, t, x);
    }

    using Filt = juce::dsp::IIR::Filter<float>;

    Filt preHpf;
    Filt lpf1, lpf2, lpf3, lpf4;
    Filt dc1,  dc2,  dc3,  dc4;
    Filt leadChunkPeak;
    Filt bassFilter, midFilter, trebleFilter;
    Filt presenceFilter;
    Filt cabHpf, cabMidScoop, cabLpf;

    juce::dsp::Convolution convolution;
    std::atomic<bool> irLoaded { false };

    // ── FX chain ─────────────────────────────────────────────────────────
    juce::dsp::Reverb reverb;

    juce::dsp::Chorus<float> chorus;

    // Simple mono delay line
    juce::AudioBuffer<float> delayBuf;
    int   delayWritePos     = 0;
    int   delayTimeSamples  = 0;
    float delayFeedback     = 0.35f;

    // ── Gate state ────────────────────────────────────────────────────────
    float gateEnvelope = 0.0f;
    float gateAttCoeff = 0.0f;
    float gateRelCoeff = 0.0f;
    float gateThresh   = 0.0f;

    // ── Cached drive values ───────────────────────────────────────────────
    float cleanDrive   = 1.0f;
    float cleanNorm    = 1.0f;
    float crunchDrive1 = 3.0f;
    float crunchDrive2 = 2.0f;
    float leadDrive1   = 8.0f;
    float leadDrive2   = 5.0f;
    float leadDrive3   = 3.0f;
    float leadDrive4   = 2.0f;

    double sampleRate = 44100.0;
    Params params;
    Channel lastChannel = Channel::Lead;

    std::atomic<float> outputLevel { 0.0f };
};
