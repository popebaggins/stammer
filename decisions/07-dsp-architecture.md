# Plan: DSP architecture for reverse + octave-delay + modulation

## Question

Design the DSP architecture for the core effect:
1. **Reverse buffer**: How is audio captured and reversed? (Simple time-reversal, or octave-shifted while reversing?)
2. **Octave delay**: How does octave-shifting work in combination? (Pitch shifting + delay, or separate octave stages?)
3. **Modulation**: Once we know the modulation type (from Research ticket #1), how does it modulate the effect?
4. **Parameter mapping**: Which parameters control which DSP stages? (Mix → dry/wet blend; Speed → reverse buffer playback speed or delay time?)
5. **Thread safety**: Audio thread polling for parameter updates, no blocking.

This ticket becomes actionable once modulation research is complete.

## Type: Plan (HITL + research dependency)

**Assigned to:** <awaiting claim after modulation research>

**Blockers:** "Modulation research" (need to know modulation type first)

**Blocks:** "JUCE project setup" (architecture informs code structure)
