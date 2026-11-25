#include "ReverseBuffer.h"

#include <algorithm>
#include <cmath>

namespace ReverseModeC
{

namespace
{
constexpr float pi = 3.14159265358979323846f;
}

void ReverseBuffer::prepare(double sampleRate)
{
    capacitySamples = std::max(1, (int) std::ceil(maxLengthSeconds * sampleRate));
    left.assign((size_t) capacitySamples, 0.0f);
    right.assign((size_t) capacitySamples, 0.0f);

    samplesWritten = 0;
    writeIndex = 0;
    phase = 0;
    triggerPosA = 0;
    triggerPosB = 0;
}

void ReverseBuffer::setGrainLengthSeconds(double seconds)
{
    const auto requested = (int) std::lround(seconds * (capacitySamples > 0 ? (double) capacitySamples / maxLengthSeconds : 0.0));
    grainLengthSamples = std::clamp(requested, 2, std::max(2, capacitySamples));
    hopSamples = grainLengthSamples / 2;
    phase %= grainLengthSamples;
}

void ReverseBuffer::readGrain(long long triggerPos, int age, float& outLeft, float& outRight) const
{
    auto rawIndex = triggerPos - 1 - (long long) age;
    auto idx = (int) (((rawIndex % capacitySamples) + capacitySamples) % capacitySamples);
    outLeft = left[(size_t) idx];
    outRight = right[(size_t) idx];
}

void ReverseBuffer::processSample(float inLeft, float inRight, float& outLeft, float& outRight)
{
    left[(size_t) writeIndex] = inLeft;
    right[(size_t) writeIndex] = inRight;
    writeIndex = (writeIndex + 1) % capacitySamples;
    ++samplesWritten;

    if (phase == 0)
        triggerPosA = samplesWritten;
    else if (phase == hopSamples)
        triggerPosB = samplesWritten;

    // Periodic raised-cosine window: w(a) + w(a + grainLength/2 mod
    // grainLength) == 1 exactly for every age, which is what makes the two
    // overlapping grains sum to constant unity gain (no amplitude dip or
    // click at the splice), without relying on floating-point agreement
    // between two separately-evaluated cosines.
    const auto ageA = phase;
    const auto ageB = (phase + hopSamples) % grainLengthSamples;
    const auto windowA = 0.5f - 0.5f * std::cos(2.0f * pi * (float) ageA / (float) grainLengthSamples);
    const auto windowB = 1.0f - windowA;

    float grainAL, grainAR, grainBL, grainBR;
    readGrain(triggerPosA, ageA, grainAL, grainAR);
    readGrain(triggerPosB, ageB, grainBL, grainBR);

    outLeft = windowA * grainAL + windowB * grainBL;
    outRight = windowA * grainAR + windowB * grainBR;

    phase = (phase + 1) % grainLengthSamples;
}

} // namespace ReverseModeC
