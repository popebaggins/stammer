# Wayfinding Map: reverse-mode-c-vst

## Destination

A fully-specified, decision-free implementation plan for the Reverse Mode C VST3 plugin: stereo-in/stereo-out reverse + octave-delay effect with modulation, <10ms latency, all 7 controls automatable, targeting potential public release (Medium polish: works, sounds good, feature-complete, tested on a few DAWs, clear README).

## Notes

**Domain:** Audio DSP, VST3 plugin development, guitar effects.

**Skills/references:** mattpocock-skills:research (for modulation research), mattpocock-skills:wayfinder (this map), volpitch project (prior real-time VST experience).

**Standing preferences:** 
- Stereo in/out, shared reverse buffer (simpler).
- Linux first, VST3 first.
- Latency target: <10ms ideally (drop WebView if native JUCE UI required).
- Write DSP from scratch; JUCE 8.1+ framework only.
- All controls automatable.
- Listening test only (no numeric verification).
- Potential public release: Medium polish (works, feature-complete, tested on few DAWs, clear README).

## Decisions so far

(None yet — charting in progress.)

## Not yet specified

- **Which DAWs should MVP be tested on?** (Reaper, Studio One, Ableton? 2–3 to start?)
- **Reverse buffer size presets:** What duration options? (1s, 2s, user-variable?)
- **Native JUCE UI design** (if WebView fails latency): Layout, component choices, styling.
- **Public release packaging:** How to distribute VST3? (GitHub releases, installer, zip? Linux distro packages?)
- **Documentation scope:** README only, or user guide? Video demo?

## Out of scope

- Standalone app in MVP (VST3 only; Standalone post-MVP).
- Numeric pitch/frequency verification (listening test sufficient).
- Original Chase Bliss branding or GUI design (custom branding for public release).
- Multi-platform in MVP (Linux first; Mac/Windows post-MVP if needed).
