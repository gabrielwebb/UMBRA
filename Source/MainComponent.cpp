#include "MainComponent.h"

// ── Presets ────────────────────────────────────────────────────────────────
// Preset fields: name, channel, boost, gate, gain, bass, mid, treble, presence, volume,
//                reverb, chorus, delay, sag, trem
static const Preset kPresets[] =
{
    // Deathcore: 6505+ — max gain, 808 boost, scooped mids, tight gate
    { "DEATHCORE", AmpProcessor::Channel::Lead,
      /*boost*/true,
      /*gate*/0.78f, /*gain*/0.90f, /*bass*/0.60f, /*mid*/0.08f,
      /*treble*/0.72f, /*presence*/0.85f, /*volume*/0.55f,
      /*reverb*/0.00f, /*chorus*/0.00f, /*delay*/0.00f,
      /*sag*/0.00f, /*trem*/0.00f },

    // Shadow (Deftones/shoegaze): warm crunch, rectifier sag, loose gate
    { "SHADOW", AmpProcessor::Channel::Crunch,
      /*boost*/false,
      /*gate*/0.20f, /*gain*/0.58f, /*bass*/0.62f, /*mid*/0.55f,
      /*treble*/0.38f, /*presence*/0.38f, /*volume*/0.55f,
      /*reverb*/0.28f, /*chorus*/0.18f, /*delay*/0.00f,
      /*sag*/0.45f, /*trem*/0.00f },

    // Hybrid (Linkin Park): punchy crunch, mid-forward, light sag
    { "HYBRID", AmpProcessor::Channel::Crunch,
      /*boost*/false,
      /*gate*/0.40f, /*gain*/0.72f, /*bass*/0.50f, /*mid*/0.42f,
      /*treble*/0.62f, /*presence*/0.62f, /*volume*/0.55f,
      /*reverb*/0.10f, /*chorus*/0.00f, /*delay*/0.00f,
      /*sag*/0.22f, /*trem*/0.00f },

    // Clean: subtle warmth
    { "CLEAN", AmpProcessor::Channel::Clean,
      /*boost*/false,
      /*gate*/0.05f, /*gain*/0.22f, /*bass*/0.55f, /*mid*/0.58f,
      /*treble*/0.50f, /*presence*/0.42f, /*volume*/0.65f,
      /*reverb*/0.12f, /*chorus*/0.00f, /*delay*/0.00f,
      /*sag*/0.00f, /*trem*/0.00f },

    // Twinkle (emo math rock): TTNG/American Football/Covet
    // Clean + bright treble + shimmer reverb + chorus + delay + tremolo
    { "TWINKLE", AmpProcessor::Channel::Clean,
      /*boost*/false,
      /*gate*/0.02f, /*gain*/0.15f, /*bass*/0.42f, /*mid*/0.60f,
      /*treble*/0.72f, /*presence*/0.58f, /*volume*/0.68f,
      /*reverb*/0.55f, /*chorus*/0.70f, /*delay*/0.38f,
      /*sag*/0.00f, /*trem*/0.65f },
};

// ── UmbraLAF ───────────────────────────────────────────────────────────────

static const juce::Colour kAccent   { 0xffcc2200 };
static const juce::Colour kAccentHi { 0xffff5522 };
static const juce::Colour kAccentGlow { 0x55cc2200 };
static const juce::Colour kPanel    { 0xff131110 };
static const juce::Colour kPanelHi  { 0xff1c1a18 };
static const juce::Colour kDark     { 0xff070604 };

void UmbraLAF::drawRotarySlider(juce::Graphics& g,
                                 int x, int y, int w, int h,
                                 float pos, float startAngle, float endAngle,
                                 juce::Slider&)
{
    const auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(4.0f);
    const auto c      = bounds.getCentre();
    const float r     = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle = startAngle + (endAngle - startAngle) * pos;

    // ── 1. Drop shadow ──
    g.setColour(juce::Colour(0xaa000000));
    g.fillEllipse(bounds.translated(0.0f, 1.5f));

    // ── 2. Outer bezel — gunmetal ring ──
    {
        juce::ColourGradient cg(juce::Colour(0xff3a3733), c.x - r * 0.5f, c.y - r * 0.5f,
                                juce::Colour(0xff0e0d0c), c.x + r * 0.6f, c.y + r * 0.7f, true);
        g.setGradientFill(cg);
        g.fillEllipse(bounds);
    }
    // Bezel highlight rim (top-left edge only)
    g.setColour(juce::Colour(0x28ffffff));
    g.drawEllipse(bounds.reduced(0.5f), 1.0f);

    // ── 3. Track groove ──
    const auto trackB = bounds.reduced(r * 0.22f);
    juce::Path groove;
    groove.addArc(trackB.getX(), trackB.getY(), trackB.getWidth(), trackB.getHeight(),
                  startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff060504));
    g.strokePath(groove, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xff1e1c1a));
    g.strokePath(groove, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // ── 4. Value arc — outer glow then bright fill ──
    if (pos > 0.001f)
    {
        juce::Path varc;
        varc.addArc(trackB.getX(), trackB.getY(), trackB.getWidth(), trackB.getHeight(),
                    startAngle, angle, true);
        // Glow halo
        g.setColour(kAccentGlow);
        g.strokePath(varc, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        // Bright arc
        g.setColour(kAccentHi);
        g.strokePath(varc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // ── 5. Knob body — inset metallic cap ──
    const auto bodyB = bounds.reduced(r * 0.30f);
    {
        juce::ColourGradient cg(juce::Colour(0xff3e3b37), c.x - r * 0.2f, c.y - r * 0.3f,
                                juce::Colour(0xff141210), c.x + r * 0.3f, c.y + r * 0.4f, true);
        g.setGradientFill(cg);
        g.fillEllipse(bodyB);
    }

    // ── 6. Specular highlight (top-left lens reflection) ──
    {
        juce::ColourGradient cg(juce::Colour(0x38ffffff), c.x - r * 0.18f, c.y - r * 0.22f,
                                juce::Colour(0x00ffffff), c.x + r * 0.05f, c.y + r * 0.05f, true);
        g.setGradientFill(cg);
        g.fillEllipse(bodyB);
    }

    // ── 7. Pointer dot ──
    const float bodyR  = juce::jmin(bodyB.getWidth(), bodyB.getHeight()) * 0.5f;
    const float dotR   = juce::jmax(2.0f, bodyR * 0.14f);
    const float dotDst = bodyR - dotR * 1.5f;
    const auto  dotPos = c.getPointOnCircumference(dotDst, angle);
    // Shadow under dot
    g.setColour(juce::Colour(0x88000000));
    g.fillEllipse(dotPos.x - dotR + 0.6f, dotPos.y - dotR + 0.7f, dotR * 2.0f, dotR * 2.0f);
    // Dot — bright white when non-zero, dimmed at minimum
    g.setColour(pos > 0.01f ? juce::Colour(0xfff0eee8) : juce::Colour(0xff555250));
    g.fillEllipse(dotPos.x - dotR, dotPos.y - dotR, dotR * 2.0f, dotR * 2.0f);
}

void UmbraLAF::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                     const juce::Colour&, bool highlighted, bool down)
{
    auto b = btn.getLocalBounds().toFloat().reduced(0.5f);
    const bool tog  = btn.getToggleState();
    const auto text = btn.getButtonText();

    // Background fill
    juce::Colour bg = tog    ? kPanel.brighter(0.08f)
                    : down   ? juce::Colour(0xff252320)
                    : highlighted ? juce::Colour(0xff1e1c1a)
                                  : juce::Colour(0xff161412);
    g.setColour(bg);
    g.fillRoundedRectangle(b, 3.0f);

    // Border — accent colour when active, subtle otherwise
    juce::Colour border = tog ? kAccent.withAlpha(0.85f) : juce::Colour(0xff2e2c2a);
    g.setColour(border);
    g.drawRoundedRectangle(b, 3.0f, 1.0f);

    // ── Channel LED indicator dot (left side of button) ──
    const bool isChannel = (text == "CLEAN" || text == "CRUNCH" || text == "LEAD");
    if (isChannel)
    {
        const juce::Colour ledOn  = (text == "CLEAN")  ? juce::Colour(0xff33bbff)
                                  : (text == "CRUNCH") ? juce::Colour(0xffff8833)
                                                       : juce::Colour(0xffff2200);
        const juce::Colour ledOff = ledOn.withAlpha(0.18f).withBrightness(0.2f);
        const float dotR = 3.0f;
        const float dotX = b.getX() + 8.0f;
        const float dotY = b.getCentreY();
        if (tog)
        {
            // Glow
            g.setColour(ledOn.withAlpha(0.35f));
            g.fillEllipse(dotX - dotR * 2.0f, dotY - dotR * 2.0f, dotR * 4.0f, dotR * 4.0f);
            g.setColour(ledOn);
            g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
            // Specular
            g.setColour(juce::Colours::white.withAlpha(0.55f));
            g.fillEllipse(dotX - dotR * 0.45f, dotY - dotR * 0.75f, dotR * 0.8f, dotR * 0.6f);
        }
        else
        {
            g.setColour(ledOff);
            g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }
}

void UmbraLAF::drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool)
{
    const bool tog  = btn.getToggleState();
    const auto text = btn.getButtonText();
    const bool isChannel = (text == "CLEAN" || text == "CRUNCH" || text == "LEAD");

    // Shift text right on channel buttons to leave room for the LED
    const auto textBounds = isChannel
        ? btn.getLocalBounds().withTrimmedLeft(8)
        : btn.getLocalBounds();

    juce::Colour col = tog ? juce::Colours::white : juce::Colour(0xff888480);
    g.setColour(col);
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
    g.drawText(text, textBounds, juce::Justification::centred, false);
}

void UmbraLAF::drawLabel(juce::Graphics& g, juce::Label& lbl)
{
    g.setColour(juce::Colour(0xff6a6560));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText(lbl.getText(), lbl.getLocalBounds(), juce::Justification::centred, false);
}

// ── LevelMeter ─────────────────────────────────────────────────────────────

LevelMeter::LevelMeter(AmpProcessor& p) : proc(p) { startTimerHz(30); }

void LevelMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);

    // Well/background
    g.setColour(kDark);
    g.fillRoundedRectangle(b, 4.0f);
    g.setColour(juce::Colour(0xff1e1c1a));
    g.drawRoundedRectangle(b, 4.0f, 0.5f);

    // "OUT" label
    g.setColour(juce::Colour(0xff3a3632));
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
    g.drawText("OUT", getLocalBounds(), juce::Justification::centredTop, false);

    // Segmented LEDs — 13 segments (bottom to top)
    // Segments 0-8: green, 9-10: yellow, 11-12: red
    constexpr int kSegs = 13;
    const float  padT   = 14.0f;   // space for "OUT" label
    const float  padB   = 3.0f;
    const float  gap    = 1.5f;
    const float  totalH = b.getHeight() - padT - padB;
    const float  segH   = (totalH - gap * (kSegs - 1)) / kSegs;
    const float  segW   = b.getWidth() - 6.0f;
    const float  segX   = b.getX() + 3.0f;

    // Level: scale ×2 so signal at 0.5 fills meter to yellow
    const float level = juce::jlimit(0.0f, 1.0f, display * 2.0f);
    const int   lit   = static_cast<int>(level * kSegs + 0.4f);

    for (int i = 0; i < kSegs; ++i)
    {
        const float segY = b.getBottom() - padB - (i + 1) * segH - i * gap;
        const bool  on   = (i < lit);

        juce::Colour col;
        if (i >= kSegs - 2)        // top 2 = red
            col = on ? juce::Colour(0xffee2200) : juce::Colour(0xff1e0500);
        else if (i >= kSegs - 4)   // next 2 = yellow
            col = on ? juce::Colour(0xffddcc00) : juce::Colour(0xff1a1500);
        else                       // bottom 9 = green
            col = on ? juce::Colour(0xff11dd44) : juce::Colour(0xff052010);

        g.setColour(col);
        g.fillRoundedRectangle(segX, segY, segW, segH, 1.0f);

        // Tiny specular sheen on lit segments
        if (on)
        {
            g.setColour(juce::Colour(0x18ffffff));
            g.fillRoundedRectangle(segX, segY, segW, segH * 0.4f, 1.0f);
        }
    }
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

    // ── 808 boost toggle ──
    boostBtn.setClickingTogglesState(true);
    boostBtn.addListener(this);
    addAndMakeVisible(boostBtn);

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
    setupKnob(sagKnob,      sagLabel,      "SAG");
    setupKnob(tremKnob,     tremLabel,     "TREM");
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

    setAudioChannels(2, 2);
    setSize(920, 390);
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

    if (btn == &boostBtn) { syncParams(); return; }

    if (btn == &preDeathcore) { applyPreset(kPresets[0]); return; }
    if (btn == &preShadow)    { applyPreset(kPresets[1]); return; }
    if (btn == &preHybrid)    { applyPreset(kPresets[2]); return; }
    if (btn == &preClean)     { applyPreset(kPresets[3]); return; }
    if (btn == &preTwinkle)   { applyPreset(kPresets[4]); return; }

    if (btn == &irLoadBtn)  { openIRChooser(); return; }
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

    boostBtn.setToggleState(p.boost, juce::dontSendNotification);

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
    sagKnob.setValue(p.sag,           juce::dontSendNotification);
    tremKnob.setValue(p.trem,         juce::dontSendNotification);

    syncParams();
    repaint();
}

void MainComponent::syncParams()
{
    AmpProcessor::Params p;
    p.channel  = currentChannel;
    p.boost    = boostBtn.getToggleState();
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
    p.sag      = static_cast<float>(sagKnob.getValue());
    p.trem     = static_cast<float>(tremKnob.getValue());
    processor.setParams(p);
}

void MainComponent::openAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent(
        deviceManager,
        1, 2,
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
    const int H = getHeight();

    // ── Full background ──
    g.fillAll(juce::Colour(0xff0d0b09));

    // ── Header ──
    {
        juce::ColourGradient hg(juce::Colour(0xff201e1c), 0.0f, 0.0f,
                                juce::Colour(0xff0e0c0b), 0.0f, 48.0f, false);
        g.setGradientFill(hg);
        g.fillRect(0, 0, W, 48);
    }
    // Thin accent bar under header
    g.setColour(kAccent);
    g.fillRect(0.0f, 46.0f, (float)W, 2.0f);

    // ── UMBRA wordmark ──
    // Red "U" + white "MBRA"
    g.setFont(juce::Font(juce::FontOptions().withHeight(26.0f).withStyle("Bold")));
    const int nameX = 18, nameY = 0, nameH = 46;
    g.setColour(kAccentHi);
    g.drawText("U", nameX, nameY, 18, nameH, juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xfff0eee8));
    g.drawText("MBRA", nameX + 16, nameY, 76, nameH, juce::Justification::centredLeft, false);

    // Model badge below wordmark
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
    g.setColour(juce::Colour(0xff504844));
    g.drawText("AMP SIMULATOR", nameX, 32, 120, 13, juce::Justification::centredLeft, false);

    // ── Preset row ──
    g.setColour(juce::Colour(0xff0e0c0b));
    g.fillRect(0, 48, W, 38);
    // Subtle bottom edge
    g.setColour(juce::Colour(0xff282420));
    g.fillRect(0, 85, W, 1);

    // ── Knob panel ──
    const auto panelBounds = juce::Rectangle<int>(8, 92, W - 16, H - 100).toFloat();
    // Panel background
    g.setColour(kPanel);
    g.fillRoundedRectangle(panelBounds, 6.0f);
    // Inner top-edge highlight
    g.setColour(juce::Colour(0xff242220));
    g.drawLine(panelBounds.getX() + 6, panelBounds.getY() + 1,
               panelBounds.getRight() - 6, panelBounds.getY() + 1, 1.0f);
    // Outer border
    g.setColour(juce::Colour(0xff2a2724));
    g.drawRoundedRectangle(panelBounds, 6.0f, 1.0f);

    // ── Section labels + separators ──
    // Compute slot width the same way resized() does
    {
        const int meterX = W - 26 - 14;
        const int kAreaW = meterX - 16;
        const int slotW  = kAreaW / 12;
        const int panelTop = 92;
        const int panelH   = H - 100;

        // Sections: INPUT(0-1), TONE(2-5), OUTPUT(6-8), FX(9-11)
        struct Sec { int start, count; const char* name; } secs[] = {
            { 0, 2, "INPUT" }, { 2, 4, "TONE" }, { 6, 3, "OUTPUT" }, { 9, 3, "FX" }
        };

        g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));

        for (auto& s : secs)
        {
            const int sx = 12 + s.start * slotW;
            const int sw = s.count * slotW;

            // Section label
            g.setColour(juce::Colour(0xff484240));
            g.drawText(s.name, sx, panelTop + 6, sw, 13, juce::Justification::centred, false);

            // Separator after section (not after the last one)
            if (s.start + s.count < 12)
            {
                const int sepX = 12 + (s.start + s.count) * slotW;
                // Dark slot
                g.setColour(juce::Colour(0xff0a0908));
                g.fillRect(sepX - 1, panelTop + 20, 2, panelH - 26);
                // Lighter half
                g.setColour(juce::Colour(0xff222220));
                g.fillRect(sepX, panelTop + 20, 1, panelH - 26);
            }
        }
    }

    // ── "CHANNEL" micro-label above header buttons ──
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
    g.setColour(juce::Colour(0xff3a3632));
    g.drawText("CHANNEL", 162, 1, 230, 10, juce::Justification::centred, false);
}

// ── Resized ────────────────────────────────────────────────────────────────

void MainComponent::resized()
{
    const int W = getWidth();

    // ── Header ──
    audioBtn.setBounds(W - 72, 11, 62, 26);

    // Channel buttons + 808 toggle (center-left of header)
    const int chW = 68, chH = 26, chY = 11;
    const int chStart = 162;
    chClean .setBounds(chStart,           chY, chW, chH);
    chCrunch.setBounds(chStart + chW + 4, chY, chW, chH);
    chLead  .setBounds(chStart + chW*2+8, chY, chW, chH);
    boostBtn.setBounds(chStart + chW*3+16, chY, 46, chH);   // "808" button

    // ── Preset / IR row (y=48, h=38) ──
    const int rowY = 50, rowH = 30;
    const int pW = 80;
    preDeathcore.setBounds(8,               rowY, pW, rowH);
    preShadow   .setBounds(8 + pW + 3,      rowY, pW, rowH);
    preHybrid   .setBounds(8 + pW*2 + 6,    rowY, pW, rowH);
    preClean    .setBounds(8 + pW*3 + 9,    rowY, 66, rowH);
    preTwinkle  .setBounds(8 + pW*3 + 9 + 66 + 3, rowY, 70, rowH);

    // IR area (right side of preset row)
    const int irClearW = 26, irLoadW = 72;
    const int irRight  = W - 10;
    irClearBtn .setBounds(irRight - irClearW,               rowY, irClearW, rowH);
    irLoadBtn  .setBounds(irRight - irClearW - irLoadW - 4, rowY, irLoadW,  rowH);
    const int irLabelX = 8 + pW*3 + 9 + 66 + 3 + 70 + 6;
    irNameLabel.setBounds(irLabelX, rowY,
                          irRight - irClearW - irLoadW - 4 - irLabelX - 6,
                          rowH);

    // ── Knob area (y=92 downward) ── 12 knobs + section labels
    const int knobAreaTop  = 92;
    const int knobAreaH    = getHeight() - knobAreaTop - 8;
    const int meterW       = 26;
    const int meterX       = W - meterW - 14;
    const int secLabelH    = 22;   // height reserved for section labels at panel top

    meter.setBounds(meterX, knobAreaTop + 8, meterW, knobAreaH - 14);

    const int numKnobs = 12;
    const int kAreaW   = meterX - 16;
    const int slotW    = kAreaW / numKnobs;
    const int knobSize = juce::jmin(knobAreaH - secLabelH - 34, slotW - 6);
    const int labelH   = 14;
    // Center knobs in the remaining space below the section-label strip
    const int knobY    = knobAreaTop + secLabelH
                         + (knobAreaH - secLabelH - knobSize - labelH) / 2;

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
    place(sagKnob,      sagLabel,      7);
    place(tremKnob,     tremLabel,     8);
    place(reverbKnob,   reverbLabel,   9);
    place(chorusKnob,   chorusLabel,   10);
    place(delayKnob,    delayLabel,    11);
}
