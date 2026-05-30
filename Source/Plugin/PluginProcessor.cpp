#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Bus layout ─────────────────────────────────────────────────────────────

juce::AudioProcessor::BusesProperties UmbraProcessor::getDefaultBusesProperties()
{
    return BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

UmbraProcessor::UmbraProcessor()
    : AudioProcessor(getDefaultBusesProperties())
{
}

// ── Audio lifecycle ────────────────────────────────────────────────────────

void UmbraProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    ampProcessor.prepare(sampleRate, samplesPerBlock);
    workBuffer.setSize(2, samplesPerBlock, false, true, false);
    ampProcessor.setParams(currentParams);
}

void UmbraProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear any output channels that don't have input
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    // Mix all input channels down to mono in workBuffer[0]
    workBuffer.setSize(2, numSamples, false, false, true);
    workBuffer.clear();

    const float scale = numIn > 0 ? 1.0f / static_cast<float>(numIn) : 1.0f;
    for (int ch = 0; ch < numIn && ch < 2; ++ch)
        workBuffer.addFrom(0, 0, buffer, ch, 0, numSamples, scale);

    ampProcessor.process(workBuffer);

    // Copy to output
    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom(ch, 0, workBuffer, std::min(ch, 1), 0, numSamples);
}

// ── State persistence (XML) ───────────────────────────────────────────────

void UmbraProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto& p = currentParams;
    juce::DynamicObject::Ptr obj(new juce::DynamicObject());
    auto ch2str = [](AmpProcessor::Channel c) -> juce::String {
        return c == AmpProcessor::Channel::Clean  ? "Clean"
             : c == AmpProcessor::Channel::Crunch ? "Crunch" : "Lead";
    };
    auto cab2str = [](AmpProcessor::CabType t) -> juce::String {
        switch (t) {
            case AmpProcessor::CabType::G12T75:   return "G12T75";
            case AmpProcessor::CabType::Greenback: return "Greenback";
            case AmpProcessor::CabType::Open:      return "Open";
            default:                               return "V30";
        }
    };
    obj->setProperty("channel",    ch2str(p.channel));
    obj->setProperty("cab",        cab2str(p.cabType));
    obj->setProperty("boost",      p.boost);
    obj->setProperty("comp",       p.comp);
    obj->setProperty("gate",       p.gate);
    obj->setProperty("gain",       p.gain);
    obj->setProperty("bass",       p.bass);
    obj->setProperty("mid",        p.mid);
    obj->setProperty("treble",     p.treble);
    obj->setProperty("presence",   p.presence);
    obj->setProperty("volume",     p.volume);
    obj->setProperty("sag",        p.sag);
    obj->setProperty("trem",       p.trem);
    obj->setProperty("reverb",     p.reverb);
    obj->setProperty("chorus",     p.chorus);
    obj->setProperty("delay",      p.delay);
    obj->setProperty("delayMs",    p.delayTimeMs);
    obj->setProperty("width",      p.width);
    obj->setProperty("phaser",     p.phaser);
    obj->setProperty("flanger",    p.flanger);
    obj->setProperty("transpose",  p.transpose);

    const auto xml = juce::JSON::toString(juce::var(obj.get()), true);
    copyXmlToBinary(*juce::XmlDocument::parse("<s>" + xml + "</s>"), destData);
}

void UmbraProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto parsed = juce::JSON::parse(xml->getAllSubText());
        if (const auto* obj = parsed.getDynamicObject())
        {
            auto getF = [&](const char* k, float def) {
                auto v = obj->getProperty(k);
                return v.isUndefined() ? def : static_cast<float>(static_cast<double>(v));
            };

            auto& p = currentParams;
            const juce::String ch = obj->getProperty("channel").toString();
            p.channel = ch == "Clean"  ? AmpProcessor::Channel::Clean
                      : ch == "Crunch" ? AmpProcessor::Channel::Crunch
                                       : AmpProcessor::Channel::Lead;
            const juce::String cab = obj->getProperty("cab").toString();
            p.cabType = cab == "G12T75"    ? AmpProcessor::CabType::G12T75
                      : cab == "Greenback" ? AmpProcessor::CabType::Greenback
                      : cab == "Open"      ? AmpProcessor::CabType::Open
                                           : AmpProcessor::CabType::V30;
            p.boost     = static_cast<bool>(obj->getProperty("boost"));
            p.comp      = getF("comp",    0.f);
            p.gate      = getF("gate",    0.f);
            p.gain      = getF("gain",    0.5f);
            p.bass      = getF("bass",    0.5f);
            p.mid       = getF("mid",     0.5f);
            p.treble    = getF("treble",  0.5f);
            p.presence  = getF("presence",0.5f);
            p.volume    = getF("volume",  0.5f);
            p.sag       = getF("sag",     0.f);
            p.trem      = getF("trem",    0.f);
            p.reverb    = getF("reverb",  0.f);
            p.chorus    = getF("chorus",  0.f);
            p.delay     = getF("delay",   0.f);
            p.delayTimeMs = getF("delayMs", 350.f);
            p.width     = getF("width",   0.5f);
            p.phaser    = getF("phaser",  0.f);
            p.flanger   = getF("flanger", 0.f);
            p.transpose = static_cast<int>(obj->getProperty("transpose"));

            ampProcessor.setParams(p);
        }
    }
}

void UmbraProcessor::pushParams(const AmpProcessor::Params& p)
{
    currentParams = p;
    ampProcessor.setParams(p);
}

// ── Editor factory ────────────────────────────────────────────────────────

juce::AudioProcessorEditor* UmbraProcessor::createEditor()
{
    return new UmbraPluginEditor(*this);
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UmbraProcessor();
}
