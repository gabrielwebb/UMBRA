#include "SharedComponents.h"

using namespace UmbraColors;

// ── Genre colours ──────────────────────────────────────────────────────────

juce::Colour genreColour(const juce::String& name)
{
    if (name == "DEFTONES") return juce::Colour(0xff007799);
    if (name == "KNOCKED")  return juce::Colour(0xffcc1100);
    if (name == "BILMURI")  return juce::Colour(0xffdd5500);
    if (name == "HYBRID")   return juce::Colour(0xff1166ee);
    if (name == "HOT MUL")  return juce::Colour(0xff22aa88);
    return kAccent;
}

// ── UmbraLAF ───────────────────────────────────────────────────────────────

void UmbraLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                 float pos, float startAngle, float endAngle,
                                 juce::Slider&)
{
    const auto  bounds = juce::Rectangle<float>(x, y, w, h).reduced(4.0f);
    const auto  c      = bounds.getCentre();
    const float r      = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle  = startAngle + (endAngle - startAngle) * pos;

    g.setColour(juce::Colour(0xaa000000));
    g.fillEllipse(bounds.translated(0.0f, 1.5f));

    {
        juce::ColourGradient cg(juce::Colour(0xff3a3733), c.x - r * 0.5f, c.y - r * 0.5f,
                                juce::Colour(0xff0e0d0c), c.x + r * 0.6f, c.y + r * 0.7f, true);
        g.setGradientFill(cg); g.fillEllipse(bounds);
    }
    g.setColour(juce::Colour(0x28ffffff)); g.drawEllipse(bounds.reduced(0.5f), 1.0f);

    const auto trackB = bounds.reduced(r * 0.22f);
    juce::Path groove;
    groove.addArc(trackB.getX(), trackB.getY(), trackB.getWidth(), trackB.getHeight(),
                  startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff060504));
    g.strokePath(groove, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xff1e1c1a));
    g.strokePath(groove, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path varc;
        varc.addArc(trackB.getX(), trackB.getY(), trackB.getWidth(), trackB.getHeight(),
                    startAngle, angle, true);
        g.setColour(kAccentGlow);
        g.strokePath(varc, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(kAccentHi);
        g.strokePath(varc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto bodyB = bounds.reduced(r * 0.30f);
    {
        juce::ColourGradient cg(juce::Colour(0xff3e3b37), c.x - r * 0.2f, c.y - r * 0.3f,
                                juce::Colour(0xff141210), c.x + r * 0.3f, c.y + r * 0.4f, true);
        g.setGradientFill(cg); g.fillEllipse(bodyB);
    }
    {
        juce::ColourGradient cg(juce::Colour(0x38ffffff), c.x - r * 0.18f, c.y - r * 0.22f,
                                juce::Colour(0x00ffffff), c.x + r * 0.05f, c.y + r * 0.05f, true);
        g.setGradientFill(cg); g.fillEllipse(bodyB);
    }

    const float bodyR  = juce::jmin(bodyB.getWidth(), bodyB.getHeight()) * 0.5f;
    const float dotR   = juce::jmax(2.0f, bodyR * 0.14f);
    const float dotDst = bodyR - dotR * 1.5f;
    const auto  dotPos = c.getPointOnCircumference(dotDst, angle);
    g.setColour(juce::Colour(0x88000000));
    g.fillEllipse(dotPos.x - dotR + 0.6f, dotPos.y - dotR + 0.7f, dotR * 2.0f, dotR * 2.0f);
    g.setColour(pos > 0.01f ? juce::Colour(0xfff0eee8) : juce::Colour(0xff555250));
    g.fillEllipse(dotPos.x - dotR, dotPos.y - dotR, dotR * 2.0f, dotR * 2.0f);
}

void UmbraLAF::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                     const juce::Colour&, bool highlighted, bool down)
{
    auto b = btn.getLocalBounds().toFloat().reduced(0.5f);
    const bool tog  = btn.getToggleState();
    const auto text = btn.getButtonText();
    const auto cid  = btn.getComponentID();
    const bool isPreset  = (cid == "preset");
    const bool isChannel = (cid == "channel");

    juce::Colour bg = tog    ? kPanel.brighter(0.08f)
                    : down   ? juce::Colour(0xff252320)
                    : highlighted ? juce::Colour(0xff1e1c1a)
                                  : juce::Colour(0xff161412);
    g.setColour(bg); g.fillRoundedRectangle(b, 3.0f);

    if (isPreset && tog) g.setColour(genreColour(text).withAlpha(0.70f));
    else g.setColour(tog ? kAccent.withAlpha(0.85f) : juce::Colour(0xff2e2c2a));
    g.drawRoundedRectangle(b, 3.0f, 1.0f);

    if (isPreset)
    {
        const juce::Colour gc = genreColour(text);
        const float barH = 3.0f, barX = b.getX() + 4.0f;
        const float barW = b.getWidth() - 8.0f;
        const float barY = b.getBottom() - barH - 1.5f;
        const auto  barRect = juce::Rectangle<float>(barX, barY, barW, barH);
        if (tog)
        {
            g.setColour(gc.withAlpha(0.30f)); g.fillRoundedRectangle(barRect.expanded(1.5f, 2.5f), 2.5f);
            g.setColour(gc.brighter(0.25f));  g.fillRoundedRectangle(barRect, 1.5f);
            g.setColour(juce::Colours::white.withAlpha(0.35f)); g.fillRoundedRectangle(barX, barY, barW, barH * 0.4f, 1.0f);
        }
        else { g.setColour(gc.withAlpha(0.16f)); g.fillRoundedRectangle(barRect, 1.5f); }
    }

    if (isChannel)
    {
        const juce::Colour ledOn  = (text == "CLEAN")  ? juce::Colour(0xff33bbff)
                                  : (text == "CRUNCH") ? juce::Colour(0xffff8833)
                                                       : juce::Colour(0xffff2200);
        const juce::Colour ledOff = ledOn.withAlpha(0.18f).withBrightness(0.2f);
        const float dotR = 3.0f, dotX = b.getX() + 8.0f, dotY = b.getCentreY();
        if (tog)
        {
            g.setColour(ledOn.withAlpha(0.35f)); g.fillEllipse(dotX-dotR*2, dotY-dotR*2, dotR*4, dotR*4);
            g.setColour(ledOn); g.fillEllipse(dotX-dotR, dotY-dotR, dotR*2, dotR*2);
            g.setColour(juce::Colours::white.withAlpha(0.55f)); g.fillEllipse(dotX-dotR*0.45f, dotY-dotR*0.75f, dotR*0.8f, dotR*0.6f);
        }
        else { g.setColour(ledOff); g.fillEllipse(dotX-dotR, dotY-dotR, dotR*2, dotR*2); }
    }
}

void UmbraLAF::drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool)
{
    const bool tog       = btn.getToggleState();
    const bool isChannel = (btn.getComponentID() == "channel");
    const auto textBounds = isChannel ? btn.getLocalBounds().withTrimmedLeft(8) : btn.getLocalBounds();
    g.setColour(tog ? juce::Colours::white : juce::Colour(0xff888480));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
    g.drawText(btn.getButtonText(), textBounds, juce::Justification::centred, false);
}

void UmbraLAF::drawLabel(juce::Graphics& g, juce::Label& lbl)
{
    g.setColour(juce::Colour(0xff6a6560));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText(lbl.getText(), lbl.getLocalBounds(), juce::Justification::centred, false);
}

// ── LevelMeter ─────────────────────────────────────────────────────────────

LevelMeter::LevelMeter(AmpProcessor& p, Source src) : proc(p), source(src)
{
    startTimerHz(30);
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(kDark); g.fillRoundedRectangle(b, 4.0f);
    g.setColour(juce::Colour(0xff1e1c1a)); g.drawRoundedRectangle(b, 4.0f, 0.5f);

    const juce::String lbl = (source == Source::Input) ? "IN" : "OUT";
    g.setColour(juce::Colour(0xff3a3632));
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
    g.drawText(lbl, getLocalBounds(), juce::Justification::centredTop, false);

    constexpr int kSegs = 13;
    const float padT = 14.f, padB = 3.f, gap = 1.5f;
    const float totH = b.getHeight() - padT - padB;
    const float segH = (totH - gap * (kSegs - 1)) / kSegs;
    const float segW = b.getWidth() - 6.f, segX = b.getX() + 3.f;
    const float level = juce::jlimit(0.f, 1.f, display * 2.f);
    const int   lit   = static_cast<int>(level * kSegs + 0.4f);

    for (int i = 0; i < kSegs; ++i)
    {
        const float segY = b.getBottom() - padB - (i + 1) * segH - i * gap;
        const bool  on   = (i < lit);
        juce::Colour col;
        if      (i >= kSegs - 2) col = on ? juce::Colour(0xffee2200) : juce::Colour(0xff1e0500);
        else if (i >= kSegs - 4) col = on ? juce::Colour(0xffddcc00) : juce::Colour(0xff1a1500);
        else                     col = on ? juce::Colour(0xff11dd44) : juce::Colour(0xff052010);
        g.setColour(col); g.fillRoundedRectangle(segX, segY, segW, segH, 1.f);
        if (on) { g.setColour(juce::Colour(0x18ffffff)); g.fillRoundedRectangle(segX, segY, segW, segH * 0.4f, 1.f); }
    }
}

void LevelMeter::timerCallback()
{
    const float lvl = (source == Source::Input) ? proc.getInputLevel() : proc.getOutputLevel();
    display = display * 0.75f + lvl * 0.25f;
    repaint();
}
