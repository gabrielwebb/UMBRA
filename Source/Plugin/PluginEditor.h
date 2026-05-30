#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DSP/AmpProcessor.h"
#include "UI/EqPanel.h"
#include "UI/SharedComponents.h"   // UmbraLAF, LevelMeter

// ── UmbraPluginEditor ──────────────────────────────────────────────────────
// Plugin-format counterpart of MainComponent. Inherits AudioProcessorEditor
// instead of AudioAppComponent so it works inside any DAW.
// All UI is identical to the standalone — same LAF, same knob layout,
// same EQ panel. Parameter changes push directly to UmbraProcessor.

class UmbraPluginEditor : public juce::AudioProcessorEditor,
                          private juce::Slider::Listener,
                          private juce::Button::Listener
{
public:
    explicit UmbraPluginEditor(UmbraProcessor& p);
    ~UmbraPluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void sliderValueChanged(juce::Slider*) override;
    void sliderDragStarted(juce::Slider*) override;
    void sliderDragEnded(juce::Slider*) override;
    void buttonClicked(juce::Button*) override;

    void setupKnob(juce::Slider&, juce::Label&, const juce::String& name, double defVal = 0.5);
    void styleChannelButton(juce::TextButton&, const juce::String& label);
    void stylePresetButton(juce::TextButton&, const juce::String& label);
    void syncParams();
    void updateChannelButtonStates();
    void openIRChooser();
    void handleTap();

    // ── Processor reference ───────────────────────────────────────────────
    UmbraProcessor& proc;

    // ── LookAndFeel (shared instance from standalone) ─────────────────────
    UmbraLAF laf;

    // ── Channel + boost ───────────────────────────────────────────────────
    juce::TextButton chClean{"CLEAN"}, chCrunch{"CRUNCH"}, chLead{"LEAD"}, boostBtn{"808"};
    AmpProcessor::Channel currentChannel = AmpProcessor::Channel::Lead;

    // ── Cabinet selector ──────────────────────────────────────────────────
    juce::TextButton cabV30{"V30"}, cabG12{"G12"}, cabGrbn{"GRBN"}, cabOpen{"OPEN"};
    AmpProcessor::CabType currentCabType = AmpProcessor::CabType::V30;

    // ── Knobs (17 total) ──────────────────────────────────────────────────
    juce::Slider compKnob, gateKnob, gainKnob, transposeKnob;
    juce::Label  compLabel, gateLabel, gainLabel, transposeLabel;
    juce::Slider bassKnob, midKnob, trebleKnob, presenceKnob;
    juce::Label  bassLabel, midLabel, trebleLabel, presenceLabel;
    juce::Slider volumeKnob, sagKnob, tremKnob;
    juce::Label  volumeLabel, sagLabel, tremLabel;
    juce::Slider reverbKnob, chorusKnob, delayKnob, widthKnob, phaserKnob, flangerKnob;
    juce::Label  reverbLabel, chorusLabel, delayLabel, widthLabel, phaserLabel, flangerLabel;

    // ── IR ────────────────────────────────────────────────────────────────
    juce::TextButton irLoadBtn{"LOAD IR"}, irClearBtn{"X"};
    juce::Label      irNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // ── Tap tempo ─────────────────────────────────────────────────────────
    juce::TextButton tapBtn{"TAP"}, subdivQ{"1/4"}, subdivD8{"d1/8"}, subdiv8{"1/8"};
    juce::Label      bpmLabel;
    juce::int64      tapHistory[4]{0,0,0,0};
    int              tapHistoryIdx  = 0;
    float            currentDelayMs = 350.f;
    float            currentSubdivMult = 1.f;

    // ── Meters ────────────────────────────────────────────────────────────
    LevelMeter inputMeter { proc.getAmpProcessor(), LevelMeter::Source::Input  };
    LevelMeter outputMeter{ proc.getAmpProcessor(), LevelMeter::Source::Output };

    // ── EQ panel ──────────────────────────────────────────────────────────
    EqPanel eqPanel { proc.getAmpProcessor() };

    // ── Knob label swap state (for value display during drag) ─────────────
    struct DragState { juce::String savedLabel; bool active = false; };
    std::map<juce::Slider*, std::pair<juce::Label*, DragState>> knobLabelMap;
    void registerKnobLabel(juce::Slider& k, juce::Label& l);
    juce::String formatKnobValue(juce::Slider& k) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UmbraPluginEditor)
};
