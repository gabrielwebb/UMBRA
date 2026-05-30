#pragma once
#include <JuceHeader.h>
#include "DSP/AmpProcessor.h"
#include "UI/EqPanel.h"
#include "UI/SharedComponents.h"   // UmbraLAF, LevelMeter, genreColour

// ── Chromatic tuner overlay ────────────────────────────────────────────────
class TunerComponent : public juce::Component, private juce::Timer
{
public:
    explicit TunerComponent(AmpProcessor& p);
    void paint(juce::Graphics&) override;
    void visibilityChanged() override;
    void timerCallback() override;

private:
    AmpProcessor& proc;
    std::array<float, AmpProcessor::kTunerBufLen> snapshot {};
    std::vector<float> yinWork;   // pre-allocated, avoids heap alloc in timer
    float        currentHz    = 0.0f;
    float        currentCents = 0.0f;
    juce::String noteName     { "--" };

    static float runYin(const float* buf, int bufLen,
                        std::vector<float>& work, double sr);
    static std::pair<juce::String, float> frequencyToNote(float hz);
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
    int   transpose;
    float comp        = 0.0f;
    float delayTimeMs = 350.0f;
    AmpProcessor::CabType cabType = AmpProcessor::CabType::V30;
    float width       = 0.5f;
    float phaser      = 0.0f;
    float flanger     = 0.0f;
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
    void sliderDragStarted(juce::Slider*) override;
    void sliderDragEnded(juce::Slider*) override;
    void buttonClicked(juce::Button*) override;

    // Per-knob: saves label text while dragging so we can restore it
    struct KnobLabelState { juce::Label* label = nullptr; juce::String savedText; bool dragging = false; };
    std::map<juce::Slider*, KnobLabelState> knobLabelMap;
    void registerKnob(juce::Slider& k, juce::Label& l);
    juce::String formatKnobValue(juce::Slider& k) const;

    void setupKnob(juce::Slider&, juce::Label&, const juce::String& name,
                   double defaultVal = 0.5);
    void styleChannelButton(juce::TextButton&, const juce::String& label);
    void stylePresetButton(juce::TextButton&, const juce::String& label);
    void applyPreset(const Preset&);
    void syncParams();
    void updateChannelButtonStates();
    void updatePresetButtonStates();
    void openAudioSettings();
    void openIRChooser();
    void savePreset();
    void loadPreset();
    void handleTap();

    // ── DSP ──────────────────────────────────────────────────────────────
    AmpProcessor processor;
    juce::AudioBuffer<float> workBuffer;

    // ── LookAndFeel ──────────────────────────────────────────────────────
    UmbraLAF laf;

    // ── Channel buttons ──────────────────────────────────────────────────
    juce::TextButton chClean  { "CLEAN"  };
    juce::TextButton chCrunch { "CRUNCH" };
    juce::TextButton chLead   { "LEAD"   };
    juce::TextButton boostBtn { "808"    };

    AmpProcessor::Channel currentChannel = AmpProcessor::Channel::Lead;
    int currentPresetIdx = -1;

    // ── Preset buttons ───────────────────────────────────────────────────
    juce::TextButton preDeftones { "DEFTONES" };
    juce::TextButton preKnocked  { "KNOCKED"  };
    juce::TextButton preBilmuri  { "BILMURI"  };
    juce::TextButton preHybrid   { "HYBRID"   };
    juce::TextButton preHotMul   { "HOT MUL"  };

    // ── Knobs (15 total) ─────────────────────────────────────────────────
    // INPUT section (4)
    juce::Slider compKnob,     gateKnob,     gainKnob,     transposeKnob;
    juce::Label  compLabel,    gateLabel,    gainLabel,    transposeLabel;
    // TONE section (4)
    juce::Slider bassKnob,     midKnob,      trebleKnob,   presenceKnob;
    juce::Label  bassLabel,    midLabel,     trebleLabel,  presenceLabel;
    // OUTPUT section (3)
    juce::Slider volumeKnob,   sagKnob,      tremKnob;
    juce::Label  volumeLabel,  sagLabel,     tremLabel;
    // FX section (6)
    juce::Slider reverbKnob,   chorusKnob,   delayKnob,    widthKnob,   phaserKnob,   flangerKnob;
    juce::Label  reverbLabel,  chorusLabel,  delayLabel,   widthLabel,  phaserLabel,  flangerLabel;

    // ── IR ───────────────────────────────────────────────────────────────
    juce::TextButton irLoadBtn  { "LOAD IR" };
    juce::TextButton irClearBtn { "X"       };
    juce::Label      irNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // ── Header buttons ───────────────────────────────────────────────────
    juce::TextButton audioBtn      { "AUDIO" };
    juce::TextButton tunerBtn      { "TUNER" };
    juce::TextButton savePresetBtn { "SAVE"  };
    juce::TextButton loadPresetBtn { "LOAD"  };

    // ── Cabinet character selector ───────────────────────────────────────
    juce::TextButton cabV30  { "V30"  };
    juce::TextButton cabG12  { "G12"  };
    juce::TextButton cabGrbn { "GRBN" };
    juce::TextButton cabOpen { "OPEN" };
    AmpProcessor::CabType currentCabType = AmpProcessor::CabType::V30;

    // ── Neural model loader (one per channel) ────────────────────────────
    juce::TextButton neuralLoadBtn  { "NEURAL" };   // load model for current channel
    juce::TextButton neuralClearBtn { "✕"      };   // clear current channel's model
    juce::Label      neuralNameLabel;               // shows loaded model filename
    void openNeuralModelChooser();
    void updateNeuralLabel();

    // ── Tap tempo + delay subdivisions ───────────────────────────────────
    juce::TextButton tapBtn    { "TAP" };
    juce::TextButton subdivQ   { "1/4" };
    juce::TextButton subdivD8  { "d1/8" };
    juce::TextButton subdiv8   { "1/8" };
    juce::Label      bpmLabel;
    juce::int64      tapHistory[4] { 0, 0, 0, 0 };
    int              tapHistoryIdx  = 0;
    float            currentDelayMs = 350.0f;
    float            currentSubdivMult = 1.0f;  // 1.0=quarter, 0.75=dot-8, 0.5=eighth

    // ── Tuner overlay ────────────────────────────────────────────────────
    TunerComponent tuner { processor };

    // ── Post-amp EQ panel ────────────────────────────────────────────────
    EqPanel eqPanel { processor };

    // ── Meters + gate/comp widget ────────────────────────────────────────
    LevelMeter inputMeter  { processor, LevelMeter::Source::Input  };
    LevelMeter outputMeter { processor, LevelMeter::Source::Output };
    // Gate LED + Comp GR meter — declared after the class body
    // (GateCompWidget is defined in MainComponent.cpp)
    std::unique_ptr<juce::Component> gateCompWidget;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
