#include "AmpProcessor.h"

using Coeffs = juce::dsp::IIR::Coefficients<float>;

// ── prepare ────────────────────────────────────────────────────────────────

void AmpProcessor::prepare(double sr, int blockSize)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<uint32_t>(blockSize);
    spec.numChannels      = 1;

    preHpf.prepare(spec);
    lpf1.prepare(spec); lpf2.prepare(spec); lpf3.prepare(spec); lpf4.prepare(spec);
    dc1.prepare(spec);  dc2.prepare(spec);  dc3.prepare(spec);  dc4.prepare(spec);
    leadChunkPeak.prepare(spec);
    bassFilter.prepare(spec);
    midFilter.prepare(spec);
    trebleFilter.prepare(spec);
    presenceFilter.prepare(spec);
    cabHpf.prepare(spec); cabMidScoop.prepare(spec); cabLpf.prepare(spec);

    // Convolution (mono)
    juce::dsp::ProcessSpec convSpec { sr, static_cast<uint32_t>(blockSize), 1 };
    convolution.prepare(convSpec);

    // Reverb (stereo)
    juce::dsp::ProcessSpec stereoSpec { sr, static_cast<uint32_t>(blockSize), 2 };
    reverb.prepare(stereoSpec);

    // Chorus (stereo)
    chorus.prepare({ sr, static_cast<uint32_t>(blockSize), 2 });
    chorus.setRate(0.35f);
    chorus.setDepth(0.55f);
    chorus.setCentreDelay(7.0f);
    chorus.setFeedback(0.08f);

    // Delay buffer — 2 seconds max at any sample rate
    const int maxDelay = static_cast<int>(sr * 2.0);
    delayBuf.setSize(1, maxDelay, false, true, false);
    delayWritePos    = 0;
    delayTimeSamples = static_cast<int>(sr * 0.350);  // 350 ms
    delayFeedback    = 0.35f;

    // Gate timing
    gateAttCoeff = 1.0f - std::exp(-1.0f / (0.001f * static_cast<float>(sr)));
    gateRelCoeff = std::exp(-1.0f / (0.060f * static_cast<float>(sr)));

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
    const float g = params.gain;

    // Pre-HPF
    const double hpf = (params.channel == Channel::Clean)  ? 60.0
                     : (params.channel == Channel::Crunch) ? 80.0
                                                           : 120.0;
    preHpf.coefficients = Coeffs::makeHighPass(sampleRate, hpf, 0.707);

    // Inter-stage LPFs
    lpf1.coefficients = Coeffs::makeLowPass(sampleRate, 8000.0, 0.707);
    lpf2.coefficients = Coeffs::makeLowPass(sampleRate, 6500.0, 0.707);
    lpf3.coefficients = Coeffs::makeLowPass(sampleRate, 5500.0, 0.707);
    lpf4.coefficients = Coeffs::makeLowPass(sampleRate, 4500.0, 0.707);

    // DC blockers
    dc1.coefficients = dc2.coefficients = dc3.coefficients = dc4.coefficients
        = Coeffs::makeHighPass(sampleRate, 10.0, 0.707);

    // 6505+ inter-stage "chug" peak: 900 Hz boost going into stage 2
    leadChunkPeak.coefficients = Coeffs::makePeakFilter(sampleRate, 900.0, 2.2f,
        juce::Decibels::decibelsToGain(9.0f));

    // Tone stack
    bassFilter.coefficients   = Coeffs::makeLowShelf( sampleRate, 150.0, 0.7,
        juce::Decibels::decibelsToGain((params.bass   - 0.5f) * 28.0f));
    midFilter.coefficients    = Coeffs::makePeakFilter(sampleRate, 650.0, 1.2f,
        juce::Decibels::decibelsToGain((params.mid    - 0.5f) * 20.0f));
    trebleFilter.coefficients = Coeffs::makeHighShelf(sampleRate, 3000.0, 0.7,
        juce::Decibels::decibelsToGain((params.treble - 0.5f) * 28.0f));

    // Presence — frequency by channel
    const double presFreq = (params.channel == Channel::Lead)   ? 3800.0
                          : (params.channel == Channel::Crunch) ? 2500.0
                                                                 : 1800.0;
    presenceFilter.coefficients = Coeffs::makePeakFilter(sampleRate, presFreq, 1.5f,
        juce::Decibels::decibelsToGain((params.presence - 0.5f) * 16.0f));

    // Cab sim
    cabHpf.coefficients      = Coeffs::makeHighPass( sampleRate, 80.0,  0.707);
    cabMidScoop.coefficients = Coeffs::makePeakFilter(sampleRate, 400.0, 1.0f,
        juce::Decibels::decibelsToGain(-7.0f));
    cabLpf.coefficients      = Coeffs::makeLowPass(  sampleRate, 5500.0, 0.85);

    // FX — reverb
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = 0.78f;
        rp.damping    = 0.18f;  // very bright / shimmery
        rp.wetLevel   = params.reverb * 0.50f;
        rp.dryLevel   = 1.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters(rp);
    }

    // FX — chorus mix (rate/depth/delay set once in prepare)
    chorus.setMix(params.chorus * 0.55f);

    // Gate threshold
    gateThresh = (params.gate < 0.01f)
                     ? 0.0f
                     : juce::Decibels::decibelsToGain(-80.0f + params.gate * 60.0f);

    // Cached drive values
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
    preHpf.reset();
    lpf1.reset(); lpf2.reset(); lpf3.reset(); lpf4.reset();
    dc1.reset();  dc2.reset();  dc3.reset();  dc4.reset();
    leadChunkPeak.reset();
    bassFilter.reset(); midFilter.reset(); trebleFilter.reset();
    presenceFilter.reset();
    cabHpf.reset(); cabMidScoop.reset(); cabLpf.reset();
    gateEnvelope = 0.0f;
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

    const float level = std::abs(x);
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
    return waveshape(x * cleanDrive) * cleanNorm * 0.4f;
}

float AmpProcessor::processCrunchSample(float x) noexcept
{
    x = waveshape(x * crunchDrive1);
    x = dc1.processSample(x);
    x = lpf1.processSample(x);

    x = waveshape(x * crunchDrive2);
    x = dc2.processSample(x);
    x = lpf2.processSample(x);
    return x;
}

float AmpProcessor::processLeadSample(float x) noexcept
{
    // Stage 1 — symmetric tanh → odd harmonics
    x = std::tanh(x * leadDrive1);
    x = dc1.processSample(x);
    x = lpf1.processSample(x);

    // Inter-stage 900 Hz boost → gets harmonically enriched by stage 2 = "chug"
    x = leadChunkPeak.processSample(x);

    // Stage 2
    x = std::tanh(x * leadDrive2);
    x = dc2.processSample(x);
    x = lpf2.processSample(x);

    // Stage 3 — hard clip + reshape (power-amp character)
    x = hardClip(x * leadDrive3, 1.0f);
    x = std::tanh(x);
    x = dc3.processSample(x);
    x = lpf3.processSample(x);

    // Stage 4 — final asymmetric compress / warmth
    x = waveshape(x * leadDrive4);
    x = dc4.processSample(x);
    x = lpf4.processSample(x);
    return x;
}

// ── process ────────────────────────────────────────────────────────────────

void AmpProcessor::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    float* ch0 = buffer.getWritePointer(0);

    // ── Mono non-linear chain ──
    for (int i = 0; i < numSamples; ++i)
    {
        float x = preHpf.processSample(ch0[i]);
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

        if (!irLoaded.load())
        {
            x = cabHpf.processSample(x);
            x = cabMidScoop.processSample(x);
            x = cabLpf.processSample(x);
        }

        // ── Output limiter ──
        // The tone-stack shelves + presence peak can push the signal well past
        // ±1.0 (e.g. treble +5 dB then presence +5 dB = ×3.2 at 3-4 kHz).
        // Hard-clamp here so none of that becomes digital distortion.
        x = juce::jlimit(-1.0f, 1.0f, x);

        // ── Mono delay (before stereo spread) ──
        if (params.delay > 0.001f)
        {
            const int maxSize = delayBuf.getNumSamples();
            int readPos = delayWritePos - delayTimeSamples;
            if (readPos < 0) readPos += maxSize;

            const float delayed = delayBuf.getSample(0, readPos);
            delayBuf.setSample(0, delayWritePos, x + delayed * delayFeedback);
            delayWritePos = (delayWritePos + 1) % maxSize;

            x += delayed * params.delay * 0.7f;
        }

        ch0[i] = x;
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
