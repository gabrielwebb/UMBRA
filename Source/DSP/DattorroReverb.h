#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <JuceHeader.h>

// ── Dattorro Plate Reverb ──────────────────────────────────────────────────
// Based on: Jon Dattorro, "Effect Design Part 1: Reverberator and Other
// Filters", J. Audio Eng. Soc., Vol 45 No 9, 1997.
//
// Signal flow:
//   mono in → bandwidth LP → 4-stage APF diffusion → two cross-coupled tanks
//   Tank: modulated APF → long delay → LP → APF → long delay
//   Stereo output: multiple taps from both tanks (decorrelated by offset)
//
// All delay lengths are scaled from Dattorro's 29761 Hz reference rate to the
// actual sample rate at prepare() time.  The reverb runs at BASE rate (no
// oversampling needed — it's a linear/allpass structure).

class DattorroReverb
{
public:
    // ── Public parameters (set before calling processSample) ──────────────
    float decay      = 0.70f;   // 0–1  → mapped to 0.25–0.98 tail length
    float bandwidth  = 0.88f;   // 0–1  → input high-freq loss
    float damping    = 0.35f;   // 0–1  → HF absorption in the tank
    float wetLevel   = 0.30f;   // output wet gain
    float dryLevel   = 1.00f;   // output dry gain

    void prepare(double sr) noexcept
    {
        sampleRate = sr;
        const double s = sr / kRef;

        // ── Delay lengths (Dattorro Table 1, scaled) ──────────────────────
        preDelayLen  = len(s,   224);
        apf1Len      = len(s,   142);
        apf2Len      = len(s,   107);
        apf3Len      = len(s,   379);
        apf4Len      = len(s,   277);
        // Tank left
        tapfL1Len    = len(s,   672);
        tdL1Len      = len(s,  4453);
        tapfL2Len    = len(s,  1800);
        tdL2Len      = len(s,  3720);
        // Tank right
        tapfR1Len    = len(s,   908);
        tdR1Len      = len(s,  4217);
        tapfR2Len    = len(s,  2656);
        tdR2Len      = len(s,  3163);

        const int cap = len(s, 6000);
        auto mk = [&](std::vector<float>& v) { v.assign(static_cast<size_t>(cap), 0.f); };
        mk(preDelay);
        mk(apf1); mk(apf2); mk(apf3); mk(apf4);
        mk(tapfL1); mk(tdL1); mk(tapfL2); mk(tdL2);
        mk(tapfR1); mk(tdR1); mk(tapfR2); mk(tdR2);

        reset();
    }

    void reset() noexcept
    {
        for (auto* b : { &preDelay, &apf1, &apf2, &apf3, &apf4,
                         &tapfL1, &tdL1, &tapfL2, &tdL2,
                         &tapfR1, &tdR1, &tapfR2, &tdR2 })
            std::fill(b->begin(), b->end(), 0.f);
        pdPos = a1p = a2p = a3p = a4p = 0;
        tl1p = dl1p = tl2p = dl2p = 0;
        tr1p = dr1p = tr2p = dr2p = 0;
        lpBW = lpTL = lpTR = 0.f;
        feedL = feedR = 0.f;
        lfoPhase = 0.f;
    }

    // Process one stereo sample pair in-place.
    void processSample(float& inL, float& inR) noexcept
    {
        const float dry = (inL + inR) * 0.5f;

        // ── Pre-delay ──────────────────────────────────────────────────────
        write(preDelay, pdPos, dry);
        const float pd = read(preDelay, pdPos, preDelayLen);

        // ── Bandwidth LP ──────────────────────────────────────────────────
        const float bw = juce::jlimit(0.001f, 0.9999f, bandwidth);
        lpBW += bw * (pd - lpBW);

        // ── Input diffuser (4 all-pass stages) ───────────────────────────
        float x = lpBW;
        x = apf(x, apf1, a1p, apf1Len,  0.750f);
        x = apf(x, apf2, a2p, apf2Len,  0.750f);
        x = apf(x, apf3, a3p, apf3Len,  0.625f);
        x = apf(x, apf4, a4p, apf4Len,  0.625f);

        // ── LFO for modulated APFs ─────────────────────────────────────────
        const float lfoInc = juce::MathConstants<float>::twoPi * 0.5f
                             / static_cast<float>(sampleRate);
        lfoPhase = std::fmod(lfoPhase + lfoInc, juce::MathConstants<float>::twoPi);
        const float modL = std::sin(lfoPhase)         * kModDepth;
        const float modR = std::sin(lfoPhase + 1.5f)  * kModDepth;

        // ── Mapped parameters ─────────────────────────────────────────────
        const float decMapped  = 0.25f + decay    * 0.73f;   // 0.25..0.98
        const float dampCoeff  = 1.f   - damping  * 0.90f;   // 1.0..0.1

        // ── Tank left ─────────────────────────────────────────────────────
        float tl = x + feedR * decMapped;
        tl = apfMod(tl, tapfL1, tl1p, tapfL1Len, 0.7f, modL);
        write(tdL1, dl1p, tl);
        tl = read(tdL1, dl1p, tdL1Len);
        lpTL += dampCoeff * (tl - lpTL);
        tl = lpTL * decMapped;
        tl = apf(tl, tapfL2, tl2p, tapfL2Len, 0.5f);
        write(tdL2, dl2p, tl);
        feedL = read(tdL2, dl2p, tdL2Len);

        // ── Tank right ────────────────────────────────────────────────────
        float tr = x + feedL * decMapped;
        tr = apfMod(tr, tapfR1, tr1p, tapfR1Len, 0.7f, modR);
        write(tdR1, dr1p, tr);
        tr = read(tdR1, dr1p, tdR1Len);
        lpTR += dampCoeff * (tr - lpTR);
        tr = lpTR * decMapped;
        tr = apf(tr, tapfR2, tr2p, tapfR2Len, 0.5f);
        write(tdR2, dr2p, tr);
        feedR = read(tdR2, dr2p, tdR2Len);

        // ── Output taps (Dattorro Table 2, 6 taps per channel) ────────────
        const float wetL =
              0.6f * read(tdL1,  dl1p, tdL1Len  * 266 / 4453)
            + 0.6f * read(tdL1,  dl1p, tdL1Len  * 2974/ 4453)
            - 0.6f * read(tapfL2,tl2p, tapfL2Len* 1913/ 1800)
            + 0.6f * read(tdL2,  dl2p, tdL2Len  * 1996/ 3720)
            - 0.6f * read(tdR1,  dr1p, tdR1Len  * 1990/ 4217)
            - 0.6f * read(tapfR2,tr2p, tapfR2Len* 187 / 2656)
            - 0.6f * read(tdR2,  dr2p, tdR2Len  * 1066/ 3163);

        const float wetR =
              0.6f * read(tdR1,  dr1p, tdR1Len  * 266 / 4217)
            + 0.6f * read(tdR1,  dr1p, tdR1Len  * 2974/ 4217)
            - 0.6f * read(tapfR2,tr2p, tapfR2Len* 1913/ 2656)
            + 0.6f * read(tdR2,  dr2p, tdR2Len  * 1996/ 3163)
            - 0.6f * read(tdL1,  dl1p, tdL1Len  * 1990/ 4453)
            - 0.6f * read(tapfL2,tl2p, tapfL2Len* 187 / 1800)
            - 0.6f * read(tdL2,  dl2p, tdL2Len  * 1066/ 3720);

        inL = dry * dryLevel + wetL * wetLevel;
        inR = dry * dryLevel + wetR * wetLevel;
    }

private:
    static constexpr double kRef      = 29761.0;
    static constexpr float  kModDepth = 8.f;

    static int len(double scale, int ref) noexcept
    {
        return std::max(1, static_cast<int>(std::round(ref * scale)));
    }

    // ── Schroeder all-pass ────────────────────────────────────────────────
    // H(z) = (-g + z^{-M}) / (1 - g·z^{-M}),  |H| = 1 for all ω.
    // State variable w[n] = x[n] + g·w[n-M];  y[n] = -g·w[n] + w[n-M].
    static float apf(float x, std::vector<float>& buf, int& pos,
                     int M, float g) noexcept
    {
        const int   sz  = static_cast<int>(buf.size());
        const int   ri  = (pos - M + sz) % sz;
        const float wM  = buf[static_cast<size_t>(ri)];
        const float wn  = x + g * wM;
        buf[static_cast<size_t>(pos)] = wn;
        pos = (pos + 1) % sz;
        return -g * wn + wM;
    }

    // ── Modulated all-pass (LFO shifts read position by ±mod samples) ────
    static float apfMod(float x, std::vector<float>& buf, int& pos,
                        int M, float g, float mod) noexcept
    {
        const int sz  = static_cast<int>(buf.size());
        const int ri  = (pos - M + static_cast<int>(mod) + sz * 2) % sz;
        const float wM  = buf[static_cast<size_t>(ri)];
        const float wn  = x + g * wM;
        buf[static_cast<size_t>(pos)] = wn;
        pos = (pos + 1) % sz;
        return -g * wn + wM;
    }

    // ── Delay write/read ──────────────────────────────────────────────────
    static void write(std::vector<float>& buf, int& pos, float v) noexcept
    {
        buf[static_cast<size_t>(pos)] = v;
        pos = (pos + 1) % static_cast<int>(buf.size());
    }

    // Read the sample written `delay` steps ago (1 = just written).
    static float read(const std::vector<float>& buf, int pos, int delay) noexcept
    {
        // pos has been incremented past the last write, so pos-1 = newest.
        const int sz  = static_cast<int>(buf.size());
        const int ri  = (pos - 1 - delay + sz * 2) % sz;
        return buf[static_cast<size_t>(ri)];
    }

    double sampleRate = 44100.0;
    int preDelayLen{}, apf1Len{}, apf2Len{}, apf3Len{}, apf4Len{};
    int tapfL1Len{}, tdL1Len{}, tapfL2Len{}, tdL2Len{};
    int tapfR1Len{}, tdR1Len{}, tapfR2Len{}, tdR2Len{};

    std::vector<float> preDelay, apf1, apf2, apf3, apf4;
    std::vector<float> tapfL1, tdL1, tapfL2, tdL2;
    std::vector<float> tapfR1, tdR1, tapfR2, tdR2;

    int pdPos{}, a1p{}, a2p{}, a3p{}, a4p{};
    int tl1p{}, dl1p{}, tl2p{}, dl2p{};
    int tr1p{}, dr1p{}, tr2p{}, dr2p{};

    float lpBW{}, lpTL{}, lpTR{};
    float feedL{}, feedR{};
    float lfoPhase{};
};
