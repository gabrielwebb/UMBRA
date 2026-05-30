#include "EqPanel.h"

using Coeffs = juce::dsp::IIR::Coefficients<float>;

// ── Band colours ───────────────────────────────────────────────────────────
juce::Colour EqPanel::bandColour(int b) noexcept
{
    switch (b)
    {
        case 0:  return juce::Colour(0xff4488ff);  // low shelf  — blue
        case 1:  return juce::Colour(0xffff8833);  // low-mid peak — orange
        case 2:  return juce::Colour(0xffffff44);  // high-mid peak — yellow
        default: return juce::Colour(0xffff4444);  // high shelf — red
    }
}

// ── Constructor / Destructor ───────────────────────────────────────────────

EqPanel::EqPanel(AmpProcessor& p) : proc(p)
{
    // Pre-compute Hanning window
    for (int i = 0; i < kFftSize; ++i)
        hanningWin[i] = 0.5f - 0.5f * std::cos(
            juce::MathConstants<float>::twoPi * i / (kFftSize - 1));

    setInterceptsMouseClicks(true, false);
    startTimerHz(30);
}

EqPanel::~EqPanel() { stopTimer(); }

// ── External band update ───────────────────────────────────────────────────

void EqPanel::setExternalBands(const EqBand src[4])
{
    for (int b = 0; b < kNumBands; ++b) bands[b] = src[b];
    repaint();
}

// ── Coordinate mapping ─────────────────────────────────────────────────────

juce::Rectangle<float> EqPanel::plotArea() const noexcept
{
    return getLocalBounds().toFloat().reduced(4.f, 4.f).withTrimmedBottom(20.f);
}

float EqPanel::freqToX(float f) const noexcept
{
    const auto a = plotArea();
    const float t = (std::log10(f) - std::log10(kFreqLow))
                  / (std::log10(kFreqHigh) - std::log10(kFreqLow));
    return a.getX() + t * a.getWidth();
}

float EqPanel::gainToY(float db) const noexcept
{
    const auto a = plotArea();
    const float t = (db - kGainMin) / (kGainMax - kGainMin);
    return a.getBottom() - t * a.getHeight();
}

float EqPanel::xToFreq(float x) const noexcept
{
    const auto a = plotArea();
    const float t = (x - a.getX()) / a.getWidth();
    return kFreqLow * std::pow(kFreqHigh / kFreqLow, juce::jlimit(0.f, 1.f, t));
}

float EqPanel::yToGain(float y) const noexcept
{
    const auto a = plotArea();
    const float t = (a.getBottom() - y) / a.getHeight();
    return kGainMin + juce::jlimit(0.f, 1.f, t) * (kGainMax - kGainMin);
}

// ── Spectrum FFT ───────────────────────────────────────────────────────────

void EqPanel::computeSpectrum()
{
    // Copy chronological samples from the processor's spectrum ring buffer.
    std::array<float, kFftSize> snap;
    proc.getSpectrumSnapshot(snap.data(), kFftSize);

    // Apply Hanning window
    for (int i = 0; i < kFftSize; ++i)
        fftData[i] = snap[i] * hanningWin[i];
    std::fill(fftData.begin() + kFftSize, fftData.end(), 0.f);

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    // Smooth with ~20 ms decay at 30 Hz
    constexpr float kDecay = 0.80f;
    for (int i = 0; i < kBins; ++i)
    {
        const float mag = juce::Decibels::gainToDecibels(
            fftData[i] / static_cast<float>(kFftSize));
        smoothMag[i] = std::max(kDecay * smoothMag[i], mag);
    }
}

juce::Path EqPanel::buildSpectrumPath(juce::Rectangle<float> area) const
{
    const double sr  = proc.getSampleRate();
    const float  nyq = static_cast<float>(sr * 0.5);
    const float  bw  = static_cast<float>(sr) / static_cast<float>(kFftSize);

    juce::Path p;
    bool started = false;

    for (int i = 1; i < kBins; ++i)
    {
        const float freq = static_cast<float>(i) * bw;
        if (freq < kFreqLow || freq > kFreqHigh) continue;

        const float x = freqToX(freq);
        const float dB = juce::jlimit(-80.f, 0.f, smoothMag[i]);
        // Map dB: 0 dB = midpoint of area, -80 dB = bottom
        const float t = (dB + 80.f) / 80.f;
        const float y = area.getBottom() - t * area.getHeight();

        if (!started) { p.startNewSubPath(x, area.getBottom()); p.lineTo(x, y); started = true; }
        else            p.lineTo(x, y);
        (void)nyq;
    }
    if (started)
    {
        p.lineTo(freqToX(kFreqHigh), area.getBottom());
        p.closeSubPath();
    }
    return p;
}

// ── EQ curve ───────────────────────────────────────────────────────────────

float EqPanel::evalEqDb(float f) const noexcept
{
    float total = 0.f;
    const double sr = proc.getSampleRate();

    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& band = bands[b];
        if (!band.enabled || std::abs(band.gainDb) < 0.001f) continue;

        const double freq = juce::jlimit(20.0, sr * 0.49, static_cast<double>(band.freq));
        const float  gain = juce::Decibels::decibelsToGain(band.gainDb);
        const float  q    = juce::jlimit(0.1f, 10.f, band.q);

        juce::dsp::IIR::Coefficients<float>::Ptr c;
        switch (band.type)
        {
            case EqBand::LowShelf:  c = Coeffs::makeLowShelf (sr, freq, q, gain); break;
            case EqBand::HighShelf: c = Coeffs::makeHighShelf(sr, freq, q, gain); break;
            case EqBand::Notch:     c = Coeffs::makePeakFilter(sr, freq, q, juce::Decibels::decibelsToGain(-30.f)); break;
            default:                c = Coeffs::makePeakFilter(sr, freq, q, gain); break;
        }
        if (c != nullptr)
            total += juce::Decibels::gainToDecibels(
                static_cast<float>(c->getMagnitudeForFrequency(static_cast<double>(f), sr)));
    }
    return total;
}

juce::Path EqPanel::buildEqCurve(juce::Rectangle<float> area) const
{
    juce::Path p;
    const float x0 = area.getX();
    const float x1 = area.getRight();
    bool started = false;

    for (float x = x0; x <= x1; x += 1.5f)
    {
        const float f  = xToFreq(x);
        const float db = evalEqDb(f);
        const float y  = gainToY(db);

        if (!started) { p.startNewSubPath(x, y); started = true; }
        else            p.lineTo(x, y);
    }
    return p;
}

// ── Paint ──────────────────────────────────────────────────────────────────

void EqPanel::paint(juce::Graphics& g)
{
    const auto b  = getLocalBounds().toFloat();
    const auto pa = plotArea();

    // ── Background ──────────────────────────────────────────────────────
    g.setColour(juce::Colour(0xff0d0b09));
    g.fillRoundedRectangle(b, 4.f);
    g.setColour(juce::Colour(0xff2a2724));
    g.drawRoundedRectangle(b.reduced(0.5f), 4.f, 1.f);

    // ── Grid ────────────────────────────────────────────────────────────
    // Vertical frequency grid lines
    static const float gridFreqs[] = { 50,100,200,500,1000,2000,5000,10000 };
    for (auto freq : gridFreqs)
    {
        const float x = freqToX(freq);
        g.setColour(juce::Colour(0xff1e1c1a));
        g.fillRect(x, pa.getY(), 1.f, pa.getHeight());

        // Frequency labels
        juce::String lbl = (freq >= 1000.f) ?
            juce::String(static_cast<int>(freq / 1000)) + "k" :
            juce::String(static_cast<int>(freq));
        g.setColour(juce::Colour(0xff383432));
        g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
        g.drawText(lbl, static_cast<int>(x) - 14, static_cast<int>(pa.getBottom()) + 2,
                   28, 14, juce::Justification::centred, false);
    }

    // Horizontal dB grid lines
    for (float db : { -12.f, -6.f, 0.f, 6.f, 12.f })
    {
        const float y = gainToY(db);
        g.setColour(db == 0.f ? juce::Colour(0xff282420) : juce::Colour(0xff1a1816));
        g.fillRect(pa.getX(), y, pa.getWidth(), 1.f);
        if (std::abs(db) <= 12.001f)
        {
            g.setColour(juce::Colour(0xff383432));
            g.setFont(juce::Font(juce::FontOptions().withHeight(7.5f)));
            g.drawText((db > 0 ? "+" : "") + juce::String(static_cast<int>(db)),
                       static_cast<int>(pa.getX()) - 22, static_cast<int>(y) - 5,
                       20, 10, juce::Justification::centredRight, false);
        }
    }

    // ── Spectrum (filled, semi-transparent) ─────────────────────────────
    {
        juce::Path spec = buildSpectrumPath(pa);
        g.setColour(juce::Colour(0x22cc5500));
        g.fillPath(spec);
        g.setColour(juce::Colour(0x44cc7700));
        g.strokePath(spec, juce::PathStrokeType(0.8f));
    }

    // ── EQ curve ────────────────────────────────────────────────────────
    {
        const bool anyEnabled = [this] {
            for (int i = 0; i < kNumBands; ++i)
                if (bands[i].enabled) return true;
            return false;
        }();

        if (anyEnabled)
        {
            juce::Path eq = buildEqCurve(pa);
            g.setColour(juce::Colour(0x55ffffff));
            g.strokePath(eq, juce::PathStrokeType(1.5f));
            g.setColour(juce::Colour(0xccffffff));
            g.strokePath(eq, juce::PathStrokeType(1.0f));
        }
        else
        {
            // Flat line at 0 dB
            g.setColour(juce::Colour(0xff333130));
            g.fillRect(pa.getX(), gainToY(0.f), pa.getWidth(), 1.f);
        }
    }

    // ── Band nodes ───────────────────────────────────────────────────────
    for (int i = 0; i < kNumBands; ++i)
    {
        const auto& band = bands[i];
        const float nx   = freqToX(band.freq);
        const float ny   = gainToY(band.enabled ? band.gainDb : 0.f);
        const juce::Colour col = bandColour(i);
        const bool  sel  = (i == selectedBand);
        const float nr   = sel ? 9.f : 7.f;

        // Glow
        if (band.enabled || sel)
        {
            g.setColour(col.withAlpha(0.25f));
            g.fillEllipse(nx - nr * 2.f, ny - nr * 2.f, nr * 4.f, nr * 4.f);
        }

        // Fill
        g.setColour(band.enabled ? col : col.withAlpha(0.3f));
        g.fillEllipse(nx - nr, ny - nr, nr * 2.f, nr * 2.f);

        // Rim
        g.setColour(col.brighter(0.3f));
        g.drawEllipse(nx - nr, ny - nr, nr * 2.f, nr * 2.f, 1.0f);

        // Band number
        g.setColour(juce::Colours::white.withAlpha(band.enabled ? 0.8f : 0.3f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(8.f).withStyle("Bold")));
        g.drawText(juce::String(i + 1),
                   static_cast<int>(nx - nr), static_cast<int>(ny - nr),
                   static_cast<int>(nr * 2), static_cast<int>(nr * 2),
                   juce::Justification::centred, false);
    }

    // ── Selected band info strip ─────────────────────────────────────────
    if (selectedBand >= 0)
    {
        const auto& band = bands[selectedBand];
        const juce::Colour col = bandColour(selectedBand);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
        g.setColour(col.brighter(0.2f));

        juce::String freqStr = band.freq >= 1000.f
            ? juce::String(band.freq / 1000.f, 1) + " kHz"
            : juce::String(static_cast<int>(band.freq)) + " Hz";
        juce::String gainStr = (band.gainDb >= 0 ? "+" : "") +
                               juce::String(band.gainDb, 1) + " dB";

        g.drawText(freqStr + "   " + gainStr + "   Q " +
                   juce::String(band.q, 1) + "   " +
                   (band.enabled ? "ON" : "OFF"),
                   static_cast<int>(pa.getX()), static_cast<int>(pa.getBottom()) + 2,
                   static_cast<int>(pa.getWidth()), 16,
                   juce::Justification::centred, false);
    }
}

// ── Timer ──────────────────────────────────────────────────────────────────

void EqPanel::timerCallback()
{
    computeSpectrum();
    repaint();
}

// ── Mouse interaction ─────────────────────────────────────────────────────

void EqPanel::mouseDown(const juce::MouseEvent& e)
{
    selectedBand = -1;
    const float mx = static_cast<float>(e.x);
    const float my = static_cast<float>(e.y);

    // Find which node was clicked (nearest within 14px)
    float best = 14.f;
    for (int i = 0; i < kNumBands; ++i)
    {
        const float nx = freqToX(bands[i].freq);
        const float ny = gainToY(bands[i].enabled ? bands[i].gainDb : 0.f);
        const float dist = std::sqrt((mx - nx) * (mx - nx) + (my - ny) * (my - ny));
        if (dist < best) { best = dist; selectedBand = i; }
    }

    // If click is not near any node, deselect and do nothing
    repaint();
}

void EqPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (selectedBand < 0) return;

    auto& band = bands[selectedBand];
    band.freq   = juce::jlimit(kFreqLow, kFreqHigh,  xToFreq(static_cast<float>(e.x)));
    band.gainDb = juce::jlimit(kGainMin,  kGainMax,   yToGain(static_cast<float>(e.y)));

    if (!band.enabled)
    {
        band.enabled = true;  // auto-enable when user drags a band
    }

    if (onEqChanged) onEqChanged();
    repaint();
}

void EqPanel::mouseDoubleClick(const juce::MouseEvent& e)
{
    const float mx = static_cast<float>(e.x);
    const float my = static_cast<float>(e.y);

    for (int i = 0; i < kNumBands; ++i)
    {
        const float nx = freqToX(bands[i].freq);
        const float ny = gainToY(bands[i].enabled ? bands[i].gainDb : 0.f);
        const float dist = std::sqrt((mx - nx) * (mx - nx) + (my - ny) * (my - ny));
        if (dist < 14.f)
        {
            bands[i].enabled = !bands[i].enabled;
            if (onEqChanged) onEqChanged();
            repaint();
            return;
        }
    }
}
