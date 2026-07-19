#include "Pcm80Archive.h"

namespace loom::browser::pcm80
{
namespace
{
Field parseField(const juce::var& v)
{
    Field f;
    f.group = v.getProperty("group", "").toString();
    f.label = v.getProperty("label", "").toString();
    f.raw = static_cast<double>(v.getProperty("raw", 0.0));
    f.tempoActive = static_cast<bool>(v.getProperty("tempo_active", false));
    f.rangeDecode = static_cast<int>(v.getProperty("range_decode", -1));
    auto numeric = v.getProperty("numeric", juce::var());
    if (!numeric.isVoid() && !numeric.isUndefined())
    {
        f.numeric = static_cast<double>(numeric);
    }
    f.unit = v.getProperty("unit", "").toString();
    return f;
}
}

bool Archive::loadFromFile(const juce::File& file, juce::String* errorMessage)
{
    presets_.clear();

    if (!file.existsAsFile())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "File not found: " + file.getFullPathName();
        }
        return false;
    }

    auto text = file.loadFileAsString();
    auto parsed = juce::JSON::parse(text);
    if (!parsed.isObject())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "Not a valid PCM80 archive (expected a JSON object)";
        }
        return false;
    }

    auto presetsVar = parsed.getProperty("presets", juce::var());
    if (auto* presetArray = presetsVar.getArray())
    {
        for (auto& presetVar : *presetArray)
        {
            Preset preset;
            preset.name = presetVar.getProperty("name", "").toString();
            preset.knobLabel = presetVar.getProperty("macro_knob_label", "").toString();
            preset.algorithmId = static_cast<int>(presetVar.getProperty("algorithm_id", -1));

            auto decoded = presetVar.getProperty("decoded", juce::var());
            if (!decoded.isObject())
            {
                continue; // no decode available for this preset - skip
            }
            preset.algorithm = decoded.getProperty("algorithm", "").toString();
            preset.reliable = static_cast<bool>(decoded.getProperty("reliable", false));

            auto patchableVar = decoded.getProperty("patchable", juce::var());
            if (auto* patchableArray = patchableVar.getArray())
            {
                preset.fields.reserve(static_cast<size_t>(patchableArray->size()));
                for (auto& fieldVar : *patchableArray)
                {
                    preset.fields.push_back(parseField(fieldVar));
                }
            }

            presets_.push_back(std::move(preset));
        }
    }

    if (presets_.empty() && errorMessage != nullptr)
    {
        *errorMessage = "No decoded presets found in this archive";
    }

    return !presets_.empty();
}

std::vector<const Preset*> Archive::presetsForAlgorithm(const juce::String& algorithm) const
{
    std::vector<const Preset*> reliableMatches;
    std::vector<const Preset*> unreliableMatches;
    for (auto& preset : presets_)
    {
        if (preset.algorithm != algorithm)
        {
            continue;
        }
        (preset.reliable ? reliableMatches : unreliableMatches).push_back(&preset);
    }
    reliableMatches.insert(reliableMatches.end(), unreliableMatches.begin(), unreliableMatches.end());
    return reliableMatches;
}
}
