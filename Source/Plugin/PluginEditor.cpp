#include "PluginEditor.h"

// ── Helper: get readable value string for a knob ───────────────────────────

juce::String UmbraPluginEditor::formatKnobValue(juce::Slider& k) const
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

// ── Knob / label registration ─────────────────────────────────────────────

void UmbraPluginEditor::registerKnobLabel(juce::Slider& k, juce::Label& l)
{
    knobLabelMap[&k] = { &l, {} };
}

// ── Constructor ────────────────────────────────────────────────────────────

UmbraPluginEditor::UmbraPluginEditor(UmbraProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&laf);

    auto setupCh = [this](juce::TextButton& btn, const juce::String& lbl) {
        btn.setButtonText(lbl); btn.setComponentID("channel");
        btn.setClickingTogglesState(false); btn.addListener(this); addAndMakeVisible(btn);
    };
    setupCh(chClean, "CLEAN"); setupCh(chCrunch, "CRUNCH"); setupCh(chLead, "LEAD");
    chLead.setToggleState(true, juce::dontSendNotification);

    boostBtn.setClickingTogglesState(true); boostBtn.addListener(this); addAndMakeVisible(boostBtn);

    auto setupCab = [this](juce::TextButton& btn) {
        btn.setClickingTogglesState(false); btn.addListener(this);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a));
        addAndMakeVisible(btn);
    };
    setupCab(cabV30); cabV30.setToggleState(true, juce::dontSendNotification);
    setupCab(cabG12); setupCab(cabGrbn); setupCab(cabOpen);

    auto setupKnobFull = [this](juce::Slider& k, juce::Label& l,
                                const juce::String& n, double def) {
        setupKnob(k, l, n, def);
        registerKnobLabel(k, l);
    };

    setupKnobFull(compKnob,     compLabel,    "COMP",    0.0);
    setupKnobFull(gateKnob,     gateLabel,    "GATE",    0.0);
    setupKnobFull(gainKnob,     gainLabel,    "GAIN",    0.5);
    transposeKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    transposeKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    transposeKnob.setRange(-12.0, 12.0, 1.0);
    transposeKnob.setValue(0.0, juce::dontSendNotification);
    transposeKnob.setDoubleClickReturnValue(true, 0.0);
    transposeKnob.addListener(this); addAndMakeVisible(transposeKnob);
    transposeLabel.setText("XPOSE", juce::dontSendNotification);
    transposeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(transposeLabel);
    registerKnobLabel(transposeKnob, transposeLabel);

    setupKnobFull(bassKnob,     bassLabel,    "BASS",    0.5);
    setupKnobFull(midKnob,      midLabel,     "MID",     0.5);
    setupKnobFull(trebleKnob,   trebleLabel,  "TREBLE",  0.5);
    setupKnobFull(presenceKnob, presenceLabel,"PRESENCE",0.5);
    setupKnobFull(volumeKnob,   volumeLabel,  "VOLUME",  0.5);
    setupKnobFull(sagKnob,      sagLabel,     "SAG",     0.0);
    setupKnobFull(tremKnob,     tremLabel,    "TREM",    0.0);
    setupKnobFull(reverbKnob,   reverbLabel,  "REVERB",  0.0);
    setupKnobFull(chorusKnob,   chorusLabel,  "CHORUS",  0.0);
    setupKnobFull(delayKnob,    delayLabel,   "DELAY",   0.0);
    setupKnobFull(widthKnob,    widthLabel,   "WIDTH",   0.5);
    setupKnobFull(phaserKnob,   phaserLabel,  "PHASE",   0.0);
    setupKnobFull(flangerKnob,  flangerLabel, "FLANGE",  0.0);

    irLoadBtn.addListener(this); irLoadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a)); addAndMakeVisible(irLoadBtn);
    irClearBtn.addListener(this); irClearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a)); addAndMakeVisible(irClearBtn);
    irNameLabel.setText("No IR loaded", juce::dontSendNotification);
    irNameLabel.setJustificationType(juce::Justification::centredLeft);
    irNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666666));
    irNameLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.5f)));
    addAndMakeVisible(irNameLabel);

    tapBtn.addListener(this); tapBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a)); addAndMakeVisible(tapBtn);
    subdivQ .addListener(this); subdivQ .setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a)); addAndMakeVisible(subdivQ);
    subdivD8.addListener(this); subdivD8.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a)); addAndMakeVisible(subdivD8);
    subdiv8 .addListener(this); subdiv8 .setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a1a)); addAndMakeVisible(subdiv8);
    subdivQ.setToggleState(true, juce::dontSendNotification);
    bpmLabel.setText("-- BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xff555250));
    bpmLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    addAndMakeVisible(bpmLabel);

    eqPanel.onEqChanged = [this] { syncParams(); };
    addAndMakeVisible(eqPanel);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    setResizable(true, false);
    setResizeLimits(800, 480, 1600, 800);
    setSize(960, 534);
}

UmbraPluginEditor::~UmbraPluginEditor() { setLookAndFeel(nullptr); }

// ── Knob helper ────────────────────────────────────────────────────────────

void UmbraPluginEditor::setupKnob(juce::Slider& k, juce::Label& l,
                                   const juce::String& n, double def)
{
    k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k.setRange(0.0, 1.0, 0.001);
    k.setDoubleClickReturnValue(true, def);
    k.addListener(this); addAndMakeVisible(k);
    l.setText(n, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

// ── Slider callbacks ───────────────────────────────────────────────────────

void UmbraPluginEditor::sliderValueChanged(juce::Slider* s)
{
    // Update label with current value during drag
    auto it = knobLabelMap.find(s);
    if (it != knobLabelMap.end() && it->second.second.active)
        it->second.first->setText(formatKnobValue(*s), juce::dontSendNotification);

    if (s == &transposeKnob) {
        const int v = static_cast<int>(std::round(transposeKnob.getValue()));
        if (!knobLabelMap[s].second.active)
            transposeLabel.setText(v == 0 ? "XPOSE"
                : (v > 0 ? "+" : "") + juce::String(v) + " ST",
                juce::dontSendNotification);
    }
    syncParams();
}

void UmbraPluginEditor::sliderDragStarted(juce::Slider* s)
{
    auto it = knobLabelMap.find(s);
    if (it != knobLabelMap.end()) {
        it->second.second.savedLabel = it->second.first->getText();
        it->second.second.active = true;
        it->second.first->setText(formatKnobValue(*s), juce::dontSendNotification);
    }
}

void UmbraPluginEditor::sliderDragEnded(juce::Slider* s)
{
    auto it = knobLabelMap.find(s);
    if (it != knobLabelMap.end()) {
        it->second.second.active = false;
        it->second.first->setText(it->second.second.savedLabel, juce::dontSendNotification);
    }
    if (s == &transposeKnob) {
        const int v = static_cast<int>(std::round(transposeKnob.getValue()));
        transposeLabel.setText(v == 0 ? "XPOSE"
            : (v > 0 ? "+" : "") + juce::String(v) + " ST",
            juce::dontSendNotification);
    }
}

void UmbraPluginEditor::buttonClicked(juce::Button* btn)
{
    auto selectCab = [this](AmpProcessor::CabType t) {
        currentCabType = t;
        cabV30 .setToggleState(t == AmpProcessor::CabType::V30,       juce::dontSendNotification);
        cabG12 .setToggleState(t == AmpProcessor::CabType::G12T75,    juce::dontSendNotification);
        cabGrbn.setToggleState(t == AmpProcessor::CabType::Greenback, juce::dontSendNotification);
        cabOpen.setToggleState(t == AmpProcessor::CabType::Open,      juce::dontSendNotification);
        syncParams();
    };

    if (btn == &chClean)  { currentChannel = AmpProcessor::Channel::Clean;  updateChannelButtonStates(); syncParams(); return; }
    if (btn == &chCrunch) { currentChannel = AmpProcessor::Channel::Crunch; updateChannelButtonStates(); syncParams(); return; }
    if (btn == &chLead)   { currentChannel = AmpProcessor::Channel::Lead;   updateChannelButtonStates(); syncParams(); return; }
    if (btn == &boostBtn) { syncParams(); return; }
    if (btn == &cabV30)   { selectCab(AmpProcessor::CabType::V30);       return; }
    if (btn == &cabG12)   { selectCab(AmpProcessor::CabType::G12T75);    return; }
    if (btn == &cabGrbn)  { selectCab(AmpProcessor::CabType::Greenback); return; }
    if (btn == &cabOpen)  { selectCab(AmpProcessor::CabType::Open);      return; }
    if (btn == &irLoadBtn)  { openIRChooser(); return; }
    if (btn == &irClearBtn) { proc.getAmpProcessor().clearIR(); irNameLabel.setText("No IR loaded", juce::dontSendNotification); irNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666666)); return; }
    if (btn == &tapBtn) { handleTap(); return; }
    if (btn == &subdivQ)  { currentSubdivMult = 1.0f;  subdivQ.setToggleState(true, juce::dontSendNotification); subdivD8.setToggleState(false, juce::dontSendNotification); subdiv8.setToggleState(false, juce::dontSendNotification); syncParams(); return; }
    if (btn == &subdivD8) { currentSubdivMult = 0.75f; subdivQ.setToggleState(false, juce::dontSendNotification); subdivD8.setToggleState(true, juce::dontSendNotification); subdiv8.setToggleState(false, juce::dontSendNotification); syncParams(); return; }
    if (btn == &subdiv8)  { currentSubdivMult = 0.5f;  subdivQ.setToggleState(false, juce::dontSendNotification); subdivD8.setToggleState(false, juce::dontSendNotification); subdiv8.setToggleState(true, juce::dontSendNotification); syncParams(); return; }
}

void UmbraPluginEditor::updateChannelButtonStates()
{
    chClean .setToggleState(currentChannel == AmpProcessor::Channel::Clean,  juce::dontSendNotification);
    chCrunch.setToggleState(currentChannel == AmpProcessor::Channel::Crunch, juce::dontSendNotification);
    chLead  .setToggleState(currentChannel == AmpProcessor::Channel::Lead,   juce::dontSendNotification);
    repaint();
}

void UmbraPluginEditor::handleTap()
{
    const juce::int64 now = juce::Time::currentTimeMillis();
    tapHistory[0] = tapHistory[1]; tapHistory[1] = tapHistory[2];
    tapHistory[2] = tapHistory[3]; tapHistory[3] = now;
    if (tapHistoryIdx < 4) ++tapHistoryIdx;
    if (tapHistoryIdx < 2) return;

    float totalMs = 0.f; int count = 0;
    const int startIdx = 4 - tapHistoryIdx;
    for (int i = startIdx; i < 3; ++i) {
        if (tapHistory[i] > 0 && tapHistory[i+1] > tapHistory[i]) {
            const float ms = static_cast<float>(tapHistory[i+1] - tapHistory[i]);
            if (ms > 100.f && ms < 3000.f) { totalMs += ms; ++count; }
        }
    }
    if (count == 0) return;
    currentDelayMs = totalMs / static_cast<float>(count);
    bpmLabel.setText(juce::String(static_cast<int>(std::round(60000.f / currentDelayMs))) + " BPM",
                     juce::dontSendNotification);
    syncParams();
}

void UmbraPluginEditor::syncParams()
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
    const auto* eb = eqPanel.getBands();
    for (int b = 0; b < 4; ++b) p.eq[b] = eb[b];
    proc.pushParams(p);
}

void UmbraPluginEditor::openIRChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Impulse Response",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav;*.aiff;*.aif");
    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            const auto res = fc.getResults();
            if (!res.isEmpty()) {
                proc.getAmpProcessor().loadIR(res[0]);
                irNameLabel.setText(res[0].getFileNameWithoutExtension(), juce::dontSendNotification);
                irNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
            }
        });
}

// ── Paint ──────────────────────────────────────────────────────────────────
// Reuses MainComponent's paint logic verbatim (same colours, same layout).

static const juce::Colour kAccentP    { 0xffcc2200 };
static const juce::Colour kAccentHiP  { 0xffff5522 };
static const juce::Colour kPanelP     { 0xff131110 };

void UmbraPluginEditor::paint(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();
    g.fillAll(juce::Colour(0xff0d0b09));
    { juce::ColourGradient hg(juce::Colour(0xff201e1c), 0.f, 0.f, juce::Colour(0xff0e0c0b), 0.f, 48.f, false); g.setGradientFill(hg); g.fillRect(0, 0, W, 48); }
    g.setColour(kAccentP); g.fillRect(0.f, 46.f, (float)W, 2.f);
    g.setFont(juce::Font(juce::FontOptions().withHeight(26.f).withStyle("Bold")));
    g.setColour(kAccentHiP); g.drawText("U", 18, 0, 18, 46, juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xfff0eee8)); g.drawText("MBRA", 34, 0, 76, 46, juce::Justification::centredLeft, false);
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f))); g.setColour(juce::Colour(0xff504844));
    g.drawText("AMP SIMULATOR", 18, 32, 120, 13, juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xff0e0c0b)); g.fillRect(0, 48, W, 38);
    g.setColour(juce::Colour(0xff282420)); g.fillRect(0, 85, W, 1);
    const auto panelBounds = juce::Rectangle<int>(8, 92, W-16, H - 270).toFloat();
    g.setColour(kPanelP); g.fillRoundedRectangle(panelBounds, 6.f);
    g.setColour(juce::Colour(0xff2a2724)); g.drawRoundedRectangle(panelBounds.reduced(0.5f), 6.f, 1.f);

    // Section labels
    {
        constexpr int kInMW=22, kInMGap=4, kOutMW=24, kOutMGap=14;
        const int kAreaLeft = 12+kInMW+kInMGap, kAreaRight = W-kOutMW-kOutMGap;
        const int kAreaW = kAreaRight-kAreaLeft, numKnobs = 17;
        const int slotW = kAreaW/numKnobs, panelTop = 92, panelH = H-270-92;
        struct Sec{int s,c;const char*n;} secs[]={{0,4,"INPUT"},{4,4,"TONE"},{8,3,"OUTPUT"},{11,6,"FX"}};
        g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
        for(auto& s:secs){
            g.setColour(juce::Colour(0xff484240));
            g.drawText(s.n, kAreaLeft+s.s*slotW, panelTop+6, s.c*slotW, 13, juce::Justification::centred, false);
            if(s.s+s.c<numKnobs){
                const int sx=kAreaLeft+(s.s+s.c)*slotW;
                g.setColour(juce::Colour(0xff0a0908)); g.fillRect(sx-1,panelTop+20,2,panelH-26);
                g.setColour(juce::Colour(0xff222220)); g.fillRect(sx,panelTop+20,1,panelH-26);
            }
        }
    }
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.f)));
    g.setColour(juce::Colour(0xff3a3632));
    g.drawText("CHANNEL", 206, 1, 230, 10, juce::Justification::centred, false);
}

// ── Resized ────────────────────────────────────────────────────────────────

void UmbraPluginEditor::resized()
{
    const int W = getWidth(), H = getHeight();

    // Header
    const int chW=68,chH=26,chY=11,chStart=206;
    chClean .setBounds(chStart,           chY,chW,chH);
    chCrunch.setBounds(chStart+chW+4,     chY,chW,chH);
    chLead  .setBounds(chStart+chW*2+8,   chY,chW,chH);
    boostBtn.setBounds(chStart+chW*3+16,  chY,46, chH);
    {
        const int cs=chStart+chW*3+16+46+12,cW=40,cH=22,cY2=13;
        cabV30 .setBounds(cs,        cY2,cW,   cH);
        cabG12 .setBounds(cs+cW+3,   cY2,cW,   cH);
        cabGrbn.setBounds(cs+cW*2+6, cY2,cW+2, cH);
        cabOpen.setBounds(cs+cW*3+12,cY2,cW,   cH);
    }

    // Preset row
    const int rowY=50,rowH=30,pW=80,pGap=3;
    const int ap=8+(pW+pGap)*5+4;
    tapBtn  .setBounds(ap,          rowY,40,rowH);
    const int sdY=rowY+4,sdH=rowH-8;
    subdivQ .setBounds(ap+44,       sdY,34,sdH);
    subdivD8.setBounds(ap+44+37,    sdY,38,sdH);
    subdiv8 .setBounds(ap+44+79,    sdY,34,sdH);
    bpmLabel.setBounds(ap+44+116,   rowY,62,rowH);
    const int irCW=26,irLW=72,irR=W-10;
    irClearBtn.setBounds(irR-irCW,        rowY,irCW,rowH);
    irLoadBtn .setBounds(irR-irCW-irLW-4, rowY,irLW, rowH);
    const int irLX=ap+44+116+64;
    irNameLabel.setBounds(irLX,rowY,irR-irCW-irLW-4-irLX-6,rowH);

    // Knob area
    const int panelTop=92, panelH=H-270-panelTop;
    constexpr int kInMW=22,kInMGap=4,kOutMW=24,kOutMGap=14;
    const int inMX=12, outMX=W-kOutMW-kOutMGap;
    inputMeter .setBounds(inMX, panelTop+8,kInMW, panelH-14);
    outputMeter.setBounds(outMX,panelTop+8,kOutMW,panelH-14);
    const int kAL=inMX+kInMW+kInMGap, kAW=outMX-kAL;
    const int nK=17, slotW=kAW/nK;
    const int secH=22;
    const int kSz=juce::jmin(panelH-secH-34,slotW-6);
    const int kY=panelTop+secH+(panelH-secH-kSz-14)/2;

    auto place=[&](juce::Slider&k,juce::Label&l,int i){
        const int kx=kAL+i*slotW+(slotW-kSz)/2;
        k.setBounds(kx,kY,kSz,kSz);
        l.setBounds(kx-4,kY+kSz+2,kSz+8,14);
    };
    place(compKnob,compLabel,0); place(gateKnob,gateLabel,1);
    place(gainKnob,gainLabel,2); place(transposeKnob,transposeLabel,3);
    place(bassKnob,bassLabel,4); place(midKnob,midLabel,5);
    place(trebleKnob,trebleLabel,6); place(presenceKnob,presenceLabel,7);
    place(volumeKnob,volumeLabel,8); place(sagKnob,sagLabel,9);
    place(tremKnob,tremLabel,10);
    place(reverbKnob,reverbLabel,11); place(chorusKnob,chorusLabel,12);
    place(delayKnob,delayLabel,13);  place(widthKnob,widthLabel,14);
    place(phaserKnob,phaserLabel,15); place(flangerKnob,flangerLabel,16);

    // EQ panel
    const int eqTop=panelTop+panelH+8;
    eqPanel.setBounds(8,eqTop,W-16,std::max(10,H-eqTop-6));
}
