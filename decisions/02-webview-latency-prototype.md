# Prototype: WebView latency validation — can JUCE WebView + React hit <10ms?

## Question

Prototype a minimal JUCE VST3 plugin with WebView + React UI:
- Create a simple JUCE/VST3 shell with WebView embedded
- Build a minimal React component (a slider to control a parameter)
- Measure end-to-end latency: parameter change in React → audio callback sees new value
- Target: <10ms latency (or report why it's not achievable)

If WebView can't hit <10ms, we'll pivot to native JUCE UI components (simpler, faster).

## Type: Prototype (HITL)

**Assigned to:** <awaiting claim>

**Blockers:** None

**Blocks:** "UI framework decision" (if WebView fails), "DSP architecture" (latency shapes how DSP is threaded)

## Decision to follow

If WebView < 10ms: Use WebView + React for UI.
If WebView ≥ 10ms: Use native JUCE UI components.
