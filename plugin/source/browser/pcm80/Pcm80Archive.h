#pragma once

#include "Pcm80Preset.h"

#include <juce_core/juce_core.h>

#include <vector>

// Loads a PCM80 preset archive JSON (tools/pcm80-import/extract_presets.py's
// output) at runtime - never bundled with the plugin, never committed to
// this repo. The archive is Lexicon's copyrighted preset data extracted
// from hardware the user owns; this class only reads a file the user
// points it at, exactly the same posture as extract_presets.py itself.
namespace loom::browser::pcm80
{
class Archive
{
  public:
    // Returns false (and leaves the archive empty) if the file can't be
    // read or parsed as JSON - errorMessage, if given, is set to why.
    bool loadFromFile(const juce::File& file, juce::String* errorMessage = nullptr);

    const std::vector<Preset>& presets() const { return presets_; }

    // Presets whose algorithm matches (case-sensitive, e.g. "Plate") -
    // reliable ones first, since those are the ones worth surfacing by
    // default (see decoder.py's "reliable" flag / the known-anomaly
    // note in its module docstring).
    std::vector<const Preset*> presetsForAlgorithm(const juce::String& algorithm) const;

  private:
    std::vector<Preset> presets_;
};
}
