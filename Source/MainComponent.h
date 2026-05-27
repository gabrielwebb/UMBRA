#pragma once
#include <JuceHeader.h>
#include "DSP/AmpProcessor.h"

// ── LookAndFeel ────────────────────────────────────────────────────────────
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

// ── Level meter ────────────────────────────────────────────────────────────
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter(AmpProcessor& p);
    void paint(juce::Graphics&) override;
    void timerCallback() override;

private:
    AmpProcessor& proc;
    float display = 0.0f;
};

// ── Preset ─────────────────────────────────────────────────────────────────
struct Preset
{
    const char*           name;
    AmpProcessor::Channel channel;
    bool  boost;
    float gate, gain, bass, mid, treble, presence, volume;
    float reverb, chorus, delay;
    float sag, trem;
};

// ── MainComponent ──────────────────────────────────────────────────────────
class MainComponent : public juce::AudioAppComponent,
                      private juce::Slider::Listener,
                      private juce::Button::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int blockSize, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void sliderValueChanged(juce::Slider*) override;
    void buttonClicked(juce::Button*) override;

    void setupKnob(juce::Slider&, juce::Label&, const juce::String& name);
    void styleChannelButton(juce::TextButton&, const juce::String& label);
    void stylePresetButton(juce::TextButton&, const juce::String& label);
    void applyPreset(const Preset&);
    void syncParams();
    void updateChannelButtonStates();
    void openAudioSettings();
    void openIRChooser();

    // ── DSP ──────────────────────────────────────────────────────────────
    AmpProcessor processor;
    juce::AudioBuffer<float> workBuffer;

    // ── LookAndFeel ──────────────────────────────────────────────────────
    UmbraLAF laf;

    // ── Channel buttons ──────────────────────────────────────────────────
    juce::TextButton chClean  { "CLEAN"  };
    juce::TextButton chCrunch { "CRUNCH" };
    juce::TextButton chLead   { "LEAD"   };
    juce::TextButton boostBtn { "808"    };   // pre-boost toggle

    AmpProcessor::Channel currentChannel = AmpProcessor::Channel::Lead;

    // ── Preset buttons ───────────────────────────────────────────────────
    juce::TextButton preDeathcore { "DEATHCORE" };
    juce::TextButton preShadow    { "SHADOW"    };
    juce::TextButton preHybrid    { "HYBRID"    };
    juce::TextButton preClean     { "CLEAN"     };
    juce::TextButton preTwinkle   { "TWINKLE"   };

    // ── Knobs ────────────────────────────────────────────────────────────
    juce::Slider gateKnob, gainKnob, bassKnob, midKnob,
                 trebleKnob, presenceKnob, volumeKnob,
                 sagKnob, tremKnob,
                 reverbKnob, chorusKnob, delayKnob;
    juce::Label  gateLabel, gainLabel, bassLabel, midLabel,
                 trebleLabel, presenceLabel, volumeLabel,
                 sagLabel, tremLabel,
                 reverbLabel, chorusLabel, delayLabel;

    // ── IR ───────────────────────────────────────────────────────────────
    juce::TextButton irLoadBtn  { "LOAD IR" };
    juce::TextButton irClearBtn { "X"       };
    juce::Label      irNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // ── Header button ────────────────────────────────────────────────────
    juce::TextButton audioBtn { "AUDIO" };

    // ── Meter ────────────────────────────────────────────────────────────
    LevelMeter meter { processor };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
