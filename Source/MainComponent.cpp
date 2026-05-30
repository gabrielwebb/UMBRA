#include "MainComponent.h"

// ── Presets ────────────────────────────────────────────────────────────────
// Fields: name, channel, boost, gate, gain, bass, mid, treble, presence, volume,
//         reverb, chorus, delay, sag, trem, transpose  [comp=0, delayTimeMs=350]
static const Preset kPresets[] =
{
    // Deftones — White Pony: Mesa Dual Rectifier, warm sag
    { "DEFTONES", AmpProcessor::Channel::Crunch,
      /*boost*/false,
      0.18f, 0.62f, 0.65f, 0.45f, 0.40f, 0.30f, 0.58f,
      0.15f, 0.10f, 0.00f,
      0.60f, 0.00f, 0 },

    // Knocked Loose — gnarly 5150 + 808 deathcore
    { "KNOCKED", AmpProcessor::Channel::Lead,
      /*boost*/true,
      0.72f, 0.88f, 0.55f, 0.12f, 0.78f, 0.85f, 0.55f,
      0.00f, 0.00f, 0.00f,
      0.05f, 0.00f, 0 },

    // Bilmuri — modern metalcore punch
    { "BILMURI", AmpProcessor::Channel::Lead,
      /*boost*/false,
      0.42f, 0.74f, 0.54f, 0.40f, 0.65f, 0.65f, 0.55f,
      0.06f, 0.00f, 0.00f,
      0.12f, 0.00f, 0 },

    // Linkin Park — Hybrid Theory: mid-forward Mesa crunch
    { "HYBRID", AmpProcessor::Channel::Crunch,
      /*boost*/false,
      0.38f, 0.72f, 0.50f, 0.52f, 0.60f, 0.60f, 0.55f,
      0.10f, 0.00f, 0.00f,
      0.22f, 0.00f, 0 },

    // Hot Mulligan — chimey Fender clean with shimmer
    { "HOT MUL", AmpProcessor::Channel::Clean,
      /*boost*/false,
      0.03f, 0.26f, 0.50f, 0.58f, 0.68f, 0.52f, 0.62f,
      0.22f, 0.15f, 0.20f,
      0.00f, 0.00f, 0 },
};

// ── Local colour aliases (matching SharedComponents.h namespace) ───────────
static const juce::Colour kAccent    = UmbraColors::kAccent;
static const juce::Colour kAccentHi  = UmbraColors::kAccentHi;
static const juce::Colour kAccentGlow= UmbraColors::kAccentGlow;
static const juce::Colour kPanel     = UmbraColors::kPanel;
static const juce::Colour kDark      = UmbraColors::kDark;

// ── GateLED ────────────────────────────────────────────────────────────────
// (UmbraLAF + LevelMeter implementations now live in UI/SharedComponents.cpp)
// Shows gate open (green) / closed (red) state and comp gain reduction bar.
// Positioned inline by MainComponent::resized() between meter and knobs.
class GateCompWidget : public juce::Component, private juce::Timer
{
public:
    explicit GateCompWidget(AmpProcessor& p) : proc(p) { startTimerHz(30); }

    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced(1.f);

        g.setColour(juce::Colour(0xff0a0908));
        g.fillRoundedRectangle(b, 3.f);

        // Gate LED (top half)
        const float ledR  = 4.f;
        const float ledX  = b.getCentreX();
        const float ledY  = b.getY() + 10.f;
        const juce::Colour gateOn  = juce::Colour(0xff11dd44);
        const juce::Colour gateOff = juce::Colour(0xffcc1100);
        const float go = gateOpen;
        const juce::Colour ledCol = go > 0.5f
            ? gateOn.interpolatedWith(gateOff, 0.f)
            : gateOff.interpolatedWith(gateOn, go * 2.f);

        g.setColour(ledCol.withAlpha(0.25f));
        g.fillEllipse(ledX - ledR * 2, ledY - ledR * 2, ledR * 4, ledR * 4);
        g.setColour(ledCol);
        g.fillEllipse(ledX - ledR, ledY - ledR, ledR * 2, ledR * 2);

        g.setFont(juce::Font(juce::FontOptions().withHeight(7.f)));
        g.setColour(juce::Colour(0xff3a3632));
        g.drawText("G", static_cast<int>(ledX - 5), static_cast<int>(ledY + ledR + 1), 10, 10, juce::Justification::centred, false);

        // Comp GR bar (bottom half)
        const float barX = b.getX() + 2.f;
        const float barW = b.getWidth() - 4.f;
        const float barTop  = b.getY() + b.getHeight() * 0.55f;
        const float barBotY = b.getBottom() - 6.f;
        const float barH    = barBotY - barTop;

        g.setColour(juce::Colour(0xff1a1816));
        g.fillRoundedRectangle(barX, barTop, barW, barH, 2.f);

        const float gr = juce::jlimit(0.f, 1.f, compGR);
        if (gr > 0.01f)
        {
            const juce::Colour grCol = gr < 0.4f ? juce::Colour(0xff11dd44)
                                     : gr < 0.7f ? juce::Colour(0xffddcc00)
                                                 : juce::Colour(0xffee2200);
            g.setColour(grCol);
            g.fillRoundedRectangle(barX, barTop + barH * (1.f - gr), barW, barH * gr, 2.f);
        }

        g.setFont(juce::Font(juce::FontOptions().withHeight(7.f)));
        g.setColour(juce::Colour(0xff3a3632));
        g.drawText("GR", static_cast<int>(barX), static_cast<int>(barBotY), static_cast<int>(barW), 8, juce::Justification::centred, false);
    }

    void timerCallback() override
    {
        gateOpen = gateOpen * 0.7f + proc.getGateOpen() * 0.3f;
        compGR   = compGR   * 0.7f + proc.getCompGR()   * 0.3f;
        repaint();
    }

private:
    AmpProcessor& proc;
    float gateOpen = 1.f;
    float compGR   = 0.f;
};

// ── TunerComponent ─────────────────────────────────────────────────────────

TunerComponent::TunerComponent(AmpProcessor& p) : proc(p)
{
    yinWork.resize(AmpProcessor::kTunerBufLen / 2, 0.0f);
    setInterceptsMouseClicks(false, false);
}

void TunerComponent::visibilityChanged()
{
    if (isVisible())
        startTimerHz(20);
    else
    {
        stopTimer();
        currentHz    = 0.0f;
        currentCents = 0.0f;
        noteName     = "--";
    }
}

void TunerComponent::timerCallback()
{
    proc.getTunerSnapshot(snapshot.data(), AmpProcessor::kTunerBufLen);
    currentHz = runYin(snapshot.data(), AmpProcessor::kTunerBufLen, yinWork,
                       proc.getSampleRate());
    if (currentHz > 30.0f)
    {
        auto [name, cents] = frequencyToNote(currentHz);
        noteName     = name;
        currentCents = cents;
    }
    else
    {
        noteName     = "--";
        currentCents = 0.0f;
    }
    repaint();
}

void TunerComponent::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds();

    // Semi-opaque dark panel
    g.setColour(juce::Colour(0xf40d0b09));
    g.fillRoundedRectangle(b.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff2a2724));
    g.drawRoundedRectangle(b.toFloat().reduced(0.5f), 6.0f, 1.0f);

    // Header
    g.setColour(juce::Colour(0xff484240));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText("CHROMATIC TUNER", b.withHeight(22), juce::Justification::centred, false);

    // Note name
    g.setFont(juce::Font(juce::FontOptions().withHeight(76.0f).withStyle("Bold")));
    g.setColour(currentHz > 30.0f ? juce::Colours::white : juce::Colour(0xff2e2c2a));
    g.drawText(noteName,
               b.withTrimmedTop(18).withTrimmedBottom(52),
               juce::Justification::centred, false);

    if (currentHz > 30.0f)
    {
        // Hz readout
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.setColour(juce::Colour(0xff555250));
        g.drawText(juce::String(currentHz, 1) + " Hz",
                   b.withTrimmedTop(b.getHeight() - 50).withTrimmedBottom(26),
                   juce::Justification::centred, false);

        // Cents bar
        const float centsNorm = juce::jlimit(-1.0f, 1.0f, currentCents / 50.0f);
        const auto barArea = b.withTrimmedTop(b.getHeight() - 28)
                              .withTrimmedBottom(6)
                              .reduced(30, 0);
        const float midX    = static_cast<float>(barArea.getCentreX());
        const float barYf   = static_cast<float>(barArea.getY());
        const float barHf   = static_cast<float>(barArea.getHeight());
        const float barWf   = static_cast<float>(barArea.getWidth());

        // Background track
        g.setColour(juce::Colour(0xff1e1c1a));
        g.fillRoundedRectangle(static_cast<float>(barArea.getX()), barYf,
                               barWf, barHf, 4.0f);

        // Centre tick
        g.setColour(juce::Colour(0xff3a3632));
        g.fillRect(midX - 0.5f, barYf, 1.0f, barHf);

        // Needle colour: green ±5¢, yellow ±15¢, red beyond
        const juce::Colour needleCol =
            std::abs(currentCents) < 5.0f  ? juce::Colour(0xff11dd44) :
            std::abs(currentCents) < 15.0f ? juce::Colour(0xffddcc00) :
                                             juce::Colour(0xffee2200);

        const float needleX = midX + centsNorm * barWf * 0.5f;
        const float fillX   = std::min(midX, needleX);
        const float fillW   = std::abs(needleX - midX);
        if (fillW > 1.0f)
        {
            g.setColour(needleCol.withAlpha(0.3f));
            g.fillRoundedRectangle(fillX, barYf, fillW, barHf, 3.0f);
        }
        g.setColour(needleCol);
        g.fillRoundedRectangle(needleX - 3.0f, barYf - 2.0f, 6.0f, barHf + 4.0f, 3.0f);

        // ±50¢ labels
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.setColour(juce::Colour(0xff3a3632));
        g.drawText("-50", barArea.withTrimmedRight(barArea.getWidth() - 28).translated(-30, 0),
                   juce::Justification::centredRight, false);
        g.drawText("+50", barArea.withTrimmedLeft(barArea.getWidth() - 28).translated(30, 0),
                   juce::Justification::centredLeft, false);
    }
}

// YIN pitch detection — autocorrelation-based, guitar range 55-1400 Hz
float TunerComponent::runYin(const float* buf, int bufLen,
                              std::vector<float>& d, double sr)
{
    const int W      = bufLen / 2;
    const int tauMin = std::max(2, static_cast<int>(sr / 1400.0));
    const int tauMax = std::min(W - 1, static_cast<int>(sr / 55.0));

    std::fill(d.begin(), d.end(), 0.0f);
    d[0] = 1.0f;

    // Cumulative mean normalised difference function (steps 1-3 of YIN paper)
    float cumSum = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < W; ++j)
        {
            const float diff = buf[j] - buf[j + tau];
            sum += diff * diff;
        }
        cumSum += sum;
        d[tau] = (cumSum > 0.0f) ? (sum * static_cast<float>(tau) / cumSum) : 1.0f;
    }

    // Step 4: absolute threshold search with parabolic interpolation
    constexpr float kThresh = 0.15f;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (d[tau] < kThresh)
        {
            float frac = 0.0f;
            if (tau > 1 && tau < tauMax)
            {
                const float a = d[tau - 1], bv = d[tau], c = d[tau + 1];
                const float denom = 2.0f * bv - a - c;
                if (denom > 1e-6f)
                    frac = juce::jlimit(-0.5f, 0.5f, 0.5f * (a - c) / denom);
            }
            return static_cast<float>(sr) / (static_cast<float>(tau) + frac);
        }
    }
    return 0.0f;
}

std::pair<juce::String, float> TunerComponent::frequencyToNote(float hz)
{
    if (hz < 20.0f) return {"--", 0.0f};
    const float noteNum = 12.0f * std::log2(hz / 440.0f) + 69.0f;
    const int   midi    = static_cast<int>(std::round(noteNum));
    const float cents   = (noteNum - static_cast<float>(midi)) * 100.0f;
    static const char* kNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    const int oct = midi / 12 - 1;
    return { juce::String(kNames[((midi % 12) + 12) % 12]) + juce::String(oct), cents };
}

// ── MainComponent ──────────────────────────────────────────────────────────

MainComponent::MainComponent()
{
    setLookAndFeel(&laf);

    // ── Channel buttons ──
    styleChannelButton(chClean,  "CLEAN");
    styleChannelButton(chCrunch, "CRUNCH");
    styleChannelButton(chLead,   "LEAD");
    chLead.setToggleState(true, juce::dontSendNotification);

    boostBtn.setClickingTogglesState(true);
    boostBtn.addListener(this);
    addAndMakeVisible(boostBtn);

    // ── Preset buttons ──
    stylePresetButton(preDeftones, "DEFTONES");
    stylePresetButton(preKnocked,  "KNOCKED");
    stylePresetButton(preBilmuri,  "BILMURI");
    stylePresetButton(preHybrid,   "HYBRID");
    stylePresetButton(preHotMul,   "HOT MUL");

    // ── INPUT section knobs ──
    setupKnob(compKnob,      compLabel,      "COMP",    0.0);
    setupKnob(gateKnob,      gateLabel,      "GATE",    0.0);
    setupKnob(gainKnob,      gainLabel,      "GAIN",    0.5);

    transposeKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    transposeKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    transposeKnob.setRange(-12.0, 12.0, 1.0);
    transposeKnob.setValue(0.0, juce::dontSendNotification);
    transposeKnob.setDoubleClickReturnValue(true, 0.0);
    transposeKnob.addListener(this);
    addAndMakeVisible(transposeKnob);
    transposeLabel.setText("XPOSE", juce::dontSendNotification);
    transposeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(transposeLabel);
    registerKnob(transposeKnob, transposeLabel);

    // ── TONE section knobs ──
    setupKnob(bassKnob,      bassLabel,      "BASS",    0.5);
    setupKnob(midKnob,       midLabel,       "MID",     0.5);
    setupKnob(trebleKnob,    trebleLabel,    "TREBLE",  0.5);
    setupKnob(presenceKnob,  presenceLabel,  "PRESENCE",0.5);

    // ── OUTPUT section knobs ──
    setupKnob(volumeKnob,    volumeLabel,    "VOLUME",  0.5);
    setupKnob(sagKnob,       sagLabel,       "SAG",     0.0);
    setupKnob(tremKnob,      tremLabel,      "TREM",    0.0);

    // ── FX section knobs ──
    setupKnob(reverbKnob,    reverbLabel,    "REVERB",  0.0);
    setupKnob(chorusKnob,    chorusLabel,    "CHORUS",  0.0);
    setupKnob(delayKnob,     delayLabel,     "DELAY",   0.0);
    setupKnob(widthKnob,     widthLabel,     "WIDTH",   0.5);
    setupKnob(phaserKnob,    phaserLabel,    "PHASE",   0.0);
    setupKnob(flangerKnob,   flangerLabel,   "FLANGE",  0.0);

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

    // ── Header buttons ──
    audioBtn.addListener(this);
    audioBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(audioBtn);

    tunerBtn.setClickingTogglesState(true);
    tunerBtn.addListener(this);
    tunerBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(tunerBtn);

    savePresetBtn.addListener(this);
    savePresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(savePresetBtn);

    loadPresetBtn.addListener(this);
    loadPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(loadPresetBtn);

    // ── Cabinet character buttons ──
    auto setupCabBtn = [this](juce::TextButton& btn)
    {
        btn.setClickingTogglesState(false);
        btn.addListener(this);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
        addAndMakeVisible(btn);
    };
    setupCabBtn(cabV30);   cabV30 .setToggleState(true,  juce::dontSendNotification);
    setupCabBtn(cabG12);
    setupCabBtn(cabGrbn);
    setupCabBtn(cabOpen);

    // ── Delay subdivision buttons ──
    auto setupSubdivBtn = [this](juce::TextButton& btn)
    {
        btn.setClickingTogglesState(false);
        btn.addListener(this);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
        addAndMakeVisible(btn);
    };
    setupSubdivBtn(subdivQ);  subdivQ.setToggleState(true, juce::dontSendNotification);
    setupSubdivBtn(subdivD8);
    setupSubdivBtn(subdiv8);

    // ── Tap tempo ──
    tapBtn.addListener(this);
    tapBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(tapBtn);

    bpmLabel.setText("-- BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xff555250));
    bpmLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    addAndMakeVisible(bpmLabel);

    // ── Tuner (hidden initially) ──
    addAndMakeVisible(tuner);
    tuner.setVisible(false);

    // ── Meters ──
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    // ── Neural model loader ──
    neuralLoadBtn.addListener(this);
    neuralLoadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(neuralLoadBtn);

    neuralClearBtn.addListener(this);
    neuralClearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(neuralClearBtn);

    neuralNameLabel.setText("No neural model", juce::dontSendNotification);
    neuralNameLabel.setJustificationType(juce::Justification::centredLeft);
    neuralNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff555250));
    neuralNameLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    addAndMakeVisible(neuralNameLabel);

    // ── EQ panel ──
    eqPanel.onEqChanged = [this] { syncParams(); };
    addAndMakeVisible(eqPanel);

    // ── Gate/Comp widget ──
    gateCompWidget = std::make_unique<GateCompWidget>(processor);
    addAndMakeVisible(*gateCompWidget);

    // ── Default preset ──
    applyPreset(kPresets[3]);   // HYBRID

    setAudioChannels(2, 2);
    setSize(960, 534);
}

MainComponent::~MainComponent()
{
    setLookAndFeel(nullptr);
    shutdownAudio();
}

// ── Knob helper ────────────────────────────────────────────────────────────

juce::String MainComponent::formatKnobValue(juce::Slider& k) const
{
    if (&k == &transposeKnob)
    {
        const int v = static_cast<int>(std::round(k.getValue()));
        return v == 0 ? "0 ST" : (v > 0 ? "+" : "") + juce::String(v) + " ST";
    }
    const float v = static_cast<float>(k.getValue());
    if (v < 0.005f) return "0";
    return juce::String(v, 2);
}

void MainComponent::registerKnob(juce::Slider& k, juce::Label& l)
{
    knobLabelMap[&k] = { &l, {}, false };
}

void MainComponent::setupKnob(juce::Slider& k, juce::Label& l,
                               const juce::String& name, double defaultVal)
{
    k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k.setRange(0.0, 1.0, 0.001);
    k.setDoubleClickReturnValue(true, defaultVal);
    k.addListener(this);
    addAndMakeVisible(k);

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);

    registerKnob(k, l);
}

void MainComponent::styleChannelButton(juce::TextButton& btn, const juce::String& label)
{
    btn.setButtonText(label);
    btn.setComponentID("channel");
    btn.setClickingTogglesState(false);
    btn.addListener(this);
    addAndMakeVisible(btn);
}

void MainComponent::stylePresetButton(juce::TextButton& btn, const juce::String& label)
{
    btn.setButtonText(label);
    btn.setComponentID("preset");
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
    auto* buf       = info.buffer;
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

void MainComponent::sliderValueChanged(juce::Slider* s)
{
    // During drag: update label with live value
    auto it = knobLabelMap.find(s);
    if (it != knobLabelMap.end() && it->second.dragging && it->second.label)
        it->second.label->setText(formatKnobValue(*s), juce::dontSendNotification);

    if (s == &transposeKnob && !(it != knobLabelMap.end() && it->second.dragging))
    {
        const int v = static_cast<int>(std::round(transposeKnob.getValue()));
        transposeLabel.setText(v == 0 ? "XPOSE"
            : (v > 0 ? "+" : "") + juce::String(v) + " ST",
            juce::dontSendNotification);
    }
    if (currentPresetIdx != -1) { currentPresetIdx = -1; updatePresetButtonStates(); }
    syncParams();
}

void MainComponent::sliderDragStarted(juce::Slider* s)
{
    auto it = knobLabelMap.find(s);
    if (it != knobLabelMap.end() && it->second.label)
    {
        it->second.savedText = it->second.label->getText();
        it->second.dragging  = true;
        it->second.label->setText(formatKnobValue(*s), juce::dontSendNotification);
    }
}

void MainComponent::sliderDragEnded(juce::Slider* s)
{
    auto it = knobLabelMap.find(s);
    if (it != knobLabelMap.end() && it->second.label)
    {
        it->second.dragging = false;
        it->second.label->setText(it->second.savedText, juce::dontSendNotification);
    }
    // Special case: transpose label shows semitones when not dragging
    if (s == &transposeKnob)
    {
        const int v = static_cast<int>(std::round(transposeKnob.getValue()));
        transposeLabel.setText(v == 0 ? "XPOSE"
            : (v > 0 ? "+" : "") + juce::String(v) + " ST",
            juce::dontSendNotification);
    }
}

void MainComponent::buttonClicked(juce::Button* btn)
{
    // Channel
    if (btn == &chClean)  { currentChannel = AmpProcessor::Channel::Clean;  currentPresetIdx = -1; updatePresetButtonStates(); updateChannelButtonStates(); updateNeuralLabel(); syncParams(); return; }
    if (btn == &chCrunch) { currentChannel = AmpProcessor::Channel::Crunch; currentPresetIdx = -1; updatePresetButtonStates(); updateChannelButtonStates(); updateNeuralLabel(); syncParams(); return; }
    if (btn == &chLead)   { currentChannel = AmpProcessor::Channel::Lead;   currentPresetIdx = -1; updatePresetButtonStates(); updateChannelButtonStates(); updateNeuralLabel(); syncParams(); return; }
    if (btn == &boostBtn) { currentPresetIdx = -1; updatePresetButtonStates(); syncParams(); return; }

    // Presets
    if (btn == &preDeftones) { applyPreset(kPresets[0]); return; }
    if (btn == &preKnocked)  { applyPreset(kPresets[1]); return; }
    if (btn == &preBilmuri)  { applyPreset(kPresets[2]); return; }
    if (btn == &preHybrid)   { applyPreset(kPresets[3]); return; }
    if (btn == &preHotMul)   { applyPreset(kPresets[4]); return; }

    // IR
    // Neural model loading (applies to the currently-selected channel)
    if (btn == &neuralLoadBtn)  { openNeuralModelChooser(); return; }
    if (btn == &neuralClearBtn) {
        processor.clearNeuralModel(currentChannel);
        updateNeuralLabel();
        return;
    }

    if (btn == &irLoadBtn)  { openIRChooser(); return; }
    if (btn == &irClearBtn) {
        processor.clearIR();
        irNameLabel.setText("No IR loaded", juce::dontSendNotification);
        irNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666666));
        return;
    }

    // Header
    if (btn == &audioBtn)  { openAudioSettings(); return; }
    if (btn == &tunerBtn)  {
        tuner.setVisible(tunerBtn.getToggleState());
        return;
    }
    if (btn == &savePresetBtn) { savePreset(); return; }
    if (btn == &loadPresetBtn) { loadPreset(); return; }

    // Tap tempo
    if (btn == &tapBtn) { handleTap(); return; }

    // Delay subdivisions
    if (btn == &subdivQ)  { currentSubdivMult = 1.0f;   subdivQ.setToggleState(true,  juce::dontSendNotification); subdivD8.setToggleState(false, juce::dontSendNotification); subdiv8.setToggleState(false, juce::dontSendNotification); syncParams(); return; }
    if (btn == &subdivD8) { currentSubdivMult = 0.75f;  subdivQ.setToggleState(false, juce::dontSendNotification); subdivD8.setToggleState(true,  juce::dontSendNotification); subdiv8.setToggleState(false, juce::dontSendNotification); syncParams(); return; }
    if (btn == &subdiv8)  { currentSubdivMult = 0.5f;   subdivQ.setToggleState(false, juce::dontSendNotification); subdivD8.setToggleState(false, juce::dontSendNotification); subdiv8.setToggleState(true,  juce::dontSendNotification); syncParams(); return; }

    // Cabinet character
    auto selectCab = [this](AmpProcessor::CabType t) {
        currentCabType = t;
        cabV30 .setToggleState(t == AmpProcessor::CabType::V30,       juce::dontSendNotification);
        cabG12 .setToggleState(t == AmpProcessor::CabType::G12T75,    juce::dontSendNotification);
        cabGrbn.setToggleState(t == AmpProcessor::CabType::Greenback, juce::dontSendNotification);
        cabOpen.setToggleState(t == AmpProcessor::CabType::Open,      juce::dontSendNotification);
        syncParams();
    };
    if (btn == &cabV30)  { selectCab(AmpProcessor::CabType::V30);       return; }
    if (btn == &cabG12)  { selectCab(AmpProcessor::CabType::G12T75);    return; }
    if (btn == &cabGrbn) { selectCab(AmpProcessor::CabType::Greenback); return; }
    if (btn == &cabOpen) { selectCab(AmpProcessor::CabType::Open);      return; }
}

void MainComponent::handleTap()
{
    const juce::int64 now = juce::Time::currentTimeMillis();

    // Shift-register: newest tap always lands at index 3 in chronological order.
    // This keeps linear-pair iteration correct regardless of how many taps have fired.
    tapHistory[0] = tapHistory[1];
    tapHistory[1] = tapHistory[2];
    tapHistory[2] = tapHistory[3];
    tapHistory[3] = now;
    if (tapHistoryIdx < 4) ++tapHistoryIdx;

    if (tapHistoryIdx < 2) return;

    // Average up to 3 consecutive inter-tap intervals (last 4 taps)
    float totalMs = 0.0f;
    int   count   = 0;
    const int startIdx = 4 - tapHistoryIdx;   // first valid slot
    for (int i = startIdx; i < 3; ++i)
    {
        if (tapHistory[i] > 0 && tapHistory[i + 1] > tapHistory[i])
        {
            const float ms = static_cast<float>(tapHistory[i + 1] - tapHistory[i]);
            if (ms > 100.0f && ms < 3000.0f)   // 20–600 BPM guard
            {
                totalMs += ms;
                ++count;
            }
        }
    }

    if (count == 0) return;

    currentDelayMs = totalMs / static_cast<float>(count);
    const int bpm = static_cast<int>(std::round(60000.0f / currentDelayMs));
    bpmLabel.setText(juce::String(bpm) + " BPM", juce::dontSendNotification);
    syncParams();
}

void MainComponent::updateChannelButtonStates()
{
    chClean .setToggleState(currentChannel == AmpProcessor::Channel::Clean,  juce::dontSendNotification);
    chCrunch.setToggleState(currentChannel == AmpProcessor::Channel::Crunch, juce::dontSendNotification);
    chLead  .setToggleState(currentChannel == AmpProcessor::Channel::Lead,   juce::dontSendNotification);
    repaint();
}

void MainComponent::updatePresetButtonStates()
{
    preDeftones.setToggleState(currentPresetIdx == 0, juce::dontSendNotification);
    preKnocked .setToggleState(currentPresetIdx == 1, juce::dontSendNotification);
    preBilmuri .setToggleState(currentPresetIdx == 2, juce::dontSendNotification);
    preHybrid  .setToggleState(currentPresetIdx == 3, juce::dontSendNotification);
    preHotMul  .setToggleState(currentPresetIdx == 4, juce::dontSendNotification);
}

void MainComponent::applyPreset(const Preset& p)
{
    currentPresetIdx = -1;
    for (int i = 0; i < (int)std::size(kPresets); ++i)
        if (&p == &kPresets[i]) { currentPresetIdx = i; break; }
    updatePresetButtonStates();

    currentChannel = p.channel;
    updateChannelButtonStates();
    boostBtn.setToggleState(p.boost, juce::dontSendNotification);

    compKnob    .setValue(p.comp,     juce::dontSendNotification);
    gateKnob    .setValue(p.gate,     juce::dontSendNotification);
    gainKnob    .setValue(p.gain,     juce::dontSendNotification);
    bassKnob    .setValue(p.bass,     juce::dontSendNotification);
    midKnob     .setValue(p.mid,      juce::dontSendNotification);
    trebleKnob  .setValue(p.treble,   juce::dontSendNotification);
    presenceKnob.setValue(p.presence, juce::dontSendNotification);
    volumeKnob  .setValue(p.volume,   juce::dontSendNotification);
    sagKnob     .setValue(p.sag,      juce::dontSendNotification);
    tremKnob    .setValue(p.trem,     juce::dontSendNotification);
    reverbKnob  .setValue(p.reverb,   juce::dontSendNotification);
    chorusKnob  .setValue(p.chorus,   juce::dontSendNotification);
    delayKnob   .setValue(p.delay,    juce::dontSendNotification);
    widthKnob   .setValue(p.width,    juce::dontSendNotification);
    phaserKnob  .setValue(p.phaser,   juce::dontSendNotification);
    flangerKnob .setValue(p.flanger,  juce::dontSendNotification);
    transposeKnob.setValue(p.transpose, juce::dontSendNotification);

    currentCabType = p.cabType;
    cabV30 .setToggleState(p.cabType == AmpProcessor::CabType::V30,       juce::dontSendNotification);
    cabG12 .setToggleState(p.cabType == AmpProcessor::CabType::G12T75,    juce::dontSendNotification);
    cabGrbn.setToggleState(p.cabType == AmpProcessor::CabType::Greenback, juce::dontSendNotification);
    cabOpen.setToggleState(p.cabType == AmpProcessor::CabType::Open,      juce::dontSendNotification);
    transposeLabel.setText(p.transpose == 0 ? "XPOSE"
        : (p.transpose > 0 ? "+" : "") + juce::String(p.transpose) + " ST",
        juce::dontSendNotification);

    currentDelayMs = p.delayTimeMs;
    const int bpm = static_cast<int>(std::round(60000.0f / currentDelayMs));
    bpmLabel.setText(juce::String(bpm) + " BPM", juce::dontSendNotification);

    syncParams();
    repaint();
}

void MainComponent::syncParams()
{
    AmpProcessor::Params p;
    p.channel     = currentChannel;
    p.boost       = boostBtn.getToggleState();
    p.comp        = static_cast<float>(compKnob.getValue());
    p.gate        = static_cast<float>(gateKnob.getValue());
    p.gain        = static_cast<float>(gainKnob.getValue());
    p.bass        = static_cast<float>(bassKnob.getValue());
    p.mid         = static_cast<float>(midKnob.getValue());
    p.treble      = static_cast<float>(trebleKnob.getValue());
    p.presence    = static_cast<float>(presenceKnob.getValue());
    p.volume      = static_cast<float>(volumeKnob.getValue());
    p.sag         = static_cast<float>(sagKnob.getValue());
    p.trem        = static_cast<float>(tremKnob.getValue());
    p.reverb      = static_cast<float>(reverbKnob.getValue());
    p.chorus      = static_cast<float>(chorusKnob.getValue());
    p.delay       = static_cast<float>(delayKnob.getValue());
    p.delayTimeMs = currentDelayMs * currentSubdivMult;
    p.width       = static_cast<float>(widthKnob.getValue());
    p.phaser      = static_cast<float>(phaserKnob.getValue());
    p.flanger     = static_cast<float>(flangerKnob.getValue());
    p.cabType     = currentCabType;
    p.transpose   = static_cast<int>(std::round(transposeKnob.getValue()));
    // Post-amp EQ bands from EqPanel
    const auto* eb = eqPanel.getBands();
    for (int b = 0; b < 4; ++b) p.eq[b] = eb[b];
    processor.setParams(p);
}

// ── IR / Audio ─────────────────────────────────────────────────────────────

void MainComponent::openAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent(
        deviceManager, 1, 2, 1, 2, false, false, false, false);
    sel->setSize(500, 340);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(sel);
    o.dialogTitle             = "Audio Settings";
    o.dialogBackgroundColour  = juce::Colour(0xff1a1a1a);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar       = true;
    o.resizable               = false;
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

// ── Neural model ───────────────────────────────────────────────────────────

void MainComponent::updateNeuralLabel()
{
    const auto name = processor.getNeuralModelName(currentChannel);
    if (name.isEmpty())
    {
        neuralNameLabel.setText("No neural model", juce::dontSendNotification);
        neuralNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff555250));
    }
    else
    {
        neuralNameLabel.setText(name, juce::dontSendNotification);
        neuralNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88ee88));
    }
}

void MainComponent::openNeuralModelChooser()
{
    const auto modelsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               .getChildFile("UMBRA Models");
    modelsDir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Neural Amp Model (LSTM .json)",
        modelsDir.exists() ? modelsDir : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.json");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto results = fc.getResults();
            if (results.isEmpty()) return;

            // Loading can be slow for large models — run on background thread
            const auto file   = results[0];
            const auto ch     = currentChannel;
            juce::Thread::launch([this, file, ch]
            {
                const bool ok = processor.loadNeuralModel(file, ch);
                juce::MessageManager::callAsync([this, ok, ch]
                {
                    if (ok && ch == currentChannel)
                        updateNeuralLabel();
                    else if (!ok)
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon, "Neural Model",
                            "Could not load this model.\n"
                            "Supported: LSTM hidden_size 8/16/20/24/32/40 (GuitarML / NeuralPi / Proteus format).");
                });
            });
        });
}

// ── Preset save / load ─────────────────────────────────────────────────────

void MainComponent::savePreset()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.umbra");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto results = fc.getResults();
            if (results.isEmpty()) return;

            auto file = results[0].withFileExtension(".umbra");

            juce::DynamicObject::Ptr obj (new juce::DynamicObject());
            const auto ch = currentChannel;
            obj->setProperty("channel",     ch == AmpProcessor::Channel::Clean  ? "Clean"
                                          : ch == AmpProcessor::Channel::Crunch ? "Crunch"
                                                                                : "Lead");
            obj->setProperty("boost",       boostBtn.getToggleState());
            obj->setProperty("comp",        compKnob.getValue());
            obj->setProperty("gate",        gateKnob.getValue());
            obj->setProperty("gain",        gainKnob.getValue());
            obj->setProperty("bass",        bassKnob.getValue());
            obj->setProperty("mid",         midKnob.getValue());
            obj->setProperty("treble",      trebleKnob.getValue());
            obj->setProperty("presence",    presenceKnob.getValue());
            obj->setProperty("volume",      volumeKnob.getValue());
            obj->setProperty("sag",         sagKnob.getValue());
            obj->setProperty("trem",        tremKnob.getValue());
            obj->setProperty("reverb",      reverbKnob.getValue());
            obj->setProperty("chorus",      chorusKnob.getValue());
            obj->setProperty("delay",       delayKnob.getValue());
            obj->setProperty("delayTimeMs", static_cast<double>(currentDelayMs));
            obj->setProperty("subdivMult",  static_cast<double>(currentSubdivMult));
            obj->setProperty("transpose",   static_cast<int>(std::round(transposeKnob.getValue())));
            obj->setProperty("width",       widthKnob.getValue());
            const auto ct = currentCabType;
            obj->setProperty("cabType", ct == AmpProcessor::CabType::G12T75    ? "G12T75"
                                      : ct == AmpProcessor::CabType::Greenback ? "Greenback"
                                      : ct == AmpProcessor::CabType::Open      ? "Open"
                                                                                : "V30");

            file.replaceWithText(juce::JSON::toString(juce::var(obj.get()), true));
        });
}

void MainComponent::loadPreset()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.umbra");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto results = fc.getResults();
            if (results.isEmpty()) return;

            const auto parsed = juce::JSON::parse(results[0].loadFileAsString());
            const auto* obj   = parsed.getDynamicObject();
            if (obj == nullptr) return;

            auto getF = [&](const char* id, float def) {
                const auto v = obj->getProperty(id);
                return v.isUndefined() ? def : static_cast<float>(static_cast<double>(v));
            };
            auto getI = [&](const char* id, int def) {
                const auto v = obj->getProperty(id);
                return v.isUndefined() ? def : static_cast<int>(v);
            };
            auto getB = [&](const char* id, bool def) {
                const auto v = obj->getProperty(id);
                return v.isUndefined() ? def : static_cast<bool>(v);
            };

            const juce::String chStr = obj->getProperty("channel").toString();
            currentChannel = (chStr == "Clean")  ? AmpProcessor::Channel::Clean
                           : (chStr == "Crunch") ? AmpProcessor::Channel::Crunch
                                                 : AmpProcessor::Channel::Lead;
            updateChannelButtonStates();
            boostBtn.setToggleState(getB("boost", false), juce::dontSendNotification);

            compKnob    .setValue(juce::jlimit(0.0f,1.0f,getF("comp",     0.0f)), juce::dontSendNotification);
            gateKnob    .setValue(juce::jlimit(0.0f,1.0f,getF("gate",     0.0f)), juce::dontSendNotification);
            gainKnob    .setValue(juce::jlimit(0.0f,1.0f,getF("gain",     0.5f)), juce::dontSendNotification);
            bassKnob    .setValue(juce::jlimit(0.0f,1.0f,getF("bass",     0.5f)), juce::dontSendNotification);
            midKnob     .setValue(juce::jlimit(0.0f,1.0f,getF("mid",      0.5f)), juce::dontSendNotification);
            trebleKnob  .setValue(juce::jlimit(0.0f,1.0f,getF("treble",   0.5f)), juce::dontSendNotification);
            presenceKnob.setValue(juce::jlimit(0.0f,1.0f,getF("presence", 0.5f)), juce::dontSendNotification);
            volumeKnob  .setValue(juce::jlimit(0.0f,1.0f,getF("volume",   0.5f)), juce::dontSendNotification);
            sagKnob     .setValue(juce::jlimit(0.0f,1.0f,getF("sag",      0.0f)), juce::dontSendNotification);
            tremKnob    .setValue(juce::jlimit(0.0f,1.0f,getF("trem",     0.0f)), juce::dontSendNotification);
            reverbKnob  .setValue(juce::jlimit(0.0f,1.0f,getF("reverb",   0.0f)), juce::dontSendNotification);
            chorusKnob  .setValue(juce::jlimit(0.0f,1.0f,getF("chorus",   0.0f)), juce::dontSendNotification);
            delayKnob   .setValue(juce::jlimit(0.0f,1.0f,getF("delay",    0.0f)), juce::dontSendNotification);
            transposeKnob.setValue(juce::jlimit(-12.0,12.0,(double)getI("transpose",0)), juce::dontSendNotification);

            currentDelayMs    = getF("delayTimeMs", 350.0f);
            currentSubdivMult = getF("subdivMult",  1.0f);
            subdivQ .setToggleState(currentSubdivMult > 0.9f,                                              juce::dontSendNotification);
            subdivD8.setToggleState(currentSubdivMult > 0.6f && currentSubdivMult < 0.9f,                 juce::dontSendNotification);
            subdiv8 .setToggleState(currentSubdivMult < 0.6f,                                             juce::dontSendNotification);
            const int bpm = static_cast<int>(std::round(60000.0f / currentDelayMs));
            bpmLabel.setText(juce::String(bpm) + " BPM", juce::dontSendNotification);

            widthKnob.setValue(juce::jlimit(0.0f, 1.0f, getF("width", 0.5f)), juce::dontSendNotification);

            const juce::String cabStr = obj->getProperty("cabType").toString();
            currentCabType = (cabStr == "G12T75")    ? AmpProcessor::CabType::G12T75
                           : (cabStr == "Greenback") ? AmpProcessor::CabType::Greenback
                           : (cabStr == "Open")      ? AmpProcessor::CabType::Open
                                                     : AmpProcessor::CabType::V30;
            cabV30 .setToggleState(currentCabType == AmpProcessor::CabType::V30,       juce::dontSendNotification);
            cabG12 .setToggleState(currentCabType == AmpProcessor::CabType::G12T75,    juce::dontSendNotification);
            cabGrbn.setToggleState(currentCabType == AmpProcessor::CabType::Greenback, juce::dontSendNotification);
            cabOpen.setToggleState(currentCabType == AmpProcessor::CabType::Open,      juce::dontSendNotification);

            currentPresetIdx = -1;
            updatePresetButtonStates();
            syncParams();
            repaint();
        });
}

// ── Paint ──────────────────────────────────────────────────────────────────

void MainComponent::paint(juce::Graphics& g)
{
    const int W = getWidth();
    const int H = getHeight();

    // Background
    g.fillAll(juce::Colour(0xff0d0b09));

    // Header gradient
    {
        juce::ColourGradient hg(juce::Colour(0xff201e1c), 0.0f, 0.0f,
                                juce::Colour(0xff0e0c0b), 0.0f, 48.0f, false);
        g.setGradientFill(hg);
        g.fillRect(0, 0, W, 48);
    }
    g.setColour(kAccent);
    g.fillRect(0.0f, 46.0f, (float)W, 2.0f);

    // UMBRA wordmark
    g.setFont(juce::Font(juce::FontOptions().withHeight(26.0f).withStyle("Bold")));
    g.setColour(kAccentHi);
    g.drawText("U", 18, 0, 18, 46, juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xfff0eee8));
    g.drawText("MBRA", 34, 0, 76, 46, juce::Justification::centredLeft, false);
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
    g.setColour(juce::Colour(0xff504844));
    g.drawText("AMP SIMULATOR", 18, 32, 120, 13, juce::Justification::centredLeft, false);

    // Preset row
    g.setColour(juce::Colour(0xff0e0c0b));
    g.fillRect(0, 48, W, 38);
    g.setColour(juce::Colour(0xff282420));
    g.fillRect(0, 85, W, 1);

    // Knob panel
    const auto panelBounds = juce::Rectangle<int>(8, 92, W - 16, H - 100).toFloat();
    g.setColour(kPanel);
    g.fillRoundedRectangle(panelBounds, 6.0f);
    g.setColour(juce::Colour(0xff242220));
    g.drawLine(panelBounds.getX() + 6, panelBounds.getY() + 1,
               panelBounds.getRight() - 6, panelBounds.getY() + 1, 1.0f);
    g.setColour(juce::Colour(0xff2a2724));
    g.drawRoundedRectangle(panelBounds, 6.0f, 1.0f);

    // Section labels + separators
    // Layout mirrors resized(): input meter 22px + 4px gap on left,
    // output meter 24px + 14px gap on right, 14 equal-width knob slots in between.
    {
        constexpr int kInMW    = 22;
        constexpr int kInMGap  = 4;
        constexpr int kOutMW   = 24;
        constexpr int kOutMGap = 14;
        // Input meter starts at x=12 (4px inside panel at x=8)
        const int kAreaLeft  = 12 + kInMW + kInMGap;       // 38 — must match resized()
        const int kAreaRight = W - kOutMW - kOutMGap;       // W - 38
        const int kAreaW     = kAreaRight - kAreaLeft;
        const int numKnobs   = 17;
        const int slotW      = kAreaW / numKnobs;
        const int panelTop   = 92;
        const int panelH     = H - 100;

        // Sections: INPUT(0-3), TONE(4-7), OUTPUT(8-10), FX(11-16)
        struct Sec { int start, count; const char* name; } secs[] = {
            { 0, 4, "INPUT" }, { 4, 4, "TONE" }, { 8, 3, "OUTPUT" }, { 11, 6, "FX" }
        };

        g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));

        for (auto& s : secs)
        {
            const int sx = kAreaLeft + s.start * slotW;
            const int sw = s.count * slotW;

            g.setColour(juce::Colour(0xff484240));
            g.drawText(s.name, sx, panelTop + 6, sw, 13, juce::Justification::centred, false);

            if (s.start + s.count < numKnobs)
            {
                const int sepX = kAreaLeft + (s.start + s.count) * slotW;
                g.setColour(juce::Colour(0xff0a0908));
                g.fillRect(sepX - 1, panelTop + 20, 2, panelH - 26);
                g.setColour(juce::Colour(0xff222220));
                g.fillRect(sepX, panelTop + 20, 1, panelH - 26);
            }
        }
    }

    // "CHANNEL" micro-label
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
    g.setColour(juce::Colour(0xff3a3632));
    g.drawText("CHANNEL", 178, 1, 230, 10, juce::Justification::centred, false);
}

// ── Resized ────────────────────────────────────────────────────────────────

void MainComponent::resized()
{
    const int W = getWidth();
    const int H = getHeight();

    // ── Header (y=0, h=48) ──────────────────────────────────────────────
    // SAVE / LOAD preset — left of channel buttons
    savePresetBtn.setBounds(112, 12, 42, 24);
    loadPresetBtn.setBounds(156, 12, 42, 24);

    // Channel buttons
    const int chW = 68, chH = 26, chY = 11;
    const int chStart = 206;
    chClean .setBounds(chStart,            chY, chW, chH);
    chCrunch.setBounds(chStart + chW + 4,  chY, chW, chH);
    chLead  .setBounds(chStart + chW*2+8,  chY, chW, chH);
    boostBtn.setBounds(chStart + chW*3+16, chY, 46,  chH);

    // Neural model loader — right after 808 button, compact
    {
        const int nX = chStart + chW*3 + 16 + 46 + 8;
        neuralLoadBtn .setBounds(nX,      11, 62, 26);
        neuralClearBtn.setBounds(nX + 65, 11, 22, 26);
        neuralNameLabel.setBounds(nX + 90, 11, 160, 26);
    }

    // Cab character buttons — positioned after neural loader
    {
        const int cabStart = chStart + chW*3 + 16 + 46 + 270;
        const int cW = 40, cH = 22, cY = 13;
        cabV30 .setBounds(cabStart,           cY, cW,   cH);
        cabG12 .setBounds(cabStart + cW + 3,  cY, cW,   cH);
        cabGrbn.setBounds(cabStart + cW*2+6,  cY, cW+2, cH);
        cabOpen.setBounds(cabStart + cW*3+12, cY, cW,   cH);
    }

    // TUNER + AUDIO — right of header
    audioBtn .setBounds(W - 70, 11, 60, 26);
    tunerBtn .setBounds(W - 70 - 62, 11, 58, 26);

    // ── Preset / IR row (y=48, h=38) ────────────────────────────────────
    const int rowY = 50, rowH = 30;
    const int pW = 80, pGap = 3;
    preDeftones.setBounds(8,                   rowY, pW, rowH);
    preKnocked .setBounds(8 + (pW+pGap),       rowY, pW, rowH);
    preBilmuri .setBounds(8 + (pW+pGap)*2,     rowY, pW, rowH);
    preHybrid  .setBounds(8 + (pW+pGap)*3,     rowY, pW, rowH);
    preHotMul  .setBounds(8 + (pW+pGap)*4,     rowY, pW, rowH);

    // TAP tempo + subdivision buttons — after presets
    const int afterPresets = 8 + (pW+pGap)*5 + 4;
    tapBtn  .setBounds(afterPresets,       rowY, 40, rowH);
    // Subdivision note-value buttons (small)
    const int sdY = rowY + 4, sdH = rowH - 8;
    subdivQ .setBounds(afterPresets + 44,      sdY, 34, sdH);
    subdivD8.setBounds(afterPresets + 44 + 37, sdY, 38, sdH);
    subdiv8 .setBounds(afterPresets + 44 + 79, sdY, 34, sdH);
    bpmLabel.setBounds(afterPresets + 44 + 116, rowY, 62, rowH);

    // IR — right-aligned
    const int irClearW = 26, irLoadW = 72;
    const int irRight  = W - 10;
    irClearBtn .setBounds(irRight - irClearW,               rowY, irClearW, rowH);
    irLoadBtn  .setBounds(irRight - irClearW - irLoadW - 4, rowY, irLoadW,  rowH);
    // IR name label: starts after BPM readout, ends before LOAD IR button
    const int irLabelX = afterPresets + 44 + 116 + 64;
    irNameLabel.setBounds(irLabelX, rowY,
                          irRight - irClearW - irLoadW - 4 - irLabelX - 6, rowH);

    // ── Knob + meter area (y=92 downward) ───────────────────────────────
    const int panelTop  = 92;
    const int panelH    = H - panelTop - 8;
    const int secLabelH = 22;

    // Meters: IN on left, OUT on right
    constexpr int kInMW   = 22;
    constexpr int kInMGap = 4;
    constexpr int kOutMW  = 24;
    constexpr int kOutMGap = 14;
    const int inMeterX  = 12;
    const int outMeterX = W - kOutMW - kOutMGap;
    inputMeter .setBounds(inMeterX,  panelTop + 8, kInMW,  panelH - 14);
    outputMeter.setBounds(outMeterX, panelTop + 8, kOutMW, panelH - 14);
    // Gate/Comp widget: 18px wide, to the left of the first knob
    if (gateCompWidget)
        gateCompWidget->setBounds(inMeterX + kInMW + 2, panelTop + 8, 16, panelH - 14);

    // Tuner overlay — covers entire knob panel
    tuner.setBounds(8, panelTop, W - 16, panelH);

    // Knob slots
    const int kAreaLeft = inMeterX + kInMW + kInMGap;   // 38
    const int kAreaW    = outMeterX - kAreaLeft;
    const int numKnobs  = 17;
    const int slotW     = kAreaW / numKnobs;
    const int knobSize  = juce::jmin(panelH - secLabelH - 34, slotW - 6);
    const int labelH    = 14;
    const int knobY     = panelTop + secLabelH
                          + (panelH - secLabelH - knobSize - labelH) / 2;

    auto place = [&](juce::Slider& k, juce::Label& l, int idx)
    {
        const int kx = kAreaLeft + idx * slotW + (slotW - knobSize) / 2;
        k.setBounds(kx, knobY, knobSize, knobSize);
        l.setBounds(kx - 4, knobY + knobSize + 2, knobSize + 8, labelH);
    };

    // INPUT: COMP(0) GATE(1) GAIN(2) XPOSE(3)
    place(compKnob,      compLabel,      0);
    place(gateKnob,      gateLabel,      1);
    place(gainKnob,      gainLabel,      2);
    place(transposeKnob, transposeLabel, 3);
    // TONE: BASS(4) MID(5) TREBLE(6) PRESENCE(7)
    place(bassKnob,      bassLabel,      4);
    place(midKnob,       midLabel,       5);
    place(trebleKnob,    trebleLabel,    6);
    place(presenceKnob,  presenceLabel,  7);
    // OUTPUT: VOLUME(8) SAG(9) TREM(10)
    place(volumeKnob,    volumeLabel,    8);
    place(sagKnob,       sagLabel,       9);
    place(tremKnob,      tremLabel,      10);
    // FX: REVERB(11) CHORUS(12) DELAY(13) WIDTH(14) PHASE(15) FLANGE(16)
    place(reverbKnob,    reverbLabel,    11);
    place(chorusKnob,    chorusLabel,    12);
    place(delayKnob,     delayLabel,     13);
    place(widthKnob,     widthLabel,     14);
    place(phaserKnob,    phaserLabel,    15);
    place(flangerKnob,   flangerLabel,   16);

    // ── EQ panel (below knob panel, fixed height 170px) ──────────────────
    const int eqTop = panelTop + panelH + 8;
    const int eqH   = H - eqTop - 6;
    eqPanel.setBounds(8, eqTop, W - 16, std::max(10, eqH));
}
