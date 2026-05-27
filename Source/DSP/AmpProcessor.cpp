#include "AmpProcessor.h"

using Coeffs = juce::dsp::IIR::Coefficients<float>;

// ── prepare ────────────────────────────────────────────────────────────────

void AmpProcessor::prepare(double sr, int blockSize)
{
    sampleRate = sr;

    // Mono spec for linear-domain filters (base sample rate)
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<uint32_t>(blockSize);
    spec.numChannels      = 1;

    // Gate sidechain & boost filters run at base rate (outside oversampled loop)
    gateSidechain.prepare(spec);
    boostHpf.prepare(spec);
    boostMid.prepare(spec);
    boostLpf.prepare(spec);

    // Nonlinear-chain filters are prepared at 4× rate
    juce::dsp::ProcessSpec osrSpec;
    osrSpec.sampleRate       = sr * 4.0;
    osrSpec.maximumBlockSize = static_cast<uint32_t>(blockSize * 4);
    osrSpec.numChannels      = 1;

    preHpf.prepare(osrSpec);
    lpf1.prepare(osrSpec); lpf2.prepare(osrSpec);
    lpf3.prepare(osrSpec); lpf4.prepare(osrSpec);
    dc1.prepare(osrSpec);  dc2.prepare(osrSpec);
    dc3.prepare(osrSpec);  dc4.prepare(osrSpec);
    leadChunkPeak.prepare(osrSpec);
    bassFilter.prepare(osrSpec);
    midFilter.prepare(osrSpec);
    trebleFilter.prepare(osrSpec);
    presenceFilter.prepare(osrSpec);
    cabHpf.prepare(osrSpec);
    cabMidScoop.prepare(osrSpec);
    cabPresence.prepare(osrSpec);
    cabLpf.prepare(osrSpec);

    // Oversampling object (1 channel, 4× = 2^2)
    oversampling.initProcessing(static_cast<size_t>(blockSize));
    oversampling.reset();

    // Convolution (mono, base rate)
    juce::dsp::ProcessSpec convSpec { sr, static_cast<uint32_t>(blockSize), 1 };
    convolution.prepare(convSpec);

    // Reverb (stereo, base rate)
    juce::dsp::ProcessSpec stereoSpec { sr, static_cast<uint32_t>(blockSize), 2 };
    reverb.prepare(stereoSpec);

    // Chorus (stereo, base rate)
    chorus.prepare({ sr, static_cast<uint32_t>(blockSize), 2 });
    chorus.setRate(0.35f);
    chorus.setDepth(0.55f);
    chorus.setCentreDelay(7.0f);
    chorus.setFeedback(0.08f);

    // Delay buffer — 2 seconds max
    const int maxDelay = static_cast<int>(sr * 2.0);
    delayBuf.setSize(1, maxDelay, false, true, false);
    delayWritePos    = 0;
    delayTimeSamples = static_cast<int>(sr * 0.350);
    delayFeedback    = 0.35f;

    // Gate timing (base rate): 0.3 ms attack, 20 ms release
    // Deliberately tight — mirrors the accidental fast timing the gate had when
    // it ran inside the 4× oversampled loop with base-rate coefficients.
    // Deathcore gate should snap open and close, not breathe.
    gateAttCoeff = 1.0f - std::exp(-1.0f / (0.0003f * static_cast<float>(sr)));
    gateRelCoeff = std::exp(-1.0f         / (0.020f  * static_cast<float>(sr)));

    // SAG timing (base rate)
    sagAttCoeff = 1.0f - std::exp(-1.0f / (0.002f * static_cast<float>(sr)));
    sagRelCoeff = std::exp(-1.0f / (0.150f * static_cast<float>(sr)));

    updateFilters();
}

// ── setParams ──────────────────────────────────────────────────────────────

void AmpProcessor::setParams(const Params& p)
{
    const bool channelChanged = (p.channel != params.channel);
    params = p;
    updateFilters();
    if (channelChanged)
        resetFilterStates();
}

// ── updateFilters ──────────────────────────────────────────────────────────

void AmpProcessor::updateFilters()
{
    const float  g   = params.gain;
    const double osr = sampleRate * 4.0;   // oversampled rate

    // ── Gate sidechain (base rate) ────────────────────────────────────────
    // HPF at 100 Hz: removes mains hum but passes all guitar fundamentals
    // regardless of tuning (drop A, drop B, etc.)
    gateSidechain.coefficients = Coeffs::makeHighPass(sampleRate, 100.0, 0.707);

    // ── Pre-boost filters (base rate) ─────────────────────────────────────
    // Tube Screamer character: tight HPF + midrange hump + soft rolloff
    boostHpf.coefficients = Coeffs::makeHighPass( sampleRate, 720.0, 0.707);
    boostMid.coefficients = Coeffs::makePeakFilter(sampleRate, 900.0, 2.0f,
        juce::Decibels::decibelsToGain(6.0f));
    boostLpf.coefficients = Coeffs::makeLowPass(  sampleRate, 3500.0, 0.707);
    boostDrive = 6.0f;

    // ── Pre-HPF (oversampled rate) ────────────────────────────────────────
    const double hpfFreq = (params.channel == Channel::Clean)  ? 60.0
                         : (params.channel == Channel::Crunch) ? 80.0
                                                               : 120.0;
    preHpf.coefficients = Coeffs::makeHighPass(osr, hpfFreq, 0.707);

    // ── Inter-stage LPFs (oversampled rate) ───────────────────────────────
    lpf1.coefficients = Coeffs::makeLowPass(osr, 8000.0, 0.707);
    lpf2.coefficients = Coeffs::makeLowPass(osr, 6500.0, 0.707);
    lpf3.coefficients = Coeffs::makeLowPass(osr, 5500.0, 0.707);
    lpf4.coefficients = Coeffs::makeLowPass(osr, 4500.0, 0.707);

    // ── DC blockers (oversampled rate) ────────────────────────────────────
    dc1.coefficients = dc2.coefficients = dc3.coefficients = dc4.coefficients
        = Coeffs::makeHighPass(osr, 10.0, 0.707);

    // ── 6505+ inter-stage chug peak (oversampled rate) ────────────────────
    leadChunkPeak.coefficients = Coeffs::makePeakFilter(osr, 900.0, 2.2f,
        juce::Decibels::decibelsToGain(9.0f));

    // ── Tone stack (oversampled rate) ─────────────────────────────────────
    bassFilter.coefficients   = Coeffs::makeLowShelf( osr, 150.0, 0.7,
        juce::Decibels::decibelsToGain((params.bass   - 0.5f) * 28.0f));
    midFilter.coefficients    = Coeffs::makePeakFilter(osr, 650.0, 1.2f,
        juce::Decibels::decibelsToGain((params.mid    - 0.5f) * 20.0f));
    trebleFilter.coefficients = Coeffs::makeHighShelf(osr, 3000.0, 0.7,
        juce::Decibels::decibelsToGain((params.treble - 0.5f) * 28.0f));

    // ── Presence (oversampled rate) ───────────────────────────────────────
    const double presFreq = (params.channel == Channel::Lead)   ? 3800.0
                          : (params.channel == Channel::Crunch) ? 2500.0
                                                                 : 1800.0;
    presenceFilter.coefficients = Coeffs::makePeakFilter(osr, presFreq, 1.5f,
        juce::Decibels::decibelsToGain((params.presence - 0.5f) * 16.0f));

    // ── Cab sim (oversampled rate) — Celestion V30 character ─────────────
    // HPF: speaker rolloff below 100 Hz
    cabHpf.coefficients      = Coeffs::makeHighPass( osr, 100.0,  0.707);
    // Cabinet dip: ~800 Hz box resonance notch (gives presence without honkiness)
    cabMidScoop.coefficients = Coeffs::makePeakFilter(osr, 800.0, 1.2f,
        juce::Decibels::decibelsToGain(-5.0f));
    // V30 presence peak: 2.5–3 kHz cone breakup / voice coil
    cabPresence.coefficients = Coeffs::makePeakFilter(osr, 2800.0, 1.8f,
        juce::Decibels::decibelsToGain(4.5f));
    // Air rolloff: V30 rolls off sharply above 5 kHz
    cabLpf.coefficients      = Coeffs::makeLowPass(  osr, 5000.0, 0.75);

    // ── Reverb ────────────────────────────────────────────────────────────
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = 0.78f;
        rp.damping    = 0.18f;
        rp.wetLevel   = params.reverb * 0.50f;
        rp.dryLevel   = 1.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters(rp);
    }

    // ── Chorus ────────────────────────────────────────────────────────────
    chorus.setMix(params.chorus * 0.55f);

    // ── Gate threshold ────────────────────────────────────────────────────
    gateThresh = (params.gate < 0.01f)
                     ? 0.0f
                     : juce::Decibels::decibelsToGain(-80.0f + params.gate * 60.0f);

    // ── Cached drive values ───────────────────────────────────────────────
    cleanDrive = 1.0f + g * 4.5f;
    cleanNorm  = 1.0f / (1.0f - std::exp(-cleanDrive));

    crunchDrive1 = 3.0f + g * 14.0f;
    crunchDrive2 = 2.0f + g * 8.0f;

    leadDrive1 = 8.0f  + g * 62.0f;
    leadDrive2 = 5.0f  + g * 30.0f;
    leadDrive3 = 3.0f  + g * 18.0f;
    leadDrive4 = 2.0f  + g * 8.0f;
}

// ── resetFilterStates ──────────────────────────────────────────────────────

void AmpProcessor::resetFilterStates()
{
    gateSidechain.reset();
    boostHpf.reset();
    boostMid.reset();
    boostLpf.reset();
    preHpf.reset();
    lpf1.reset(); lpf2.reset(); lpf3.reset(); lpf4.reset();
    dc1.reset();  dc2.reset();  dc3.reset();  dc4.reset();
    leadChunkPeak.reset();
    bassFilter.reset(); midFilter.reset(); trebleFilter.reset();
    presenceFilter.reset();
    cabHpf.reset(); cabMidScoop.reset(); cabPresence.reset(); cabLpf.reset();

    gateEnvelope = 0.0f;
    sagEnvelope  = 0.0f;
    lfoPhase     = 0.0f;
    delayBuf.clear();
    delayWritePos = 0;
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

// ── Per-sample helpers ─────────────────────────────────────────────────────

float AmpProcessor::applyGateSample(float x) noexcept
{
    if (gateThresh == 0.0f) return x;

    // Level detection on bandpass-filtered signal — avoids false triggers from hum
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
    return x * gv;
}

float AmpProcessor::processCleanSample(float x) noexcept
{
    // Stage 1 — asymmetric warmth
    x = waveshape(x * cleanDrive) * cleanNorm * 0.4f;
    x = dc1.processSample(x);
    x = lpf1.processSample(x);
    // Stage 2 — very light second harmonic bloom (class A character)
    const float stage2Drive = 1.0f + params.gain * 1.2f;
    x = waveshape(x * stage2Drive) * 0.9f;
    x = dc2.processSample(x);
    return x;
}

float AmpProcessor::processCrunchSample(float x) noexcept
{
    // Stage 1 — asymmetric even-harmonic warmth (vintage character)
    x = waveshape(x * crunchDrive1);
    x = dc1.processSample(x);
    x = lpf1.processSample(x);

    // SAG: measure level after stage 1, scale remaining stages
    if (params.sag > 0.001f)
    {
        const float lvl = std::abs(x);
        if (lvl > sagEnvelope)
            sagEnvelope += (lvl - sagEnvelope) * sagAttCoeff;
        else
            sagEnvelope *= sagRelCoeff;
    }
    const float sagScale = 1.0f - params.sag * sagEnvelope * 0.45f;

    // Stage 2 — soft tanh (adds odd harmonics, thickens the crunch)
    x = std::tanh(x * crunchDrive2 * sagScale);
    x = dc2.processSample(x);
    x = lpf2.processSample(x);

    // Stage 3 — light triode clip for compression and warmth
    x = triodeClip(x, (2.0f + params.gain * 5.0f) * sagScale);
    x = dc3.processSample(x);
    x = lpf3.processSample(x);
    return x;
}

float AmpProcessor::processLeadSample(float x) noexcept
{
    // Stage 1 — symmetric tanh → odd harmonics (aggressive)
    x = std::tanh(x * leadDrive1);
    x = dc1.processSample(x);
    x = lpf1.processSample(x);

    // SAG: measure level after stage 1
    if (params.sag > 0.001f)
    {
        const float lvl = std::abs(x);
        if (lvl > sagEnvelope)
            sagEnvelope += (lvl - sagEnvelope) * sagAttCoeff;
        else
            sagEnvelope *= sagRelCoeff;
    }
    const float sagScale = 1.0f - params.sag * sagEnvelope * 0.45f;

    // Inter-stage 900 Hz boost → "chug" into stage 2
    x = leadChunkPeak.processSample(x);

    // Stage 2
    x = std::tanh(x * leadDrive2 * sagScale);
    x = dc2.processSample(x);
    x = lpf2.processSample(x);

    // Stage 3 — asymmetric triode clip (tube plate characteristic)
    x = triodeClip(x, leadDrive3 * sagScale);
    x = dc3.processSample(x);
    x = lpf3.processSample(x);

    // Stage 4 — final asymmetric compress / warmth
    x = waveshape(x * leadDrive4 * sagScale);
    x = dc4.processSample(x);
    x = lpf4.processSample(x);
    return x;
}

// ── process ────────────────────────────────────────────────────────────────

void AmpProcessor::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    float* ch0 = buffer.getWritePointer(0);

    // ── Gate at base rate — on raw signal, before boost ──
    // Must run first so the sidechain sees the true guitar level,
    // not the boosted/compressed/HPF'd post-808 signal.
    if (gateThresh > 0.0f)
    {
        for (int i = 0; i < numSamples; ++i)
            ch0[i] = applyGateSample(ch0[i]);
    }

    // ── Pre-boost (808 / Tube Screamer) — runs at base rate ──
    if (params.boost)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float x = boostHpf.processSample(ch0[i]);
            x = boostMid.processSample(x);         // TS midrange hump first
            x = std::tanh(x * boostDrive);         // soft saturation
            x = boostLpf.processSample(x);
            ch0[i] = x * 0.65f;                    // normalize
        }
    }

    // ── Upsample → nonlinear chain → downsample ──
    {
        juce::dsp::AudioBlock<float> inputBlock(buffer);
        auto monoBlock = inputBlock.getSingleChannelBlock(0);
        auto upBlock   = oversampling.processSamplesUp(monoBlock);

        const int upSamples = static_cast<int>(upBlock.getNumSamples());
        float* up = upBlock.getChannelPointer(0);

        for (int i = 0; i < upSamples; ++i)
        {
            float x = preHpf.processSample(up[i]);

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

            if (!irLoaded.load())
            {
                x = cabHpf.processSample(x);
                x = cabMidScoop.processSample(x);
                x = cabPresence.processSample(x);
                x = cabLpf.processSample(x);
            }

            x = softLimit(x);
            up[i] = x;
        }

        oversampling.processSamplesDown(monoBlock);
    }

    // ── Mono delay (before stereo spread) ──
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

    // ── Tremolo LFO (mono, before stereo spread) ──
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

    // ── IR convolution (mono) ──
    if (irLoaded.load())
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto mono = block.getSingleChannelBlock(0);
        convolution.process(juce::dsp::ProcessContextReplacing<float>(mono));
    }

    // ── Copy mono → stereo ──
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    // ── Stereo FX: chorus then reverb ──
    if (params.chorus > 0.001f)
    {
        juce::dsp::AudioBlock<float> stereoBlock(buffer);
        chorus.process(juce::dsp::ProcessContextReplacing<float>(stereoBlock));
    }

    if (params.reverb > 0.001f)
    {
        juce::dsp::AudioBlock<float> revBlock(buffer);
        reverb.process(juce::dsp::ProcessContextReplacing<float>(revBlock));
    }

    // ── Output volume ──
    buffer.applyGain(params.volume * 0.35f);

    // ── Level meter ──
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = std::max(peak, buffer.getMagnitude(ch, 0, numSamples));
    outputLevel.store(outputLevel.load() * 0.85f + peak * 0.15f);
}
