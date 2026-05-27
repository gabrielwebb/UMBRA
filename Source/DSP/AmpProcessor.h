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
        // FX
        float reverb     = 0.0f;   // 0-1 wet amount
        float chorus     = 0.0f;   // 0-1 mix
        float delay      = 0.0f;   // 0-1 mix (fixed 350 ms)
        // New
        bool  boost      = false;  // 808/Tube Screamer pre-boost
        float sag        = 0.0f;   // 0-1 rectifier sag amount
        float trem       = 0.0f;   // 0-1 tremolo depth
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
    // Asymmetric triode plate clip: soft positive rail, harder negative rail
    static float triodeClip(float x, float drive) noexcept
    {
        x *= drive;
        return (x >= 0.0f) ? 1.0f - std::exp(-x)
                           : -1.0f + std::exp(x * 1.3f);
    }

    using Filt = juce::dsp::IIR::Filter<float>;

    Filt preHpf;
    Filt lpf1, lpf2, lpf3, lpf4;
    Filt dc1,  dc2,  dc3,  dc4;
    Filt leadChunkPeak;
    Filt bassFilter, midFilter, trebleFilter;
    Filt presenceFilter;
    Filt cabHpf, cabMidScoop, cabLpf;

    // ── Gate sidechain ────────────────────────────────────────────────────
    Filt gateSidechain;   // bandpass ~800 Hz — avoids false triggers from hum

    // ── Pre-boost (808 / Tube Screamer) ───────────────────────────────────
    Filt  boostHpf;       // HPF 720 Hz — cut muddy lows
    Filt  boostLpf;       // LPF 3500 Hz — smooth highs
    float boostDrive = 6.0f;

    // ── Oversampling (4×) ─────────────────────────────────────────────────
    juce::dsp::Oversampling<float> oversampling {
        1, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
    };

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

    // ── SAG (rectifier sag) ───────────────────────────────────────────────
    float sagEnvelope  = 0.0f;
    float sagAttCoeff  = 0.0f;   // ~2 ms
    float sagRelCoeff  = 0.0f;   // ~150 ms

    // ── Tremolo LFO ───────────────────────────────────────────────────────
    float lfoPhase    = 0.0f;
    float lfoRate     = 5.0f;   // Hz

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

    std::atomic<float> outputLevel { 0.0f };
};
