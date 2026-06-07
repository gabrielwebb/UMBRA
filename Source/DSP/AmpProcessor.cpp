#include "AmpProcessor.h"

using Filt   = juce::dsp::IIR::Filter<float>;
using Coeffs = juce::dsp::IIR::Coefficients<float>;

// ── prepare ────────────────────────────────────────────────────────────────

void AmpProcessor::prepare(double sr, int blockSize)
{
    sampleRate = sr;

    // ── Oversampled spec (8×) — nonlinear chain ────────────────────────────
    juce::dsp::ProcessSpec osrSpec;
    osrSpec.sampleRate       = sr * 8.0;
    osrSpec.maximumBlockSize = static_cast<uint32_t>(blockSize * 8);
    osrSpec.numChannels      = 1;

    boostHpf.prepare(osrSpec); boostMid.prepare(osrSpec); boostLpf.prepare(osrSpec);
    preHpf.prepare(osrSpec);
    lpf1.prepare(osrSpec); lpf2.prepare(osrSpec);
    lpf3.prepare(osrSpec); lpf4.prepare(osrSpec);
    dc1.prepare(osrSpec);  dc2.prepare(osrSpec);
    dc3.prepare(osrSpec);  dc4.prepare(osrSpec);
    leadChunkPeak.prepare(osrSpec);
    bassFilter.prepare(osrSpec); midFilter.prepare(osrSpec);
    trebleFilter.prepare(osrSpec); presenceFilter.prepare(osrSpec);
    cabHpf.prepare(osrSpec); cabLoResonance.prepare(osrSpec);
    cabMidScoop.prepare(osrSpec); cabPresence.prepare(osrSpec); cabLpf.prepare(osrSpec);
    gateSidechain.prepare(osrSpec);
    powerAmpDC.prepare(osrSpec); powerAmpLpf.prepare(osrSpec);

    // ── Base-rate spec (stereo, for post-chain FX) ─────────────────────────
    juce::dsp::ProcessSpec stereoSpec { sr, static_cast<uint32_t>(blockSize), 2 };
    juce::dsp::ProcessSpec monoSpec   { sr, static_cast<uint32_t>(blockSize), 1 };

    // Convolution (mono)
    convolution.prepare(monoSpec);

    // Chorus
    chorus.prepare(stereoSpec);
    chorus.setRate(0.35f); chorus.setDepth(0.55f);
    chorus.setCentreDelay(7.0f); chorus.setFeedback(0.08f);

    // Phaser (stereo)
    phaser.prepare(stereoSpec);
    phaser.setRate(0.5f);
    phaser.setDepth(0.5f);
    phaser.setCentreFrequency(1300.f);
    phaser.setFeedback(0.7f);
    phaser.setMix(0.5f);

    // Post-amp parametric EQ (stereo, base rate — 4 bands × 2 channels)
    {
        juce::dsp::ProcessSpec s1 { sr, static_cast<uint32_t>(blockSize), 1 };
        for (auto& f : postEqL) f.prepare(s1);
        for (auto& f : postEqR) f.prepare(s1);
    }

    // Dattorro plate reverb
    dattorroReverb.prepare(sr);

    // Oversampling
    oversampling.initProcessing(static_cast<size_t>(blockSize));
    oversampling.reset();

    // Pitch shifter
    pitchShifter.reset();

    // Delay buffer (2 s max)
    const int maxDelay = static_cast<int>(sr * 2.0);
    delayBuf.setSize(1, maxDelay, false, true, false);
    delayWritePos    = 0;
    delayTimeSamples = static_cast<int>(sr * 0.350);

    // Flanger buffer
    flangerBufL.fill(0.f); flangerBufR.fill(0.f);
    flangerWriteL = flangerWriteR = 0;
    flangerLfoPhase = 0.f;

    // ── Timing coefficients ────────────────────────────────────────────────
    // (gate coeffs are computed in updateFilters — they depend on neural mode)
    const float osr_f = static_cast<float>(sr) * 8.0f;
    const float sr_f  = static_cast<float>(sr);
    powerAttCoeff = 1.f - std::exp(-1.f / (0.008f  * osr_f));
    powerRelCoeff = std::exp(-1.f         / (0.200f  * osr_f));
    sagAttCoeff   = 1.f - std::exp(-1.f / (0.002f  * sr_f));
    sagRelCoeff   = std::exp(-1.f         / (0.150f  * sr_f));
    compAttCoeff  = 1.f - std::exp(-1.f / (0.003f  * sr_f));
    compRelCoeff  = std::exp(-1.f         / (0.150f  * sr_f));

    // ── Reverb pre-delay ──────────────────────────────────────────────────
    preDelaySamples = static_cast<int>(sr * 0.012);
    preDelayBuf.fill(0.f); preDelayBuf2.fill(0.f); preDelayWrite = 0;
    reverbDryBuf.setSize(2, blockSize + 16, false, true, false);

    // ── Tuner / spectrum buffers ──────────────────────────────────────────
    tunerBuf.fill(0.f);    tunerWritePos.store(0);
    spectrumBuf.fill(0.f); spectrumWritePos.store(0);

    updateFilters();
}

// ── setParams ──────────────────────────────────────────────────────────────

void AmpProcessor::setParams(const Params& p)
{
    const bool channelChanged = (p.channel != params.channel);
    params = p;
    pitchShifter.setSemitones(params.transpose);
    delayTimeSamples = juce::jlimit(1,
        delayBuf.getNumSamples() - 1,
        static_cast<int>(sampleRate * static_cast<double>(params.delayTimeMs) / 1000.0));
    updateFilters();
    if (channelChanged) resetFilterStates();
}

// ── neuralActiveNow ──────────────────────────────────────────────────────────

bool AmpProcessor::neuralActiveNow() const noexcept
{
    switch (params.channel)
    {
        case Channel::Clean:  return neuralClean .isLoaded();
        case Channel::Crunch: return neuralCrunch.isLoaded();
        case Channel::Lead:   return neuralLead  .isLoaded();
    }
    return false;
}

// ── updateFilters ──────────────────────────────────────────────────────────

void AmpProcessor::updateFilters()
{
    const float  g   = params.gain;
    const double sr  = sampleRate;
    // Analog filters run inside the 8× oversampled loop for the waveshaper
    // path, but at BASE rate for the neural path (the LSTM is rate-dependent).
    // `osr` is therefore the actual rate the pre/tone/cab filters run at.
    const bool   neural = neuralActiveNow();
    const double osr = neural ? sr : sr * 8.0;

    // Gate timing — must match the rate the gate actually runs at.
    {
        const float r = static_cast<float>(osr);
        gateAttCoeff = 1.f - std::exp(-1.f / (0.0003f * r));
        gateRelCoeff = std::exp(-1.f        / (0.020f  * r));
    }

    // Gate sidechain
    gateSidechain.coefficients = Coeffs::makeHighPass(osr, 100.0, 0.707);

    // Boost (Tube Screamer)
    boostHpf.coefficients = Coeffs::makeHighPass( osr, 720.0, 0.707);
    boostMid.coefficients = Coeffs::makePeakFilter(osr, 900.0, 2.0f,
        juce::Decibels::decibelsToGain(6.0f));
    boostLpf.coefficients = Coeffs::makeLowPass(  osr, 3500.0, 0.707);
    boostDrive = 6.0f;

    // Pre-HPF
    const double hpfFreq = (params.channel == Channel::Clean) ? 60.0
                         : (params.channel == Channel::Crunch)? 80.0
                                                              : 120.0;
    preHpf.coefficients = Coeffs::makeHighPass(osr, hpfFreq, 0.707);

    // Inter-stage LPFs
    lpf1.coefficients = Coeffs::makeLowPass(osr, 8000.0, 0.707);
    lpf2.coefficients = Coeffs::makeLowPass(osr, 6500.0, 0.707);
    lpf3.coefficients = Coeffs::makeLowPass(osr, 5500.0, 0.707);
    lpf4.coefficients = Coeffs::makeLowPass(osr, 4500.0, 0.707);

    // DC blockers
    dc1.coefficients = dc2.coefficients = dc3.coefficients = dc4.coefficients
        = Coeffs::makeHighPass(osr, 10.0, 0.707);

    // Lead input chug peak
    leadChunkPeak.coefficients = Coeffs::makePeakFilter(osr, 900.0, 2.2f,
        juce::Decibels::decibelsToGain(4.0f));

    // Passive-style tone stack
    const float midDb    = (params.mid    - 0.5f) * 20.0f;
    const float passLoad = (0.5f - params.mid) * 4.0f;
    const float bassDb   = (params.bass   - 0.5f) * 28.0f + passLoad;
    const float trebleDb = (params.treble - 0.5f) * 28.0f + passLoad;
    const double midFreq = (params.channel == Channel::Lead)   ? 700.0
                         : (params.channel == Channel::Crunch) ? 600.0
                                                               : 500.0;
    bassFilter.coefficients   = Coeffs::makeLowShelf( osr, 120.0, 0.7,
        juce::Decibels::decibelsToGain(bassDb));
    midFilter.coefficients    = Coeffs::makePeakFilter(osr, midFreq, 1.3f,
        juce::Decibels::decibelsToGain(midDb));
    trebleFilter.coefficients = Coeffs::makeHighShelf(osr, 2500.0, 0.7,
        juce::Decibels::decibelsToGain(trebleDb));

    // Presence (NFB model)
    const double presFreq = (params.channel == Channel::Lead)   ? 3800.0
                          : (params.channel == Channel::Crunch) ? 2500.0
                                                                 : 1800.0;
    presenceFilter.coefficients = Coeffs::makePeakFilter(osr, presFreq, 1.5f,
        juce::Decibels::decibelsToGain((params.presence - 0.5f) * 16.0f));

    // Power amp
    powerAmpDC.coefficients  = Coeffs::makeHighPass(osr,  40.0, 0.707);
    powerAmpLpf.coefficients = Coeffs::makeLowPass( osr, 8000.0, 0.8);
    powerAmpDrive = 1.2f + params.gain * 0.4f + params.presence * 0.5f;

    // Cabinet character
    switch (params.cabType)
    {
        default:
        case CabType::V30:
            cabHpf.coefficients         = Coeffs::makeHighPass( osr,  80.0, 0.9);
            cabLoResonance.coefficients = Coeffs::makePeakFilter(osr, 120.0, 2.5f,
                juce::Decibels::decibelsToGain( 2.0f));
            cabMidScoop.coefficients    = Coeffs::makePeakFilter(osr, 600.0, 1.1f,
                juce::Decibels::decibelsToGain(-4.0f));
            cabPresence.coefficients    = Coeffs::makePeakFilter(osr, 3000.0, 2.0f,
                juce::Decibels::decibelsToGain( 5.0f));
            cabLpf.coefficients         = Coeffs::makeLowPass(  osr, 5500.0, 0.8);
            break;
        case CabType::G12T75:
            cabHpf.coefficients         = Coeffs::makeHighPass( osr,  90.0, 0.85);
            cabLoResonance.coefficients = Coeffs::makePeakFilter(osr, 100.0, 3.0f,
                juce::Decibels::decibelsToGain( 1.5f));
            cabMidScoop.coefficients    = Coeffs::makePeakFilter(osr, 450.0, 1.0f,
                juce::Decibels::decibelsToGain(-5.0f));
            cabPresence.coefficients    = Coeffs::makePeakFilter(osr, 4000.0, 1.5f,
                juce::Decibels::decibelsToGain( 8.0f));
            cabLpf.coefficients         = Coeffs::makeLowPass(  osr, 7000.0, 0.75);
            break;
        case CabType::Greenback:
            cabHpf.coefficients         = Coeffs::makeHighPass( osr,  70.0, 1.0);
            cabLoResonance.coefficients = Coeffs::makePeakFilter(osr, 150.0, 1.8f,
                juce::Decibels::decibelsToGain( 3.0f));
            cabMidScoop.coefficients    = Coeffs::makePeakFilter(osr, 700.0, 0.9f,
                juce::Decibels::decibelsToGain(-2.0f));
            cabPresence.coefficients    = Coeffs::makePeakFilter(osr, 2500.0, 2.5f,
                juce::Decibels::decibelsToGain( 3.0f));
            cabLpf.coefficients         = Coeffs::makeLowPass(  osr, 4500.0, 0.85);
            break;
        case CabType::Open:
            cabHpf.coefficients         = Coeffs::makeHighPass( osr,  60.0, 0.7);
            cabLoResonance.coefficients = Coeffs::makePeakFilter(osr, 120.0, 2.0f,
                juce::Decibels::decibelsToGain( 1.0f));
            cabMidScoop.coefficients    = Coeffs::makePeakFilter(osr, 500.0, 0.8f,
                juce::Decibels::decibelsToGain(-1.0f));
            cabPresence.coefficients    = Coeffs::makePeakFilter(osr, 2000.0, 1.5f,
                juce::Decibels::decibelsToGain( 2.0f));
            cabLpf.coefficients         = Coeffs::makeLowPass(  osr, 8000.0, 0.65);
            break;
    }

    // Post-amp 4-band parametric EQ (base rate, stereo)
    for (int b = 0; b < 4; ++b)
    {
        const auto& band = params.eq[b];
        juce::dsp::IIR::Coefficients<float>::Ptr c;
        const double f = juce::jlimit(20.0, sr * 0.49, static_cast<double>(band.freq));
        const float  gn = juce::Decibels::decibelsToGain(band.gainDb);
        const float  q  = juce::jlimit(0.1f, 10.f, band.q);

        switch (band.type)
        {
            case EqBand::LowShelf:  c = Coeffs::makeLowShelf( sr, f, q, gn); break;
            case EqBand::HighShelf: c = Coeffs::makeHighShelf(sr, f, q, gn); break;
            case EqBand::Notch:     c = Coeffs::makePeakFilter(sr, f, q, juce::Decibels::decibelsToGain(-30.f)); break;
            default:                c = Coeffs::makePeakFilter(sr, f, q, gn); break;
        }
        postEqL[b].coefficients = c;
        postEqR[b].coefficients = c;
    }

    // Chorus
    chorus.setMix(params.chorus * 0.55f);

    // Phaser rate/depth driven by params.phaser knob
    phaser.setMix(params.phaser * 0.7f);

    // Gate threshold
    gateThresh = (params.gate < 0.01f)
                 ? 0.0f
                 : juce::Decibels::decibelsToGain(-80.0f + params.gate * 36.0f);

    // Cached drive values
    cleanDrive = 1.0f + g * 4.5f;
    cleanNorm  = 1.0f / (1.0f - std::exp(-cleanDrive));
    crunchDrive1 = 1.5f + g * 5.0f;
    crunchDrive2 = 1.5f + g * 4.0f;
    leadDrive1 = 1.5f + g * 4.0f;
    leadDrive2 = 1.5f + g * 4.5f;
    leadDrive3 = 1.2f + g * 3.5f;
    leadDrive4 = 1.0f + g * 2.0f;
}

// ── resetFilterStates ──────────────────────────────────────────────────────

void AmpProcessor::resetFilterStates()
{
    gateSidechain.reset();
    boostHpf.reset(); boostMid.reset(); boostLpf.reset();
    preHpf.reset();
    lpf1.reset(); lpf2.reset(); lpf3.reset(); lpf4.reset();
    dc1.reset();  dc2.reset();  dc3.reset();  dc4.reset();
    leadChunkPeak.reset();
    bassFilter.reset(); midFilter.reset(); trebleFilter.reset(); presenceFilter.reset();
    cabHpf.reset(); cabLoResonance.reset(); cabMidScoop.reset();
    cabPresence.reset(); cabLpf.reset();
    powerAmpDC.reset(); powerAmpLpf.reset();
    for (auto& f : postEqL) f.reset();
    for (auto& f : postEqR) f.reset();

    oversampling.reset();   // clear stale state when toggling neural/waveshaper
    neuralClean.reset(); neuralCrunch.reset(); neuralLead.reset();
    gateEnvelope = sagEnvelope = powerEnvelope = compEnvelope = 0.0f;
    lfoPhase     = flangerLfoPhase = 0.0f;
    delayBuf.clear(); delayWritePos = 0;
    flangerBufL.fill(0.f); flangerBufR.fill(0.f);
    flangerWriteL = flangerWriteR = 0;
    preDelayBuf.fill(0.f); preDelayBuf2.fill(0.f); preDelayWrite = 0;
    dattorroReverb.reset();
}

// ── IR ─────────────────────────────────────────────────────────────────────

void AmpProcessor::loadIR(const juce::File& file)
{
    convolution.loadImpulseResponse(file,
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::yes, 0);
    irLoaded.store(true);
}

void AmpProcessor::clearIR()
{
    irLoaded.store(false);
    convolution.reset();
}

// ── Neural model API ───────────────────────────────────────────────────────

bool AmpProcessor::loadNeuralModel(const juce::File& file, Channel ch)
{
    bool ok = false;
    switch (ch)
    {
        case Channel::Clean:  ok = neuralClean .loadFromFile(file); break;
        case Channel::Crunch: ok = neuralCrunch.loadFromFile(file); break;
        case Channel::Lead:   ok = neuralLead  .loadFromFile(file); break;
    }
    // Loading toggles neural mode for this channel → re-rate the analog filters
    // (base vs 8×) and clear stale state so the switch is click-free.
    if (ok && ch == params.channel) { updateFilters(); resetFilterStates(); }
    return ok;
}

void AmpProcessor::clearNeuralModel(Channel ch)
{
    switch (ch)
    {
        case Channel::Clean:  neuralClean .clear(); break;
        case Channel::Crunch: neuralCrunch.clear(); break;
        case Channel::Lead:   neuralLead  .clear(); break;
    }
    if (ch == params.channel) { updateFilters(); resetFilterStates(); }
}

bool AmpProcessor::hasNeuralModel(Channel ch) const noexcept
{
    switch (ch)
    {
        case Channel::Clean:  return neuralClean .isLoaded();
        case Channel::Crunch: return neuralCrunch.isLoaded();
        case Channel::Lead:   return neuralLead  .isLoaded();
    }
    return false;
}

juce::String AmpProcessor::getNeuralModelName(Channel ch) const noexcept
{
    switch (ch)
    {
        case Channel::Clean:  return neuralClean .getName();
        case Channel::Crunch: return neuralCrunch.getName();
        case Channel::Lead:   return neuralLead  .getName();
    }
    return {};
}

// ── Snapshot helpers ───────────────────────────────────────────────────────

void AmpProcessor::getTunerSnapshot(float* out, int len) const noexcept
{
    const int wp   = tunerWritePos.load(std::memory_order_acquire);
    const int mask = kTunerBufLen - 1;
    for (int i = 0; i < len; ++i)
        out[i] = tunerBuf[static_cast<size_t>((wp + i) & mask)];
}

void AmpProcessor::getSpectrumSnapshot(float* out, int len) const noexcept
{
    const int wp   = spectrumWritePos.load(std::memory_order_acquire);
    const int mask = kSpectrumBufLen - 1;
    for (int i = 0; i < len; ++i)
        out[i] = spectrumBuf[static_cast<size_t>((wp + i) & mask)];
}

// ── Per-sample helpers ─────────────────────────────────────────────────────

float AmpProcessor::applyGateSample(float x) noexcept
{
    if (gateThresh == 0.0f)
    {
        gateOpenLevel.store(1.0f, std::memory_order_relaxed);
        return x;
    }
    const float sc    = gateSidechain.processSample(x);
    const float level = std::abs(sc);
    if (level > gateEnvelope)
        gateEnvelope += (level - gateEnvelope) * gateAttCoeff;
    else
        gateEnvelope *= gateRelCoeff;

    const float hi = gateThresh, lo = gateThresh * 0.5f;
    const float gv = (gateEnvelope >= hi) ? 1.0f
                   : (gateEnvelope >  lo) ? (gateEnvelope - lo) / (hi - lo)
                                          : 0.0f;
    gateOpenLevel.store(gv, std::memory_order_relaxed);
    return x * gv;
}

float AmpProcessor::applyCompressorSample(float x) noexcept
{
    if (params.comp < 0.001f)
    {
        compGainReduction.store(0.0f, std::memory_order_relaxed);
        return x;
    }
    // Optical model: slow-attack photo-resistor, longer release
    const float lvl = std::abs(x);
    if (lvl > compEnvelope)
        compEnvelope += (lvl - compEnvelope) * compAttCoeff;
    else
        compEnvelope *= compRelCoeff;

    constexpr float kThresh = 0.25f;
    if (compEnvelope <= kThresh)
    {
        compGainReduction.store(0.0f, std::memory_order_relaxed);
        return x;
    }
    // Soft-knee: ratio ramps 1:1→8:1 as comp knob goes 0→1
    const float ratio = 1.0f + params.comp * 7.0f;
    const float gr    = (kThresh + (compEnvelope - kThresh) / ratio) / compEnvelope;
    compGainReduction.store(1.0f - gr, std::memory_order_relaxed);
    return x * gr;
}

float AmpProcessor::processCleanSample(float x) noexcept
{
    x = waveshape(x * cleanDrive) * cleanNorm * 0.4f;
    x = dc1.processSample(x);
    x = lpf1.processSample(x);
    const float stage2Drive = 1.0f + params.gain * 1.2f;
    x = waveshape(x * stage2Drive) * 0.9f;
    x = dc2.processSample(x);
    return x;
}

float AmpProcessor::processCrunchSample(float x) noexcept
{
    x = waveshape(x * crunchDrive1);
    x = dc1.processSample(x);
    x = lpf1.processSample(x);
    if (params.sag > 0.001f)
    {
        const float lvl = std::abs(x);
        if (lvl > sagEnvelope) sagEnvelope += (lvl - sagEnvelope) * sagAttCoeff;
        else                   sagEnvelope *= sagRelCoeff;
    }
    const float sagScale = 1.0f - params.sag * sagEnvelope * 0.45f;
    x *= 0.4f;
    x = std::tanh(x * crunchDrive2 * sagScale);
    x = dc2.processSample(x); x = lpf2.processSample(x); x *= 0.5f;
    x = triodeClip(x, (1.5f + params.gain * 3.5f) * sagScale);
    x = dc3.processSample(x); x = lpf3.processSample(x);
    return x;
}

float AmpProcessor::processLeadSample(float x) noexcept
{
    x = leadChunkPeak.processSample(x);
    x = std::tanh(x * leadDrive1);
    x = dc1.processSample(x); x = lpf1.processSample(x);
    if (params.sag > 0.001f)
    {
        const float lvl = std::abs(x);
        if (lvl > sagEnvelope) sagEnvelope += (lvl - sagEnvelope) * sagAttCoeff;
        else                   sagEnvelope *= sagRelCoeff;
    }
    const float sagScale = 1.0f - params.sag * sagEnvelope * 0.45f;
    x *= 0.35f;
    x = std::tanh(x * leadDrive2 * sagScale);
    x = dc2.processSample(x); x = lpf2.processSample(x); x *= 0.45f;
    x = triodeClip(x, leadDrive3 * sagScale);
    x = dc3.processSample(x); x = lpf3.processSample(x); x *= 0.55f;
    x = waveshape(x * leadDrive4 * sagScale);
    x = dc4.processSample(x); x = lpf4.processSample(x);
    return x;
}

float AmpProcessor::processPowerAmpSample(float x) noexcept
{
    x = powerAmpDC.processSample(x);
    x = (x >= 0.0f) ?  1.0f - std::exp(-x * powerAmpDrive)
                    : -1.0f + std::exp( x * powerAmpDrive * 0.88f);
    x += x * std::abs(x) * 0.04f;
    const float lvl = std::abs(x);
    if (lvl > powerEnvelope) powerEnvelope += (lvl - powerEnvelope) * powerAttCoeff;
    else                     powerEnvelope *= powerRelCoeff;
    x *= 1.0f / (1.0f + powerEnvelope * 0.28f);
    x = powerAmpLpf.processSample(x);
    return x;
}

// ── process ────────────────────────────────────────────────────────────────

void AmpProcessor::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    float* ch0 = buffer.getWritePointer(0);

    // ── Input level + tuner buffer (raw signal, pre-DSP) ──────────────────
    {
        float inPeak = 0.f;
        for (int i = 0; i < numSamples; ++i) inPeak = std::max(inPeak, std::abs(ch0[i]));
        inputLevel.store(inputLevel.load(std::memory_order_relaxed) * 0.85f + inPeak * 0.15f,
                         std::memory_order_relaxed);

        int wp = tunerWritePos.load(std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            tunerBuf[static_cast<size_t>(wp & (kTunerBufLen - 1))] = ch0[i];
            ++wp;
        }
        tunerWritePos.store(wp & (kTunerBufLen - 1), std::memory_order_release);
    }

    // ── Transpose ─────────────────────────────────────────────────────────
    if (params.transpose != 0)
        for (int i = 0; i < numSamples; ++i)
            ch0[i] = pitchShifter.process(ch0[i]);

    // ── Compressor (before amp, optical model) ────────────────────────────
    for (int i = 0; i < numSamples; ++i)
        ch0[i] = applyCompressorSample(ch0[i]);

    // GuitarML captures bake in the amp's own gain; the GAIN knob acts as an
    // input drive (±1 octave around unity) like a guitar volume knob.
    // gain 0.0 → ×0.5, 0.5 → ×1.0, 1.0 → ×2.0
    const float nnDrive = std::pow(4.0f, params.gain - 0.5f);

    if (neuralActiveNow())
    {
        // ── Neural path — BASE RATE (no oversampling) ─────────────────────
        // An LSTM amp capture is rate-dependent: it was trained at the project
        // sample rate and its output is already band-limited, so it must run at
        // base rate (oversampling it shifts every internal time-constant 8× and
        // produces a thin, fizzy, scratchy tone). The analog filters around it
        // already have base-rate coefficients (see updateFilters/neuralActiveNow).
        // The capture also contains a full power amp, so processPowerAmpSample
        // is skipped — re-saturating it would double-clip.
        NeuralAmpModel& model = (params.channel == Channel::Clean)  ? neuralClean
                              : (params.channel == Channel::Crunch) ? neuralCrunch
                                                                    : neuralLead;
        const bool noIR = !irLoaded.load();
        for (int i = 0; i < numSamples; ++i)
        {
            float x = ch0[i];

            if (params.boost)
            {
                x = boostHpf.processSample(x);
                x = boostMid.processSample(x);
                x = std::tanh(x * boostDrive);
                x = boostLpf.processSample(x);
                x *= 0.65f;
            }

            x = preHpf.processSample(x);
            x = applyGateSample(x);

            x = model.process(x * nnDrive);

            x = bassFilter.processSample(x);
            x = midFilter.processSample(x);
            x = trebleFilter.processSample(x);
            x = presenceFilter.processSample(x);

            if (noIR)
            {
                x = cabHpf.processSample(x);
                x = cabLoResonance.processSample(x);
                x = cabMidScoop.processSample(x);
                x = cabPresence.processSample(x);
                x = cabLpf.processSample(x);
            }

            ch0[i] = softLimit(x);
        }
    }
    else
    {
        // ── Waveshaper path — 8× OVERSAMPLED (memoryless nonlinearities) ──
        juce::dsp::AudioBlock<float> inputBlock(buffer);
        auto monoBlock = inputBlock.getSingleChannelBlock(0);
        auto upBlock   = oversampling.processSamplesUp(monoBlock);

        const int upSamples = static_cast<int>(upBlock.getNumSamples());
        float* up = upBlock.getChannelPointer(0);

        for (int i = 0; i < upSamples; ++i)
        {
            float x = up[i];

            if (params.boost)
            {
                x = boostHpf.processSample(x);
                x = boostMid.processSample(x);
                x = std::tanh(x * boostDrive);
                x = boostLpf.processSample(x);
                x *= 0.65f;
            }

            x = preHpf.processSample(x);
            x = applyGateSample(x);

            switch (params.channel)
            {
                case Channel::Clean:  x = processCleanSample(x);  break;
                case Channel::Crunch: x = processCrunchSample(x); break;
                case Channel::Lead:   x = processLeadSample(x);   break;
            }

            x = bassFilter.processSample(x);
            x = midFilter.processSample(x);
            x = trebleFilter.processSample(x);
            x = presenceFilter.processSample(x);
            x = processPowerAmpSample(x);

            if (!irLoaded.load())
            {
                x = cabHpf.processSample(x);
                x = cabLoResonance.processSample(x);
                x = cabMidScoop.processSample(x);
                x = cabPresence.processSample(x);
                x = cabLpf.processSample(x);
            }

            up[i] = softLimit(x);
        }

        oversampling.processSamplesDown(monoBlock);
    }

    // ── Mono delay ────────────────────────────────────────────────────────
    if (params.delay > 0.001f)
    {
        const int maxSize = delayBuf.getNumSamples();
        for (int i = 0; i < numSamples; ++i)
        {
            int readPos = delayWritePos - delayTimeSamples;
            if (readPos < 0) readPos += maxSize;
            const float delayed = delayBuf.getSample(0, readPos);
            delayBuf.setSample(0, delayWritePos, ch0[i] + delayed * delayFeedback);
            delayWritePos = (delayWritePos + 1) % maxSize;
            ch0[i] += delayed * params.delay * 0.7f;
        }
    }

    // ── Tremolo LFO ───────────────────────────────────────────────────────
    if (params.trem > 0.001f)
    {
        const float phaseInc = juce::MathConstants<float>::twoPi
                               * lfoRate / static_cast<float>(sampleRate);
        for (int i = 0; i < numSamples; ++i)
        {
            const float lfoVal = 1.0f - params.trem * 0.6f
                                 * (0.5f + 0.5f * std::sin(lfoPhase));
            ch0[i] *= lfoVal;
            lfoPhase += phaseInc;
            if (lfoPhase > juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;
        }
    }

    // ── IR convolution (mono) ─────────────────────────────────────────────
    if (irLoaded.load())
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto mono = block.getSingleChannelBlock(0);
        convolution.process(juce::dsp::ProcessContextReplacing<float>(mono));
    }

    // ── Mono → stereo ─────────────────────────────────────────────────────
    if (numCh > 1) buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    // ── Chorus ────────────────────────────────────────────────────────────
    if (params.chorus > 0.001f)
    {
        juce::dsp::AudioBlock<float> stereoBlock(buffer);
        chorus.process(juce::dsp::ProcessContextReplacing<float>(stereoBlock));
    }

    // ── Phaser ────────────────────────────────────────────────────────────
    if (params.phaser > 0.001f)
    {
        juce::dsp::AudioBlock<float> stereoBlock(buffer);
        phaser.process(juce::dsp::ProcessContextReplacing<float>(stereoBlock));
    }

    // ── Flanger ───────────────────────────────────────────────────────────
    if (params.flanger > 0.001f)
    {
        float* L = buffer.getWritePointer(0);
        float* R = numCh > 1 ? buffer.getWritePointer(1) : L;
        const float flangerPhaseInc = juce::MathConstants<float>::twoPi * 0.25f
                                      / static_cast<float>(sampleRate);
        // Base delay: 7 ms; mod depth: ±5 ms
        const float baseDelay = static_cast<float>(sampleRate) * 0.007f;
        const float modDepth  = static_cast<float>(sampleRate) * 0.005f;
        const float feedback  = 0.65f;
        const float mix       = params.flanger * 0.7f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float modL = baseDelay + std::sin(flangerLfoPhase)        * modDepth;
            const float modR = baseDelay + std::sin(flangerLfoPhase + 1.5f) * modDepth;

            const int delL  = juce::jlimit(1, kFlangerBufLen - 2, static_cast<int>(modL));
            const int delR  = juce::jlimit(1, kFlangerBufLen - 2, static_cast<int>(modR));

            const int rPosL = (flangerWriteL - delL + kFlangerBufLen) % kFlangerBufLen;
            const int rPosR = (flangerWriteR - delR + kFlangerBufLen) % kFlangerBufLen;

            const float flanL = flangerBufL[static_cast<size_t>(rPosL)];
            const float flanR = flangerBufR[static_cast<size_t>(rPosR)];

            flangerBufL[static_cast<size_t>(flangerWriteL)] = L[i] + flanL * feedback;
            flangerBufR[static_cast<size_t>(flangerWriteR)] = R[i] + flanR * feedback;
            flangerWriteL = (flangerWriteL + 1) % kFlangerBufLen;
            flangerWriteR = (flangerWriteR + 1) % kFlangerBufLen;

            L[i] = L[i] * (1.f - mix) + flanL * mix;
            R[i] = R[i] * (1.f - mix) + flanR * mix;

            flangerLfoPhase += flangerPhaseInc;
            if (flangerLfoPhase > juce::MathConstants<float>::twoPi)
                flangerLfoPhase -= juce::MathConstants<float>::twoPi;
        }
    }

    // ── Dattorro plate reverb ─────────────────────────────────────────────
    if (params.reverb > 0.001f)
    {
        // Save dry
        reverbDryBuf.copyFrom(0, 0, buffer, 0, 0, numSamples);
        if (numCh > 1) reverbDryBuf.copyFrom(1, 0, buffer, 1, 0, numSamples);

        // Pre-delay (12 ms, independent L/R for stereo preservation)
        {
            float* data0 = buffer.getWritePointer(0);
            float* data1 = numCh > 1 ? buffer.getWritePointer(1) : nullptr;
            for (int i = 0; i < numSamples; ++i)
            {
                const auto wi   = static_cast<size_t>(preDelayWrite);
                const auto rPos = static_cast<size_t>((preDelayWrite - preDelaySamples + kPreDelay)
                                                       & (kPreDelay - 1));
                preDelayBuf[wi] = data0[i]; data0[i] = preDelayBuf[rPos];
                if (data1) { preDelayBuf2[wi] = data1[i]; data1[i] = preDelayBuf2[rPos]; }
                preDelayWrite = (preDelayWrite + 1) & (kPreDelay - 1);
            }
        }

        // Run Dattorro plate reverb sample-by-sample
        dattorroReverb.decay     = params.reverb * 0.95f + 0.04f;
        dattorroReverb.bandwidth = 0.88f;
        dattorroReverb.damping   = 0.30f;
        dattorroReverb.wetLevel  = params.reverb * 0.55f;
        dattorroReverb.dryLevel  = 0.0f;  // dry restored below

        {
            float* L = buffer.getWritePointer(0);
            float* R = numCh > 1 ? buffer.getWritePointer(1) : L;
            for (int i = 0; i < numSamples; ++i)
                dattorroReverb.processSample(L[i], R[i]);
        }

        // Restore dry
        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFrom(ch, 0, reverbDryBuf, ch, 0, numSamples);
    }

    // ── Post-amp 4-band parametric EQ ────────────────────────────────────
    {
        float* L = buffer.getWritePointer(0);
        float* R = numCh > 1 ? buffer.getWritePointer(1) : nullptr;
        for (int b = 0; b < 4; ++b)
        {
            if (!params.eq[b].enabled) continue;
            for (int i = 0; i < numSamples; ++i)
            {
                L[i] = postEqL[b].processSample(L[i]);
                if (R) R[i] = postEqR[b].processSample(R[i]);
            }
        }
    }

    // ── M/S stereo width ─────────────────────────────────────────────────
    if (numCh > 1)
    {
        const float sGain = params.width * 2.0f;
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = (L[i] + R[i]) * 0.5f;
            const float side = (L[i] - R[i]) * 0.5f * sGain;
            L[i] = mid + side;
            R[i] = mid - side;
        }
    }

    // ── Output volume ─────────────────────────────────────────────────────
    buffer.applyGain(params.volume * 0.35f);

    // ── Spectrum buffer (post-processing, for the EQ panel analyzer) ──────
    {
        int wp = spectrumWritePos.load(std::memory_order_relaxed);
        const float* src = buffer.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i)
        {
            spectrumBuf[static_cast<size_t>(wp & (kSpectrumBufLen - 1))] = src[i];
            ++wp;
        }
        spectrumWritePos.store(wp & (kSpectrumBufLen - 1), std::memory_order_release);
    }

    // ── Output level meter ────────────────────────────────────────────────
    float peak = 0.f;
    for (int ch = 0; ch < numCh; ++ch)
        peak = std::max(peak, buffer.getMagnitude(ch, 0, numSamples));
    outputLevel.store(outputLevel.load(std::memory_order_relaxed) * 0.85f + peak * 0.15f,
                      std::memory_order_relaxed);
}
