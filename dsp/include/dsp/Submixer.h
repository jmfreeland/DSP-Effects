#pragma once

namespace dsp
{
/**
 * The PCM81 Pitch-class algorithms' own "Submixer": routes the two main
 * stereo inputs into a Rvb block and an FX block's own stereo inputs
 * (Sends), and routes their two stereo outputs back to the main stereo
 * outputs (Returns) - the arithmetic half of the manual's own Submixer
 * row, "Sends", "Returns", and "Routing". Each of Sends/Returns is a
 * discrete-valued control on the real hardware (0/100/150/200/300 with
 * labeled positions, not a continuous crossfade) so this Component
 * models them as enums rather than floats.
 *
 * Deliberately does *not* handle Routing (parallel vs. series either
 * direction): that decides *which* block's Sends-computed input is
 * actually used and which block's own output feeds Returns, a
 * sequencing decision each owning Graph makes itself since it alone owns
 * both Block instances and must call them in the right order (e.g. in
 * series mode, the second block's input comes from the first block's
 * output, not from send()). See docs/lexicon-pcm81-reference.md's "The
 * Pitch algorithms" section for the full Sends/Returns/Routing tables
 * this reproduces, and the manual's own "Useful Configurations" diagrams
 * - every one of those is just a Sends/Returns combination already
 * covered by the two 4-way enums below (e.g. "Dual Mono In/Stereo Out"
 * is Sends::kLRvbRFx + Returns::kStereo; "Dual Mono In/Mono Out" is
 * Sends::kLRvbRFx + Returns::kRvbLFxR), so nothing further is needed
 * here to reproduce them.
 */
class Submixer
{
  public:
    enum class Sends
    {
        kStereo,  // Stereo(0)/Stereo(300, a duplicate labeled position): L->Rvb L, R->Rvb R / L->FX L, R->FX R
        kLRvbRFx, // L=Rvb,R=FX(100): L->Rvb L&R / R->FX L&R
        kMono,    // Mono(150): L+R->Rvb L&R / L+R->FX L&R
        kLFxRRvb, // L=FX,R=Rvb(200): R->Rvb L&R / L->FX L&R
    };

    enum class Returns
    {
        kStereo,  // Stereo(0)/Stereo(300): Rvb L->outL, Rvb R->outR / FX L->outL, FX R->outR (summed)
        kRvbLFxR, // Rvb=L,FX=R(100): Rvb L&R->outL / FX L&R->outR
        kMono,    // Mono(150): (Rvb L+R)->outL&outR, (FX L+R)->outL&outR (summed)
        kFxLRvbR, // FX=L,Rvb=R(200): FX L&R->outL / Rvb L&R->outR
    };

    void setSends(Sends sends) { sends_ = sends; }
    void setReturns(Returns returns) { returns_ = returns; }

    struct SendResult
    {
        float rvbLeft;
        float rvbRight;
        float fxLeft;
        float fxRight;
    };

    SendResult send(float left, float right) const
    {
        switch (sends_)
        {
            case Sends::kLRvbRFx:
                return { left, left, right, right };
            case Sends::kMono:
            {
                auto mono = left + right;
                return { mono, mono, mono, mono };
            }
            case Sends::kLFxRRvb:
                return { right, right, left, left };
            case Sends::kStereo:
            default:
                return { left, right, left, right };
        }
    }

    struct ReturnResult
    {
        float left;
        float right;
    };

    ReturnResult receive(float rvbLeft, float rvbRight, float fxLeft, float fxRight) const
    {
        switch (returns_)
        {
            case Returns::kRvbLFxR:
                return { rvbLeft + rvbRight, fxLeft + fxRight };
            case Returns::kMono:
            {
                auto rvbMono = rvbLeft + rvbRight;
                auto fxMono = fxLeft + fxRight;
                return { rvbMono + fxMono, rvbMono + fxMono };
            }
            case Returns::kFxLRvbR:
                return { fxLeft + fxRight, rvbLeft + rvbRight };
            case Returns::kStereo:
            default:
                return { rvbLeft + fxLeft, rvbRight + fxRight };
        }
    }

  private:
    Sends sends_ = Sends::kStereo;
    Returns returns_ = Returns::kStereo;
};
}
