#pragma once

#include "dsp/graphs/StereoChmbAlgorithm.h"

#include <cmath>

namespace dsp::graphs
{
/**
 * The Lexicon PCM81 "VSO-Chmb" algorithm: identical to Stereo-Chmb (see
 * docs/lexicon-pcm81-stereo-chmb.md) - "Like the Stereo-Chmb algorithm,
 * VSO-Chmb is combined with a stereo chamber reverb" - plus one added
 * `Varispeed` parameter that directly computes the compensating pitch
 * shift for a known playback-speed change, so it's implemented here by
 * inheriting `StereoChmbAlgorithm` wholesale (matching the Block tier's
 * own `Infinite : public Chamber` shape - "Chamber + different
 * defaults/one addition") and adding just that one new setter, rather
 * than duplicating the whole Submixer/Routing/Stereo-Shifter machinery
 * for a single closed-form calculation on top.
 *
 * Per the manual: "This algorithm is a utility program designed to
 * provide pitch correction of varispeed material... Simply match the
 * value of the Varispeed parameter to the varispeed setting of the
 * playback source." Its own worked examples pin the formula down
 * exactly: compressing a 30-second spot to 24 seconds needs the
 * playback source sped up 20% (per the manual's own convention -
 * newDuration = originalDuration * (1 - varispeed/100), i.e. speed
 * multiplier = 1/(1 - varispeed/100) - NOT "speed * 1.20"; that
 * multiplier is actually 1/(1-0.20) = 1.25), which the manual states
 * creates "an upward pitch shift of 386 cents" (1200*log2(1.25) = 386.3,
 * confirming the convention above rather than the more literal-sounding
 * "increased by 20%" reading). VSO-Chmb's Varispeed then applies the
 * exact inverse shift to restore original pitch:
 *
 *   speedMultiplier = 1 / (1 - varispeed/100)
 *   shiftCents = -1200 * log2(speedMultiplier) = 1200 * log2(1 - varispeed/100)
 *
 * The manual's second worked example (expanding 28s to 30s needs speed
 * decreased 7.14%) checks out the same way: 1/(1-(-7.14/100)) = 28/30.
 */
class VSOChmbAlgorithm : public StereoChmbAlgorithm
{
  public:
    // +55.00% .. -35.00% per the manual's own Varispeed range, 0.01% steps.
    void setVarispeed(float percent)
    {
        auto speedMultiplier = 1.0f - percent / 100.0f;
        auto cents = 1200.0f * std::log2(speedMultiplier);
        setShiftCents(cents);
    }
};
}
