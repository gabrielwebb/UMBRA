#include "MainComponent.h"

// ── Presets ────────────────────────────────────────────────────────────────
static const Preset kPresets[] =
{
    // Deathcore: 6505+ — 4-stage lead, heavy mid scoop, tight gate
    { "DEATHCORE", AmpProcessor::Channel::Lead,
      /*gate*/0.75f, /*gain*/0.88f, /*bass*/0.57f, /*mid*/0.10f,
      /*treble*/0.68f, /*presence*/0.82f, /*volume*/0.55f,
      /*reverb*/0.00f, /*chorus*/0.00f, /*delay*/0.00f },

    // Shadow (Deftones/shoegaze): warm crunch, loose gate, dark treble
    { "SHADOW", AmpProcessor::Channel::Crunch,
      /*gate*/0.18f, /*gain*/0.55f, /*bass*/0.65f, /*mid*/0.52f,
      /*treble*/0.36f, /*presence*/0.34f, /*volume*/0.55f,
      /*reverb*/0.22f, /*chorus*/0.15f, /*delay*/0.00f },

    // Hybrid (Linkin Park): punchy crunch, slight scoop, bright
    { "HYBRID", AmpProcessor::Channel::Crunch,
      /*gate*/0.42f, /*gain*/0.70f, /*bass*/0.52f, /*mid*/0.38f,
      /*treble*/0.62f, /*presence*/0.60f, /*volume*/0.55f,
      /*reverb*/0.08f, /*chorus*/0.00f, /*delay*/0.00f },

    // Clean: subtle warmth
    { "CLEAN", AmpProcessor::Channel::Clean,
      /*gate*/0.05f, /*gain*/0.22f, /*bass*/0.55f, /*mid*/0.58f,
      /*treble*/0.50f, /*presence*/0.42f, /*volume*/0.65f,
      /*reverb*/0.12f, /*chorus*/0.00f, /*delay*/0.00f },

    // Twinkle (emo math rock): TTNG/American Football/Covet
    // Clean + bright treble + lots of shimmer reverb + slow chorus + quarter-note delay
    { "TWINKLE", AmpProcessor::Channel::Clean,
      /*gate*/0.02f, /*gain*/0.15f, /*bass*/0.42f, /*mid*/0.60f,
      /*treble*/0.72f, /*presence*/0.58f, /*volume*/0.68f,
      /*reverb*/0.55f, /*chorus*/0.70f, /*delay*/0.38f },
};

// ── UmbraLAF ───────────────────────────────────────────────────────────────

static const juce::Colour kAccent   { 0xffcc2200 };
static const juce::Colour kAccentHi { 0xffff4400 };
static const juce::Colour kPanel    { 0xff141414 };
static const juce::Colour kDark     { 0xff0a0a0a };

void UmbraLAF::drawRotarySlider(juce::Graphics& g,
                                 int x, int y, int w, int h,
                                 float pos, float startAngle, float endAngle,
                                 juce::Slider&)
{
    const auto b  = juce::Rectangle<float>(x, y, w, h).reduced(5.0f);
    const auto c  = b.getCentre();
    const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;

    // Shadow
    g.setColour(kDark);
    g.fillEllipse(b.expanded(2.0f));

    // Body
    juce::ColourGradient grad(juce::Colour(0xff2e2e2e), c.x - r * 0.4f, c.y - r * 0.4f,
                              juce::Colour(0xff0e0e0e), c.x + r * 0.4f, c.y + r * 0.4f, true);
    g.setGradientFill(grad);
    g.fillEllipse(b);
    g.setColour(juce::Colour(0x22ffffff));
    g.drawEllipse(b, 1.0f);

    // Track
    auto ab = b.reduced(4.0f);
    juce::Path track;
    track.addArc(ab.getX(), ab.getY(), ab.getWidth(), ab.getHeight(), startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    const float va = startAngle + (endAngle - startAngle) * pos;
    juce::Path varc;
    varc.addArc(ab.getX(), ab.getY(), ab.getWidth(), ab.getHeight(), startAngle, va, true);
    g.setColour(kAccent);
    g.strokePath(varc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Indicator line
    const auto le = c.getPointOnCircumference(r * 0.72f, va);
    const auto ls = c.getPointOnCircumference(r * 0.22f, va);
    g.setColour(kAccentHi);
    g.drawLine(juce::Line<float>(ls, le), 2.0f);

    // Centre cap
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillEllipse(c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
}

void UmbraLAF::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                     const juce::Colour&, bool highlighted, bool down)
{
    auto b = btn.getLocalBounds().toFloat().reduced(0.5f);
    const bool tog = btn.getToggleState();

    juce::Colour bg = tog    ? kAccent.darker(0.2f)
                    : down   ? juce::Colour(0xff2a2a2a)
                    : highlighted ? juce::Colour(0xff222222)
                                  : juce::Colour(0xff1a1a1a);
    g.setColour(bg);
    g.fillRoundedRectangle(b, 3.0f);

    juce::Colour border = tog ? kAccent : juce::Colour(0xff333333);
    g.setColour(border);
    g.drawRoundedRectangle(b, 3.0f, 1.0f);
}

void UmbraLAF::drawButtonText(juce::Graphics& g, juce::TextButton& btn,
                               bool, bool)
{
    juce::Colour col = btn.getToggleState() ? juce::Colours::white
                                            : juce::Colour(0xffaaaaaa);
    g.setColour(col);
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
    g.drawText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, false);
}

void UmbraLAF::drawLabel(juce::Graphics& g, juce::Label& lbl)
{
    g.setColour(juce::Colour(0xff888888));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f)));
    g.drawText(lbl.getText(), lbl.getLocalBounds(), juce::Justification::centred, false);
}

// ── LevelMeter ─────────────────────────────────────────────────────────────

LevelMeter::LevelMeter(AmpProcessor& p) : proc(p) { startTimerHz(30); }

void LevelMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(kDark);
    g.fillRoundedRectangle(b, 4.0f);
    g.setColour(juce::Colour(0xff202020));
    g.drawRoundedRectangle(b, 4.0f, 1.0f);

    // ×2 scale: green below −6 dBFS, yellow −6 to −1.7, red above −1.7
    const float lv = juce::jlimit(0.0f, 1.0f, display * 2.0f);
    if (lv > 0.0f)
    {
        auto fill = b;
        const float fh = fill.getHeight() * lv;
        fill = fill.removeFromBottom(fh);
        juce::Colour col = lv < 0.55f ? juce::Colour(0xff22cc44)
                         : lv < 0.82f ? juce::Colour(0xffddaa00)
                                      : juce::Colour(0xffdd2200);
        g.setColour(col);
        g.fillRoundedRectangle(fill, 3.0f);
    }

    g.setColour(juce::Colour(0xff555555));
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    g.drawText("OUT", getLocalBounds(), juce::Justification::centredTop, false);
}

void LevelMeter::timerCallback()
{
    display = display * 0.75f + proc.getOutputLevel() * 0.25f;
    repaint();
}

// ── MainComponent ──────────────────────────────────────────────────────────

MainComponent::MainComponent()
{
    setLookAndFeel(&laf);

    // ── Channel buttons (radio-style) ──
    styleChannelButton(chClean,  "CLEAN");
    styleChannelButton(chCrunch, "CRUNCH");
    styleChannelButton(chLead,   "LEAD");
    chLead.setToggleState(true, juce::dontSendNotification);

    // ── Preset buttons ──
    stylePresetButton(preDeathcore, "DEATHCORE");
    stylePresetButton(preShadow,    "SHADOW");
    stylePresetButton(preHybrid,    "HYBRID");
    stylePresetButton(preClean,     "CLEAN");
    stylePresetButton(preTwinkle,   "TWINKLE");

    // ── Knobs ──
    setupKnob(gateKnob,     gateLabel,     "GATE");
    setupKnob(gainKnob,     gainLabel,     "GAIN");
    setupKnob(bassKnob,     bassLabel,     "BASS");
    setupKnob(midKnob,      midLabel,      "MID");
    setupKnob(trebleKnob,   trebleLabel,   "TREBLE");
    setupKnob(presenceKnob, presenceLabel, "PRESENCE");
    setupKnob(volumeKnob,   volumeLabel,   "VOLUME");
    setupKnob(reverbKnob,   reverbLabel,   "REVERB");
    setupKnob(chorusKnob,   chorusLabel,   "CHORUS");
    setupKnob(delayKnob,    delayLabel,    "DELAY");

    // Load deathcore preset as default
    applyPreset(kPresets[0]);

    // ── IR section ──
    irNameLabel.setText("No IR loaded", juce::dontSendNotification);
    irNameLabel.setJustificationType(juce::Justification::centredLeft);
    irNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666666));
    irNameLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.5f)));
    addAndMakeVisible(irNameLabel);

    irLoadBtn.addListener(this);
    irLoadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(irLoadBtn);

    irClearBtn.addListener(this);
    irClearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(irClearBtn);

    // ── Audio button ──
    audioBtn.addListener(this);
    audioBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(audioBtn);

    addAndMakeVisible(meter);

    // Open with stereo in/out. JUCE routes whatever input device is active.
    // If no interface is connected the input will be silent; use AUDIO to pick one.
    setAudioChannels(2, 2);
    setSize(820, 370);
}

MainComponent::~MainComponent()
{
    setLookAndFeel(nullptr);
    shutdownAudio();
}

void MainComponent::setupKnob(juce::Slider& k, juce::Label& l, const juce::String& name)
{
    k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k.setRange(0.0, 1.0, 0.001);
    k.addListener(this);
    addAndMakeVisible(k);

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void MainComponent::styleChannelButton(juce::TextButton& btn, const juce::String& label)
{
    btn.setButtonText(label);
    btn.setClickingTogglesState(false);
    btn.addListener(this);
    addAndMakeVisible(btn);
}

void MainComponent::stylePresetButton(juce::TextButton& btn, const juce::String& label)
{
    btn.setButtonText(label);
    btn.setClickingTogglesState(false);
    btn.addListener(this);
    addAndMakeVisible(btn);
}

// ── Audio callbacks ────────────────────────────────────────────────────────

void MainComponent::prepareToPlay(int blockSize, double sr)
{
    processor.prepare(sr, blockSize);
    workBuffer.setSize(2, blockSize, false, true, false);
    syncParams();
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    auto* buf         = info.buffer;
    const int start   = info.startSample;
    const int samples = info.numSamples;

    workBuffer.setSize(2, samples, false, false, true);
    workBuffer.clear();

    // Ask the live device how many input channels are actually active.
    // This works regardless of how setAudioChannels was called.
    int numIn = 0;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        numIn = dev->getActiveInputChannels().countNumberOfSetBits();

    if (numIn > 0)
    {
        const float scale = 1.0f / static_cast<float>(numIn);
        for (int ch = 0; ch < numIn && ch < buf->getNumChannels(); ++ch)
            workBuffer.addFrom(0, 0, *buf, ch, start, samples, scale);
    }

    processor.process(workBuffer);

    for (int ch = 0; ch < buf->getNumChannels(); ++ch)
        buf->copyFrom(ch, start, workBuffer, std::min(ch, 1), 0, samples);
}

void MainComponent::releaseResources() {}

// ── UI events ──────────────────────────────────────────────────────────────

void MainComponent::sliderValueChanged(juce::Slider*) { syncParams(); }

void MainComponent::buttonClicked(juce::Button* btn)
{
    if (btn == &chClean)  { currentChannel = AmpProcessor::Channel::Clean;  updateChannelButtonStates(); syncParams(); return; }
    if (btn == &chCrunch) { currentChannel = AmpProcessor::Channel::Crunch; updateChannelButtonStates(); syncParams(); return; }
    if (btn == &chLead)   { currentChannel = AmpProcessor::Channel::Lead;   updateChannelButtonStates(); syncParams(); return; }

    if (btn == &preDeathcore) { applyPreset(kPresets[0]); return; }
    if (btn == &preShadow)    { applyPreset(kPresets[1]); return; }
    if (btn == &preHybrid)    { applyPreset(kPresets[2]); return; }
    if (btn == &preClean)     { applyPreset(kPresets[3]); return; }
    if (btn == &preTwinkle)   { applyPreset(kPresets[4]); return; }

    if (btn == &irLoadBtn)  { openIRChooser();    return; }
    if (btn == &irClearBtn) {
        processor.clearIR();
        irNameLabel.setText("No IR loaded", juce::dontSendNotification);
        return;
    }
    if (btn == &audioBtn) { openAudioSettings(); return; }
}

void MainComponent::updateChannelButtonStates()
{
    chClean .setToggleState(currentChannel == AmpProcessor::Channel::Clean,  juce::dontSendNotification);
    chCrunch.setToggleState(currentChannel == AmpProcessor::Channel::Crunch, juce::dontSendNotification);
    chLead  .setToggleState(currentChannel == AmpProcessor::Channel::Lead,   juce::dontSendNotification);
    repaint();
}

void MainComponent::applyPreset(const Preset& p)
{
    currentChannel = p.channel;
    updateChannelButtonStates();

    gateKnob.setValue(p.gate,         juce::dontSendNotification);
    gainKnob.setValue(p.gain,         juce::dontSendNotification);
    bassKnob.setValue(p.bass,         juce::dontSendNotification);
    midKnob.setValue(p.mid,           juce::dontSendNotification);
    trebleKnob.setValue(p.treble,     juce::dontSendNotification);
    presenceKnob.setValue(p.presence, juce::dontSendNotification);
    volumeKnob.setValue(p.volume,     juce::dontSendNotification);
    reverbKnob.setValue(p.reverb,     juce::dontSendNotification);
    chorusKnob.setValue(p.chorus,     juce::dontSendNotification);
    delayKnob.setValue(p.delay,       juce::dontSendNotification);

    syncParams();
    repaint();
}

void MainComponent::syncParams()
{
    AmpProcessor::Params p;
    p.channel  = currentChannel;
    p.gate     = static_cast<float>(gateKnob.getValue());
    p.gain     = static_cast<float>(gainKnob.getValue());
    p.bass     = static_cast<float>(bassKnob.getValue());
    p.mid      = static_cast<float>(midKnob.getValue());
    p.treble   = static_cast<float>(trebleKnob.getValue());
    p.presence = static_cast<float>(presenceKnob.getValue());
    p.volume   = static_cast<float>(volumeKnob.getValue());
    p.reverb   = static_cast<float>(reverbKnob.getValue());
    p.chorus   = static_cast<float>(chorusKnob.getValue());
    p.delay    = static_cast<float>(delayKnob.getValue());
    processor.setParams(p);
}

void MainComponent::openAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent(
        deviceManager,
        1, 2,   // min 1 input — auto-activates channel 1 when a device is picked
        1, 2,
        false, false, false, false);
    sel->setSize(500, 340);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(sel);
    o.dialogTitle            = "Audio Settings";
    o.dialogBackgroundColour = juce::Colour(0xff1a1a1a);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar      = true;
    o.resizable              = false;
    o.launchAsync();
}

void MainComponent::openIRChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Impulse Response",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav;*.aiff;*.aif");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto results = fc.getResults();
            if (results.size() > 0)
            {
                processor.loadIR(results[0]);
                irNameLabel.setText(results[0].getFileNameWithoutExtension(),
                                    juce::dontSendNotification);
                irNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
            }
        });
}

// ── Paint ──────────────────────────────────────────────────────────────────

void MainComponent::paint(juce::Graphics& g)
{
    const int W = getWidth();

    // Background
    g.fillAll(juce::Colour(0xff0d0d0d));

    // Header gradient
    const auto header = juce::Rectangle<int>(0, 0, W, 48).toFloat();
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff1e1e1e), 0, 0,
        juce::Colour(0xff111111), 0, 48, false));
    g.fillRect(header);

    // Orange accent line under header
    g.setColour(kAccent);
    g.fillRect(0.0f, 46.0f, (float)W, 2.0f);

    // App name
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions().withHeight(24.0f).withStyle("Bold")));
    g.drawText("UMBRA", juce::Rectangle<int>(0, 0, 160, 48), juce::Justification::centred, false);

    // Subtitle
    g.setColour(kAccent);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    g.drawText("AMP SIMULATOR", juce::Rectangle<int>(0, 30, 160, 16), juce::Justification::centred, false);

    // Preset/IR row background
    g.setColour(juce::Colour(0xff111111));
    g.fillRect(0, 48, W, 38);
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRect(0, 86, W, 1);

    // Knob panel
    g.setColour(kPanel);
    g.fillRoundedRectangle(juce::Rectangle<int>(8, 94, W - 16, getHeight() - 102).toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff262626));
    g.drawRoundedRectangle(juce::Rectangle<int>(8, 94, W - 16, getHeight() - 102).toFloat(), 6.0f, 1.0f);

    // Channel section label
    g.setColour(juce::Colour(0xff444444));
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    g.drawText("CHANNEL", juce::Rectangle<int>(160, 0, 220, 14), juce::Justification::centred, false);
}

// ── Resized ────────────────────────────────────────────────────────────────

void MainComponent::resized()
{
    const int W = getWidth();

    // ── Header ──
    audioBtn.setBounds(W - 72, 11, 62, 26);

    // Channel buttons (center of header)
    const int chW = 72, chH = 26, chY = 11;
    const int chStart = 170;
    chClean .setBounds(chStart,           chY, chW, chH);
    chCrunch.setBounds(chStart + chW + 4, chY, chW, chH);
    chLead  .setBounds(chStart + chW*2+8, chY, chW, chH);

    // ── Preset / IR row (y=48, h=38) ──
    const int rowY = 50, rowH = 30;
    const int pW = 82;
    preDeathcore.setBounds(8,               rowY, pW, rowH);
    preShadow   .setBounds(8 + pW + 3,      rowY, pW, rowH);
    preHybrid   .setBounds(8 + pW*2 + 6,    rowY, pW, rowH);
    preClean    .setBounds(8 + pW*3 + 9,    rowY, 68, rowH);
    preTwinkle  .setBounds(8 + pW*3 + 9 + 68 + 3, rowY, 72, rowH);

    // IR area (right side of preset row)
    const int irClearW = 26, irLoadW = 72;
    const int irRight  = W - 10;
    irClearBtn .setBounds(irRight - irClearW,               rowY, irClearW, rowH);
    irLoadBtn  .setBounds(irRight - irClearW - irLoadW - 4, rowY, irLoadW,  rowH);
    const int irLabelX = 8 + pW*3 + 9 + 68 + 3 + 72 + 6;
    irNameLabel.setBounds(irLabelX, rowY,
                          irRight - irClearW - irLoadW - 4 - irLabelX - 6,
                          rowH);

    // ── Knob area (y=94 downward) ──
    const int knobAreaTop = 98;
    const int knobAreaH   = getHeight() - knobAreaTop - 10;
    const int meterW      = 26;
    const int meterX      = W - meterW - 14;

    meter.setBounds(meterX, knobAreaTop + 6, meterW, knobAreaH - 12);

    // 10 knob slots (7 amp + 3 FX)
    const int numKnobs = 10;
    const int kAreaW   = meterX - 16;
    const int slotW    = kAreaW / numKnobs;
    const int knobSize = juce::jmin(knobAreaH - 32, slotW - 6);
    const int labelH   = 14;
    const int knobY    = knobAreaTop + (knobAreaH - knobSize - labelH) / 2;

    auto place = [&](juce::Slider& k, juce::Label& l, int idx)
    {
        const int kx = 12 + idx * slotW + (slotW - knobSize) / 2;
        k.setBounds(kx, knobY, knobSize, knobSize);
        l.setBounds(kx - 4, knobY + knobSize + 2, knobSize + 8, labelH);
    };

    place(gateKnob,     gateLabel,     0);
    place(gainKnob,     gainLabel,     1);
    place(bassKnob,     bassLabel,     2);
    place(midKnob,      midLabel,      3);
    place(trebleKnob,   trebleLabel,   4);
    place(presenceKnob, presenceLabel, 5);
    place(volumeKnob,   volumeLabel,   6);
    place(reverbKnob,   reverbLabel,   7);
    place(chorusKnob,   chorusLabel,   8);
    place(delayKnob,    delayLabel,    9);
}
