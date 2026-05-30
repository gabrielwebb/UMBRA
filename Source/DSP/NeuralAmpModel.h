#pragma once
// ── NeuralAmpModel ─────────────────────────────────────────────────────────
// Wraps RTNeural compile-time LSTM models.
//
// Supported hidden sizes (covers all GuitarML / NeuralPi / Proteus captures):
//   8, 16, 20, 24, 32, 40
//
// JSON formats supported:
//   • GuitarML / NeuralPi  — { "model_data": {..., "hidden_size": N}, "state_dict": {...} }
//   • RTNeural native       — { "layers": [ {"type":"lstm", "hidden_size":N}, ...] }
//
// Thread safety: loadFromFile() — message/worker thread only.
//               process()      — audio thread only.
//               isLoaded()     — any thread.

#ifdef UMBRA_HAS_RTNEURAL
#include <RTNeural/RTNeural.h>
#endif

#include <JuceHeader.h>
#include <atomic>

class NeuralAmpModel
{
public:
    NeuralAmpModel()  = default;
    ~NeuralAmpModel() = default;

    // ── Load / clear ──────────────────────────────────────────────────────

    bool loadFromFile(const juce::File& file)
    {
#ifdef UMBRA_HAS_RTNEURAL
        const juce::String content = file.loadFileAsString();
        if (content.isEmpty()) return false;

        try
        {
            auto fullJson = nlohmann::json::parse(content.toStdString());

            // ── Detect hidden_size ─────────────────────────────────────────
            // GuitarML format stores architecture in "model_data"
            int hidden = 0;

            if (fullJson.contains("model_data"))
            {
                auto& md = fullJson.at("model_data");
                hidden = md.value("hidden_size", 0);
                if (hidden == 0)   // fallback: count weights directly
                    hidden = detectHiddenFromStateDict(fullJson);
            }
            else if (fullJson.contains("layers"))
            {
                // RTNeural native format
                for (auto& layer : fullJson.at("layers"))
                {
                    auto tp = layer.value("type", std::string{});
                    if (tp == "lstm" || tp == "LSTM" || tp == "gru" || tp == "GRU")
                    { hidden = layer.value("hidden_size", 0); break; }
                }
            }
            else
            {
                // Try inferring from weight matrix dimensions
                hidden = detectHiddenFromStateDict(fullJson);
            }

            if (hidden == 0)
            {
                DBG("NeuralAmpModel: could not detect hidden_size");
                return false;
            }

            // ── Load weights into matching compile-time model ──────────────
            bool ok = false;
            {
                juce::ScopedLock sl(lock);
                // Pass the FULL json object — RTNeural reads "state_dict" from it
                switch (hidden)
                {
                    case  8:  ok = tryLoad(model8,  fullJson); activeSize =  8;  break;
                    case 16:  ok = tryLoad(model16, fullJson); activeSize = 16;  break;
                    case 20:  ok = tryLoad(model20, fullJson); activeSize = 20;  break;
                    case 24:  ok = tryLoad(model24, fullJson); activeSize = 24;  break;
                    case 32:  ok = tryLoad(model32, fullJson); activeSize = 32;  break;
                    case 40:  ok = tryLoad(model40, fullJson); activeSize = 40;  break;
                    default:
                        DBG("NeuralAmpModel: unsupported hidden_size " << hidden);
                        return false;
                }
                if (ok) name = file.getFileNameWithoutExtension();
            }
            loaded.store(ok);
            if (!ok) DBG("NeuralAmpModel: parseJson failed for " << file.getFileName());
            return ok;
        }
        catch (const std::exception& e) { DBG("NeuralAmpModel load error: " << e.what()); }
        catch (...)                      { DBG("NeuralAmpModel: unknown load error"); }
#else
        juce::ignoreUnused(file);
        DBG("NeuralAmpModel: RTNeural not compiled in (rebuild after cmake reconfigure)");
#endif
        return false;
    }

    void clear()
    {
        juce::ScopedLock sl(lock);
        loaded.store(false);
        name = {};
        activeSize = 0;
    }

    bool         isLoaded() const noexcept { return loaded.load(); }
    juce::String getName()  const noexcept { juce::ScopedLock sl(lock); return name; }

    void reset() noexcept
    {
#ifdef UMBRA_HAS_RTNEURAL
        juce::ScopedLock sl(lock);
        model8 .reset(); model16.reset(); model20.reset();
        model24.reset(); model32.reset(); model40.reset();
#endif
    }

    // ── Process (audio thread) ────────────────────────────────────────────
    // Returns neural model output, or x unmodified when no model is loaded.
    // Input should be in roughly [-1, 1] range (conditioned pre-gain).
    float process(float x) noexcept
    {
#ifdef UMBRA_HAS_RTNEURAL
        if (!loaded.load(std::memory_order_relaxed)) return x;
        float in[1] = { x };
        // No lock: swap happens off audio thread; worst case = 1 block of stale model
        switch (activeSize)
        {
            case  8: model8 .forward(in); return model8 .getOutputs()[0];
            case 16: model16.forward(in); return model16.getOutputs()[0];
            case 20: model20.forward(in); return model20.getOutputs()[0];
            case 24: model24.forward(in); return model24.getOutputs()[0];
            case 32: model32.forward(in); return model32.getOutputs()[0];
            case 40: model40.forward(in); return model40.getOutputs()[0];
            default: break;
        }
#endif
        return x;
    }

private:
#ifdef UMBRA_HAS_RTNEURAL
    // Compile-time LSTM + Dense models — fully inlined, no virtual dispatch.
    // Adding new sizes: declare here, add case in loadFromFile + process.
    RTNeural::ModelT<float,1,1, RTNeural::LSTMLayerT<float,1, 8>, RTNeural::DenseT<float, 8,1>> model8;
    RTNeural::ModelT<float,1,1, RTNeural::LSTMLayerT<float,1,16>, RTNeural::DenseT<float,16,1>> model16;
    RTNeural::ModelT<float,1,1, RTNeural::LSTMLayerT<float,1,20>, RTNeural::DenseT<float,20,1>> model20;
    RTNeural::ModelT<float,1,1, RTNeural::LSTMLayerT<float,1,24>, RTNeural::DenseT<float,24,1>> model24;
    RTNeural::ModelT<float,1,1, RTNeural::LSTMLayerT<float,1,32>, RTNeural::DenseT<float,32,1>> model32;
    RTNeural::ModelT<float,1,1, RTNeural::LSTMLayerT<float,1,40>, RTNeural::DenseT<float,40,1>> model40;

    template<typename M>
    static bool tryLoad(M& model, const nlohmann::json& json)
    {
        try { model.parseJson(json, false); model.reset(); return true; }
        catch (const std::exception& e) { DBG("parseJson: " << e.what()); return false; }
        catch (...) { return false; }
    }

    // Infer hidden size from PyTorch weight matrix shape in "state_dict".
    // rec.weight_ih_l0 has shape [4*hidden, input_size].
    static int detectHiddenFromStateDict(const nlohmann::json& json)
    {
        try {
            if (!json.contains("state_dict")) return 0;
            auto& sd = json.at("state_dict");
            if (sd.contains("rec.weight_ih_l0"))
            {
                int rows = static_cast<int>(sd.at("rec.weight_ih_l0").size());
                if (rows > 0 && rows % 4 == 0) return rows / 4;
            }
        } catch (...) {}
        return 0;
    }
#endif

    std::atomic<bool> loaded { false };
    mutable juce::CriticalSection lock;
    juce::String  name;
    int           activeSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralAmpModel)
};
