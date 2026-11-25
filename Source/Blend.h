#pragma once

namespace ReverseModeC
{

// Rough/linear 3-way crossfade between the Forward (dry pass-through),
// Reverse (granular reversed), and Octave-up Reverse (granular reversed at
// 2x rate) voices. balance in [-1, 1]: -1 is Forward only, 0 is Reverse
// only, +1 is Octave-up Reverse only -- two chained linear crossfades
// (Forward->Reverse over [-1, 0], then Reverse->Octave-up over [0, 1]), so
// Reverse has some presence everywhere except the two pure endpoints. The
// exact (nonlinear) curve shape is tuned in a later ticket; this is
// intentionally simple.
inline float blendBalance(float forward, float reverse, float octaveUp, float balance)
{
    if (balance <= 0.0f)
    {
        const auto t = balance + 1.0f;
        return forward * (1.0f - t) + reverse * t;
    }

    const auto t = balance;
    return reverse * (1.0f - t) + octaveUp * t;
}

// Dry/wet blend between the raw input and the Balance-blended signal.
// mix in [0, 1]: 0 is fully dry, 1 is fully wet.
inline float blendMix(float dry, float wet, float mix)
{
    return dry * (1.0f - mix) + wet * mix;
}

} // namespace ReverseModeC
