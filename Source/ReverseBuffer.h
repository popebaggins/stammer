#pragma once

#include <vector>

namespace ReverseModeC
{

// A fixed-capacity stereo circular record buffer paired with a dual
// overlapping-window granular reverse reader. Capacity is allocated once in
// prepare() (4 seconds max) and never reallocated afterwards; changing the
// active grain length only adjusts in-place bookkeeping.
//
// Not thread-safe by design: it assumes a single calling thread (the audio
// thread), matching the processor's single-writer requirement.
class ReverseBuffer
{
public:
    static constexpr double maxLengthSeconds = 4.0;

    // Allocates the circular record buffer for maxLengthSeconds at the given
    // sample rate. Call once, off the audio thread, before processing.
    void prepare(double sampleRate);

    // Sets the active grain/window length in seconds, clamped to the
    // capacity allocated by prepare(). Never allocates.
    void setGrainLengthSeconds(double seconds);

    int getGrainLengthSamples() const { return grainLengthSamples; }

    // Writes one input sample into the circular buffer and produces the
    // corresponding time-reversed granular output sample. Call exactly once
    // per sample, in order; audio-thread safe (no allocation, no locks).
    //
    // When octaveUpLeft/octaveUpRight are non-null, also produces the +1
    // octave voice: the same grain/window timing (ageA/ageB and window
    // weights), but stepping through recorded history at 2x the rate, which
    // pitches the readback up one octave.
    void processSample(float inLeft, float inRight, float& outLeft, float& outRight,
                        float* octaveUpLeft = nullptr, float* octaveUpRight = nullptr);

private:
    std::vector<float> left, right;
    int capacitySamples = 0;
    int grainLengthSamples = 0;
    int hopSamples = 0;

    long long samplesWritten = 0;
    int writeIndex = 0;
    int phase = 0;
    long long triggerPosA = 0;
    long long triggerPosB = 0;

    void readGrain(long long triggerPos, int age, int rate, float& outLeft, float& outRight) const;
};

} // namespace ReverseModeC
