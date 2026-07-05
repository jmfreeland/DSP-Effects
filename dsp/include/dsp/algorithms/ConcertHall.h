#pragma once

#include "dsp/algorithms/ReverbCore.h"

namespace dsp::algorithms
{
/**
 * A Lexicon PCM81-inspired "Concert Hall": clean reverberation designed to
 * stay behind the source, with initial echo density that builds up
 * gradually rather than arriving all at once. Concert Hall uses the
 * shared ReverbCore signal path as-is - no EkoDly/EkoFbk pre-echo (the
 * manual scopes that to Plate/Chamber/Infinite) and no dynamic diffusion
 * shaping - so this class exists mainly to give it its own type identity
 * and default tuning; see ReverbCore for the actual topology and
 * docs/lexicon-pcm81-hall.md for how this compares to the primary source.
 */
class ConcertHall : public ReverbCore
{
};
}
