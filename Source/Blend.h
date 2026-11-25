#pragma once

namespace ReverseModeC
{

// Rough/linear 2-way crossfade between the Forward (dry pass-through) and
// Reverse (granular reversed) voices. balance in [-1, 1]: -1 is Forward
// only, +1 is Reverse only, 0 is an equal blend. The exact (nonlinear)
// curve shape is tuned in a later ticket; this is intentionally simple.
inline float blendBalance(float forward, float reverse, float balance)
{
    const auto t = (balance + 1.0f) * 0.5f;
    return forward * (1.0f - t) + reverse * t;
}

// Dry/wet blend between the raw input and the Balance-blended Forward/Reverse
// signal. mix in [0, 1]: 0 is fully dry, 1 is fully wet.
inline float blendMix(float dry, float wet, float mix)
{
    return dry * (1.0f - mix) + wet * mix;
}

} // namespace ReverseModeC
