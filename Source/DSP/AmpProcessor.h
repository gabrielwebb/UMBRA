#pragma once
#include <JuceHeader.h>
#include "DattorroReverb.h"
#include "NeuralAmpModel.h"

// ── Granular OLA pitch shifter ─────────────────────────────────────────────
class PitchShifter
{
public:
    static constexpr int kBufLen = 8192;
    static constexpr int kGrain  = 256;
    static constexpr int kHop    = kGrain / 2;

    void reset() noexcept
    {
        buf.fill(0.f);
        wHead    = kGrain;
        rHead[0] = 0.f;
        rHead[1] = static_cast<float>(kHop);
        phase[0] = 0;
        phase[1] = kHop;
    }

    void setSemitones(int s) noexcept
    {
        semitones = s;
        ratio = (s == 0) ? 1.0f : std::pow(2.f, s / 12.f);
    }

    float process(float in) noexcept
    {
        if (semitones == 0) return in;
        buf[wHead & (kBufLen - 1)] = in;
        float out = 0.f;
        for (int i = 0; i < 2; ++i)
        {
            if (phase[i] == 0)
                rHead[i] = static_cast<float>((wHead - kGrain + i * kHop + kBufLen) & (kBufLen - 1));
            const float w  = 0.5f * (1.f - std::cos(
                juce::MathConstants<float>::twoPi * phase[i] / kGrain));
            const int   ri = static_cast<int>(rHead[i]);
            const float fr = rHead[i] - static_cast<float>(ri);
            const float s  = buf[ ri      & (kBufLen - 1)] * (1.f - fr)
                           + buf[(ri + 1) & (kBufLen - 1)] * fr;
            out += w * s;
            rHead[i] += ratio;
            if (rHead[i] >= static_cast<float>(kBufLen)) rHead[i] -= static_cast<float>(kBufLen);
            phase[i] = (phase[i] + 1) % kGrain;
        }
        ++wHead;
        return out;
    }

private:
    std::array<float, kBufLen> buf{};
    int   wHead   = 0;
    float rHead[2]{ 0.f, static_cast<float>(kHop) };
    int   phase[2]{ 0, kHop };
    int   semitones = 0;
    float ratio     = 1.0f;
};

// ── EQ Band ────────────────────────────────────────────────────────────────
struct EqBand
{
    enum Type { LowShelf, Peak, HighShelf, Notch };
    bool  enabled = false;
    Type  type    = Peak;
    float freq    = 1000.f;   // Hz
    float gainDb  = 0.f;      // -18 to +18 dB
    float q       = 0.9f;     // 0.1 to 10
};

// ── AmpProcessor ──────────────────────────────────────────────────────────
class AmpProcessor
{
public:
    enum class Channel  { Clean, Crunch, Lead };
    enum class CabType  { V30, G12T75, Greenback, Open };

    struct Params
    {
        Channel channel   = Channel::Lead;
        float gate        = 0.0f;
        float gain        = 0.5f;
        float bass        = 0.5f;
        float mid         = 0.5f;
        float treble      = 0.5f;
        float presence    = 0.5f;
        float volume      = 0.5f;
        // FX
        float reverb      = 0.0f;
        float chorus      = 0.0f;
        float delay       = 0.0f;
        float delayTimeMs = 350.0f;
        float width       = 0.5f;
        float phaser      = 0.0f;   // 0-1 wet
        float flanger     = 0.0f;   // 0-1 wet
        // Dynamics / shaping
        bool    boost     = false;
        float   comp      = 0.0f;
        float   sag       = 0.0f;
        float   trem      = 0.0f;
        int     transpose = 0;
        CabType cabType   = CabType::V30;
        // Post-amp 4-band parametric EQ
        EqBand  eq[4]     = {
            { false, EqBand::LowShelf,  100.f,  0.f, 0.7f },
            { false, EqBand::Peak,      500.f,  0.f, 1.2f },
            { false, EqBand::Peak,     2500.f,  0.f, 1.2f },
            { false, EqBand::HighShelf, 8000.f, 0.f, 0.7f },
        };
    };

    void   prepare(double sampleRate, int blockSize);
    void   process(juce::AudioBuffer<float>& buffer);
    void   setParams(const Params& p);
    void   loadIR(const juce::File& file);
    void   clearIR();
    bool   hasIR()           const noexcept { return irLoaded.load(); }
    float  getOutputLevel()  const noexcept { return outputLevel.load(); }
    float  getInputLevel()   const noexcept { return inputLevel.load(); }
    float  getCompGR()       const noexcept { return compGainReduction.load(); }
    float  getGateOpen()     const noexcept { return gateOpenLevel.load(); }
    double getSampleRate()   const noexcept { return sampleRate; }

    // Tuner + Spectrum: copy latest samples from circular buffer.
    static constexpr int kTunerBufLen    = 2048;
    static constexpr int kSpectrumBufLen = 4096;
    void getTunerSnapshot  (float* out, int len) const noexcept;
    void getSpectrumSnapshot(float* out, int len) const noexcept;

    // ── Neural amp model loading ─────────────────────────────────────────
    // Call from the message thread. Returns false if the model file is
    // unreadable or uses an unsupported architecture.
    bool         loadNeuralModel(const juce::File& file, Channel ch);
    void         clearNeuralModel(Channel ch);
    bool         hasNeuralModel(Channel ch)  const noexcept;
    juce::String getNeuralModelName(Channel ch) const noexcept;

private:
    void updateFilters();
    void resetFilterStates();

    float applyGateSample      (float x) noexcept;
    float applyCompressorSample(float x) noexcept;
    float processCleanSample   (float x) noexcept;
    float processCrunchSample  (float x) noexcept;
    float processLeadSample    (float x) noexcept;
    float processPowerAmpSample(float x) noexcept;

    static float waveshape(float x) noexcept
    { return x >= 0.0f ? 1.0f - std::exp(-x) : -1.0f + std::exp(x * 0.8f); }

    static float hardClip(float x, float t) noexcept
    { return juce::jlimit(-t, t, x); }

    static float triodeClip(float x, float drive) noexcept
    {
        x *= drive;
        return (x >= 0.0f) ? 1.0f - std::exp(-x) : -1.0f + std::exp(x * 1.3f);
    }

    static float softLimit(float x) noexcept
    {
        const float ax = std::abs(x);
        if (ax <= 0.9f) return x;
        const float excess = ax - 0.9f;
        const float sat = 0.9f + 0.1f * (excess / (1.0f + excess * 10.0f));
        return std::copysign(sat, x);
    }

    using Filt   = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    // ── Pre-amp filters (oversampled rate) ───────────────────────────────
    Filt preHpf;
    Filt lpf1, lpf2, lpf3, lpf4;
    Filt dc1,  dc2,  dc3,  dc4;
    Filt leadChunkPeak;
    Filt bassFilter, midFilter, trebleFilter, presenceFilter;

    // ── Cabinet sim ──────────────────────────────────────────────────────
    Filt cabHpf, cabLoResonance, cabMidScoop, cabPresence, cabLpf;

    // ── Gate sidechain ────────────────────────────────────────────────────
    Filt gateSidechain;

    // ── Pre-boost ─────────────────────────────────────────────────────────
    Filt  boostHpf, boostMid, boostLpf;
    float boostDrive = 6.0f;

    // ── Power amp ─────────────────────────────────────────────────────────
    Filt  powerAmpDC, powerAmpLpf;
    float powerAmpDrive = 1.5f;
    float powerEnvelope = 0.0f;
    float powerAttCoeff = 0.0f;
    float powerRelCoeff = 0.0f;

    // ── Reverb pre-delay (two independent channels) ───────────────────────
    static constexpr int kPreDelay = 2048;
    std::array<float, kPreDelay> preDelayBuf  {};
    std::array<float, kPreDelay> preDelayBuf2 {};
    int preDelayWrite   = 0;
    int preDelaySamples = 0;
    juce::AudioBuffer<float> reverbDryBuf;

    // ── Neural amp models (one per channel) ──────────────────────────────
    NeuralAmpModel neuralClean, neuralCrunch, neuralLead;

    // ── Dattorro plate reverb (replaces juce::dsp::Reverb) ───────────────
    DattorroReverb dattorroReverb;

    // ── Oversampling (8×) ─────────────────────────────────────────────────
    juce::dsp::Oversampling<float> oversampling {
        1, 3, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
    };

    juce::dsp::Convolution convolution;
    std::atomic<bool> irLoaded { false };

    // ── Chorus ────────────────────────────────────────────────────────────
    juce::dsp::Chorus<float> chorus;

    // ── Phaser ────────────────────────────────────────────────────────────
    juce::dsp::Phaser<float> phaser;

    // ── Flanger (modulated delay) ─────────────────────────────────────────
    // 1-14 ms delay, sinusoidal LFO at 0.25 Hz, ±6 ms mod depth, 70% feedback
    static constexpr int kFlangerBufLen = 4096;  // ~85 ms @ 48 kHz
    std::array<float, kFlangerBufLen> flangerBufL {}, flangerBufR {};
    int   flangerWriteL   = 0;
    int   flangerWriteR   = 0;
    float flangerLfoPhase = 0.0f;

    // ── Mono delay line ───────────────────────────────────────────────────
    juce::AudioBuffer<float> delayBuf;
    int   delayWritePos    = 0;
    int   delayTimeSamples = 0;
    float delayFeedback    = 0.35f;

    // ── Post-amp parametric EQ (base rate, stereo) ────────────────────────
    // 4 bands × 2 channels (L/R). Updated when params.eq[] changes.
    Filt postEqL[4], postEqR[4];

    // ── Gate state ────────────────────────────────────────────────────────
    float gateEnvelope = 0.0f;
    float gateAttCoeff = 0.0f;
    float gateRelCoeff = 0.0f;
    float gateThresh   = 0.0f;

    // ── SAG ───────────────────────────────────────────────────────────────
    float sagEnvelope  = 0.0f;
    float sagAttCoeff  = 0.0f;
    float sagRelCoeff  = 0.0f;

    // ── Tremolo LFO ───────────────────────────────────────────────────────
    float lfoPhase = 0.0f;
    float lfoRate  = 5.0f;

    // ── Optical compressor ────────────────────────────────────────────────
    float compEnvelope = 0.0f;
    float compAttCoeff = 0.0f;
    float compRelCoeff = 0.0f;

    // ── Cached drive values ───────────────────────────────────────────────
    float cleanDrive = 1.0f, cleanNorm = 1.0f;
    float crunchDrive1 = 3.0f, crunchDrive2 = 2.0f;
    float leadDrive1 = 8.0f, leadDrive2 = 5.0f, leadDrive3 = 3.0f, leadDrive4 = 2.0f;

    PitchShifter pitchShifter;

    // ── Meters / visual feedback (thread-safe atomics) ────────────────────
    std::atomic<float> inputLevel       { 0.0f };
    std::atomic<float> outputLevel      { 0.0f };
    std::atomic<float> compGainReduction{ 0.0f };  // 0=no GR, 1=full silence
    std::atomic<float> gateOpenLevel    { 0.0f };  // 0=closed, 1=fully open

    // ── Tuner input buffer ────────────────────────────────────────────────
    std::array<float, kTunerBufLen> tunerBuf {};
    std::atomic<int>  tunerWritePos { 0 };

    // ── Spectrum output buffer (post-processing signal) ───────────────────
    std::array<float, kSpectrumBufLen> spectrumBuf {};
    std::atomic<int>  spectrumWritePos { 0 };

    double sampleRate = 44100.0;
    Params params;
};
