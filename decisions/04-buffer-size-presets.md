# Decision: Reverse buffer size presets

## Question

The reverse buffer has a fixed size. Should it be:
- (a) **User-selectable between presets** (e.g., 1s, 2s, 4s)?
- (b) **Single fixed size** (e.g., 2s)?
- (c) **Continuous slider** (user can adjust 0.5s–4s in real-time)?

Option (a) is simplest; option (c) is most flexible but adds complexity (can't shrink buffer mid-note without artifacts).

Recommend: **(a) User-selectable presets.** Offer 1s, 2s, 4s via a dropdown. Users pick before playing; no real-time resizing complexity.

## Type: Decision (HITL Grilling)

**Assigned to:** <awaiting claim>

**Blockers:** None

**Blocks:** None (UI/UX decision)
