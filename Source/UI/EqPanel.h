#pragma once
#include <JuceHeader.h>
#include "../DSP/AmpProcessor.h"

// ── EqPanel ────────────────────────────────────────────────────────────────
// Neural DSP-style post-amp EQ: real-time spectrum analyzer behind a
// frequency response curve with four draggable band nodes.
//
// Usage
//   • Owns the 4 EqBand objects (read them via getBands()).
//   • Set onEqChanged to be notified when any band is modified by the user.
//   • Call setExternalBands() to push new values in (e.g. from preset load).

class EqPanel : public juce::Component,
                private juce::Timer
{
public:
    explicit EqPanel(AmpProcessor& p);
    ~EqPanel() override;

    // Callback fired after any interactive change
    std::function<void()> onEqChanged;

    // Read current band state
    const EqBand* getBands() const noexcept { return bands; }

    // Push band state from outside (preset load, etc.)
    void setExternalBands(const EqBand src[4]);

    // juce::Component
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // ── Coordinate mapping ────────────────────────────────────────────────
    static constexpr float kFreqLow   = 20.f;
    static constexpr float kFreqHigh  = 20000.f;
    static constexpr float kGainMin   = -18.f;
    static constexpr float kGainMax   =  18.f;

    float freqToX(float f) const noexcept;
    float gainToY(float db) const noexcept;
    float xToFreq(float x) const noexcept;
    float yToGain(float y) const noexcept;

    // ── Spectrum (FFT) ────────────────────────────────────────────────────
    static constexpr int kFftOrder  = 11;   // 2048-pt FFT
    static constexpr int kFftSize   = 1 << kFftOrder;
    static constexpr int kBins      = kFftSize / 2;

    juce::dsp::FFT fft { kFftOrder };
    std::array<float, kFftSize * 2> fftData {};     // input window + output
    std::array<float, kFftSize>     hanningWin {};
    std::array<float, kBins>        smoothMag {};    // smoothed dB magnitude

    void computeSpectrum();
    juce::Path buildSpectrumPath(juce::Rectangle<float> area) const;

    // ── EQ curve ──────────────────────────────────────────────────────────
    juce::Path buildEqCurve(juce::Rectangle<float> area) const;
    float evalEqDb(float f) const noexcept;

    // ── Band nodes ────────────────────────────────────────────────────────
    static constexpr int kNumBands = 4;
    EqBand bands[kNumBands] = {
        { false, EqBand::LowShelf,   100.f, 0.f, 0.7f },
        { false, EqBand::Peak,       500.f, 0.f, 1.2f },
        { false, EqBand::Peak,      2500.f, 0.f, 1.2f },
        { false, EqBand::HighShelf, 8000.f, 0.f, 0.7f },
    };
    int   selectedBand  = -1;

    static juce::Colour bandColour(int b) noexcept;
    juce::Rectangle<float> plotArea() const noexcept;

    // ── References ────────────────────────────────────────────────────────
    AmpProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqPanel)
};
