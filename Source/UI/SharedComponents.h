#pragma once
#include <JuceHeader.h>
#include "DSP/AmpProcessor.h"

// ── Shared colour constants (used by paint code in both standalone + plugin) ─
namespace UmbraColors
{
    inline const juce::Colour kAccent    { 0xffcc2200 };
    inline const juce::Colour kAccentHi  { 0xffff5522 };
    inline const juce::Colour kAccentGlow{ 0x55cc2200 };
    inline const juce::Colour kPanel     { 0xff131110 };
    inline const juce::Colour kPanelHi   { 0xff1c1a18 };
    inline const juce::Colour kDark      { 0xff070604 };
}

// ── Genre colours for preset buttons ──────────────────────────────────────
juce::Colour genreColour(const juce::String& name);

// ── UmbraLAF ───────────────────────────────────────────────────────────────
class UmbraLAF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& bg,
                              bool highlighted, bool down) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool highlighted, bool down) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;
};

// ── LevelMeter ─────────────────────────────────────────────────────────────
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    enum class Source { Input, Output };
    LevelMeter(AmpProcessor& p, Source src = Source::Output);
    void paint(juce::Graphics&) override;
    void timerCallback() override;

private:
    AmpProcessor& proc;
    Source        source;
    float         display = 0.0f;
};
