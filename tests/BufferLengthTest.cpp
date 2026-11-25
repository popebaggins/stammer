#include <catch2/catch_test_macros.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

#include <cstdlib>
#include <new>

namespace
{
constexpr double sampleRate = 44100.0;
constexpr int blockSize = 512;

// Counts heap allocations made while tracking is enabled, so a test can
// bracket exactly the processBlock calls under test -- not the (legitimate,
// off-audio-thread) allocations prepareToPlay makes -- and assert the count
// doesn't move.
namespace AllocationTracking
{
thread_local bool enabled = false;
thread_local std::size_t count = 0;
}

struct ScopedAllocationTracking
{
    ScopedAllocationTracking() { AllocationTracking::count = 0; AllocationTracking::enabled = true; }
    ~ScopedAllocationTracking() { AllocationTracking::enabled = false; }
};

juce::AudioBuffer<float> rampBuffer(int numChannels, int numSamples)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample(ch, i, 0.1f + 0.0005f * (float) i);
    return buffer;
}
}

void* operator new(std::size_t size)
{
    if (AllocationTracking::enabled)
        ++AllocationTracking::count;
    if (auto* p = std::malloc(size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

TEST_CASE("Buffer Length preset sets the active grain length in samples, for each of 1s/2s/4s", "[bufferLength]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    auto setPresetAndProcessOneBlock = [&](int choiceIndex)
    {
        *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::bufferLength) = (float) choiceIndex;
        auto block = rampBuffer(2, blockSize);
        processor.processBlock(block, midi);
    };

    setPresetAndProcessOneBlock(0); // "1s"
    CHECK(processor.getActiveGrainLengthSamples() == 44100);

    setPresetAndProcessOneBlock(2); // "4s"
    CHECK(processor.getActiveGrainLengthSamples() == 176400);

    setPresetAndProcessOneBlock(1); // "2s"
    CHECK(processor.getActiveGrainLengthSamples() == 88200);
}

TEST_CASE("Changing the Buffer Length preset mid-stream does not allocate on the audio thread", "[bufferLength]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    // Warm up off-tracked, in case anything in the library lazily allocates
    // (e.g. map node allocation) on its very first processBlock call.
    auto warmup = rampBuffer(2, blockSize);
    processor.processBlock(warmup, midi);

    // Build the buffers to feed (and their own heap allocations) before
    // tracking starts, so only the processBlock calls -- the audio thread's
    // actual work -- are measured. Likewise, look up the raw parameter
    // pointer once up front: a real host writes through an already-cached
    // atomic pointer (as the processor itself does internally), never by
    // re-resolving the parameter ID from a string on every block.
    auto blockA = rampBuffer(2, blockSize);
    auto blockB = rampBuffer(2, blockSize);
    auto blockC = rampBuffer(2, blockSize);
    auto* bufferLengthParam = processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::bufferLength);

    std::size_t allocationsDuringProcessing = 0;
    {
        ScopedAllocationTracking tracking;

        *bufferLengthParam = 0.0f; // "1s"
        processor.processBlock(blockA, midi);

        *bufferLengthParam = 2.0f; // "4s"
        processor.processBlock(blockB, midi);

        *bufferLengthParam = 1.0f; // "2s"
        processor.processBlock(blockC, midi);

        // Read the count while still inside the tracked scope, but defer the
        // CHECK itself until tracking is disabled -- Catch2's own assertion
        // machinery may allocate, which would otherwise pollute the count.
        allocationsDuringProcessing = AllocationTracking::count;
    }
    CHECK(allocationsDuringProcessing == 0);
}
