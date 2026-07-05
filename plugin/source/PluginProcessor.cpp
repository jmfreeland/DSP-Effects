#include "PluginProcessor.h"

#include <span>

namespace
{
constexpr auto kDecayId = "decay";
constexpr auto kLowRatioId = "lowRatio";
constexpr auto kCrossoverId = "crossover";
constexpr auto kDampingId = "damping";
constexpr auto kDiffusionId = "diffusion";
constexpr auto kSizeId = "size";
constexpr auto kPreDelayId = "preDelay";
constexpr auto kEarlyReflectionLevelId = "earlyReflectionLevel";
constexpr auto kSpinId = "spin";
constexpr auto kChorusId = "chorus";
constexpr auto kPostDelayLeftId = "postDelayLeft";
constexpr auto kPostDelayRightId = "postDelayRight";
constexpr auto kPostDelayMixId = "postDelayMix";
constexpr auto kFxMixId = "fxMix";
constexpr auto kFxWidthId = "fxWidth";
constexpr auto kHiCutId = "hiCut";
constexpr auto kFxAdjustId = "fxAdjust";
constexpr auto kMixId = "mix";
constexpr auto kFreezeId = "freeze";

// One voice's four parameter IDs, e.g. "voice0Delay".
juce::String voiceParamId(int index, const char* suffix)
{
    return "voice" + juce::String(index) + suffix;
}
}

LexiconHallAudioProcessor::LexiconHallAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float LexiconHallAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
LexiconHallAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kDecayId, 1 },
      "Decay",
      juce::NormalisableRange<float>(0.3f, 8.0f, 0.01f, 0.5f),
      2.5f,
      juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kLowRatioId, 1 },
      "Low Ratio",
      juce::NormalisableRange<float>(0.2f, 2.0f, 0.01f),
      1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kCrossoverId, 1 },
      "Crossover",
      juce::NormalisableRange<float>(100.0f, 2000.0f, 1.0f, 0.4f),
      400.0f,
      juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kDampingId, 1 }, "Damping", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kDiffusionId, 1 }, "Diffusion", juce::NormalisableRange<float>(0.0f, 1.0f),
      0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kSizeId, 1 }, "Size", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kPreDelayId, 1 },
      "Pre Delay",
      juce::NormalisableRange<float>(0.0f, 0.93f, 0.001f),
      0.0f,
      juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kEarlyReflectionLevelId, 1 }, "Early Reflections",
      juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kSpinId, 1 }, "Spin", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kChorusId, 1 }, "Chorus", juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    // Four delay Voices: Delay/Feedback/Level/Pan, matching the PCM81's
    // 4-Voice Reverb Shell.
    static constexpr float kDefaultDelay[4] = { 0.09f, 0.13f, 0.0f, 0.0f };
    static constexpr float kDefaultFeedback[4] = { 0.15f, 0.10f, 0.0f, 0.0f };
    static constexpr float kDefaultLevel[4] = { 0.25f, 0.18f, 0.0f, 0.0f };
    static constexpr float kDefaultPan[4] = { -0.3f, 0.3f, 0.0f, 0.0f };
    for (int i = 0; i < 4; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{ voiceParamId(i, "Delay"), 1 },
          "Voice " + juce::String(i + 1) + " Delay",
          juce::NormalisableRange<float>(0.0f, 1.365f, 0.001f),
          kDefaultDelay[i],
          juce::AudioParameterFloatAttributes().withLabel("s")));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{ voiceParamId(i, "Feedback"), 1 },
          "Voice " + juce::String(i + 1) + " Feedback",
          juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
          kDefaultFeedback[i]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{ voiceParamId(i, "Level"), 1 },
          "Voice " + juce::String(i + 1) + " Level",
          juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
          kDefaultLevel[i]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{ voiceParamId(i, "Pan"), 1 },
          "Voice " + juce::String(i + 1) + " Pan",
          juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
          kDefaultPan[i]));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kPostDelayLeftId, 1 },
      "Post Delay L",
      juce::NormalisableRange<float>(0.0f, 1.365f, 0.001f),
      0.25f,
      juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kPostDelayRightId, 1 },
      "Post Delay R",
      juce::NormalisableRange<float>(0.0f, 1.365f, 0.001f),
      0.25f,
      juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kPostDelayMixId, 1 }, "Post Delay Mix",
      juce::NormalisableRange<float>(0.0f, 1.0f), 0.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kFxMixId, 1 }, "FX Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kFxWidthId, 1 }, "FX Width", juce::NormalisableRange<float>(0.0f, 2.0f),
      1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kHiCutId, 1 },
      "Hi Cut",
      juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.4f),
      18000.0f,
      juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kFxAdjustId, 1 },
      "FX Adjust",
      juce::NormalisableRange<float>(-73.0f, 12.0f, 0.1f),
      0.0f,
      juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ kMixId, 1 }, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));

    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kFreezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

void LexiconHallAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::ConcertHallAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void LexiconHallAudioProcessor::releaseResources() {}

bool LexiconHallAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LexiconHallAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setDecaySeconds(paramValue(kDecayId));
    engine_.setLowRatio(paramValue(kLowRatioId));
    engine_.setCrossoverFrequency(paramValue(kCrossoverId));
    engine_.setDamping(paramValue(kDampingId));
    engine_.setDiffusion(paramValue(kDiffusionId));
    engine_.setSize(paramValue(kSizeId));
    engine_.setPreDelaySeconds(paramValue(kPreDelayId));
    engine_.setEarlyReflectionLevel(paramValue(kEarlyReflectionLevelId));
    engine_.setSpin(paramValue(kSpinId));
    engine_.setChorus(paramValue(kChorusId));

    for (int i = 0; i < 4; ++i)
    {
        engine_.setVoice(i, paramValue(voiceParamId(i, "Delay").toRawUTF8()),
                          paramValue(voiceParamId(i, "Feedback").toRawUTF8()),
                          paramValue(voiceParamId(i, "Level").toRawUTF8()),
                          paramValue(voiceParamId(i, "Pan").toRawUTF8()));
    }

    engine_.setPostDelaySeconds(paramValue(kPostDelayLeftId), paramValue(kPostDelayRightId));
    engine_.setPostDelayMix(paramValue(kPostDelayMixId));
    engine_.setFxMix(paramValue(kFxMixId));
    engine_.setFxWidth(paramValue(kFxWidthId));
    engine_.setHiCut(paramValue(kHiCutId));
    engine_.setFxAdjustDb(paramValue(kFxAdjustId));
    engine_.setMix(paramValue(kMixId));
    engine_.setFrozen(paramValue(kFreezeId) >= 0.5f);

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* LexiconHallAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void LexiconHallAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void LexiconHallAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

// This creates new instances of the plugin.
// NOLINTNEXTLINE(readability-identifier-naming) - JUCE-mandated symbol name.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LexiconHallAudioProcessor();
}
