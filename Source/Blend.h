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

// Baseline nonlinear 3-way crossfade between the Forward (dry
// pass-through), Reverse (granular reversed), and Octave-up Reverse
// (granular reversed at 2x rate) voices. balance in [-1, 1]: -1 is Forward
// only, 0 is Reverse only, +1 is Octave-up Reverse only.
//
// Equal-power (sin/cos) replaces ticket 03's linear amplitude crossfade on
// both halves: this is the by-ear-tuned curve's starting point, ahead of
// further by-ear refinement against the field guide's reference graph
// (docs/research/reverse-mode-c-vst-research.md, p.20-21).
inline float blendBalance(float forward, float reverse, float octaveUp, float balance)
{
    if (balance <= 0.0f)
    {
        const auto left = -balance;
        return forward * detail::equalPowerIn(left) + reverse * detail::equalPowerOut(left);
    }

    const auto right = balance;
    return reverse * detail::equalPowerOut(right) + octaveUp * detail::equalPowerIn(right);
}

// Dry/wet blend between the raw input and the Balance-blended signal.
// mix in [0, 1]: 0 is fully dry, 1 is fully wet.
inline float blendMix(float dry, float wet, float mix)
{
    return dry * (1.0f - mix) + wet * mix;
}

} // namespace ReverseModeC
