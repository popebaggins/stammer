#pragma once

#include <cmath>

namespace ReverseModeC
{

namespace detail
{
    constexpr float halfPi = 1.5707963267948966f;

    // Equal-power crossfade weights: gainIn^2 + gainOut^2 == 1 throughout,
    // so a segment never dips below unity perceived loudness the way a
    // linear amplitude fade does at its midpoint (0.5 + 0.5 == 1 in
    // amplitude, but only 0.71 in power). u is the segment's own 0..1
    // position: 0 at the voice being faded from, 1 at the voice being
    // faded to.
    inline float equalPowerIn(float u)
    {
        return std::sin(u * halfPi);
    }

    inline float equalPowerOut(float u)
    {
        return std::cos(u * halfPi);
    }
}

// Final, by-ear-tuned nonlinear 3-way crossfade between the Forward (dry
// pass-through), Reverse (granular reversed), and Octave-up Reverse
// (granular reversed at 2x rate) voices, compared against the field
// guide's Balance volume-vs-knob-position graph
// (docs/research/reverse-mode-c-vst-research.md, p.20-21). balance in
// [-1, 1]: -1 is Forward only, 0 is Reverse only, +1 is Octave-up Reverse
// only.
//
// This is the exact, final mapping -- documented here so ticket 07's
// WebView UI can reuse it unchanged instead of re-deriving it:
//
//   left  = clamp(-balance, 0, 1)                -- 0 at centre, 1 at full Forward
//   right = clamp(balance,  0, 1) ^ octaveUpSkew  -- 0 at centre, 1 at full Octave-up
//
//   balance <= 0:  out = Forward * sin(left  * pi/2) + Reverse   * cos(left  * pi/2)
//   balance >= 0:  out = Reverse * cos(right * pi/2) + Octave-up * sin(right * pi/2)
//
// Two by-ear choices over a naive linear, or plain symmetric equal-power,
// curve (the TDD baseline this started from):
//
//  1. Equal-power (sin/cos) on BOTH halves rather than linear amplitude.
//     A linear crossfade sags to ~0.71x power right at each segment's
//     midpoint, which read as an audible dip in the centre Reverse voice
//     while sweeping Balance through it -- not what the reference graph
//     shows. Equal-power removes that dip and gives each voice a steep,
//     immediately-audible onset right off its pure endpoint, matching the
//     field guide's description of the left half: "slight turn" away from
//     an extreme already yields a "forward+reverse blend", not a
//     near-silent trickle.
//
//  2. The right half's parameter is warped by octaveUpSkew (> 1) before
//     the equal-power mapping; the left half is left un-warped
//     (equivalent to an exponent of 1). This holds the Octave-up voice
//     further back through the early part of the right-hand sweep,
//     letting it arrive more steeply near +1 -- matching the field
//     guide's framing of the right half as a continuous, graduated
//     "reweight" of all three voices, a distinct character from the left
//     half's quicker solo/blend. By ear: below ~1.3 the Octave-up voice
//     leaked in too early, muddying the Reverse voice before Balance had
//     travelled far past centre; above ~2.0 it felt withheld for too much
//     of the knob's travel and then arrived abruptly right near +1. 1.6
//     is the middle of that range -- Octave-up stays subordinate through
//     roughly the first third of the right half, then rises the rest of
//     the way.
inline constexpr float octaveUpSkew = 1.6f;

inline float blendBalance(float forward, float reverse, float octaveUp, float balance)
{
    if (balance <= 0.0f)
    {
        const auto left = -balance;
        return forward * detail::equalPowerIn(left) + reverse * detail::equalPowerOut(left);
    }

    const auto right = std::pow(balance, octaveUpSkew);
    return reverse * detail::equalPowerOut(right) + octaveUp * detail::equalPowerIn(right);
}

// Dry/wet blend between the raw input and the Balance-blended signal.
// mix in [0, 1]: 0 is fully dry, 1 is fully wet.
inline float blendMix(float dry, float wet, float mix)
{
    return dry * (1.0f - mix) + wet * mix;
}

} // namespace ReverseModeC
