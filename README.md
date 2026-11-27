# Reverse Mode C

A Linux VST3 effect plugin inspired by the reverse-delay behavior of the
Chase Bliss Audio Reverse Mode C guitar pedal. Stereo in/stereo out, built
from scratch in C++ with the JUCE framework, with a React/TypeScript
WebView UI.

**Independence disclaimer:** This project is an independent software clone
inspired by the *behavior* of the Chase Bliss Audio Reverse Mode C pedal.
It uses no original Chase Bliss Audio branding, artwork, firmware, or code,
and is **not affiliated with, endorsed by, or sponsored by Chase Bliss
Audio** in any way. "Chase Bliss Audio" and "Reverse Mode C" are mentioned
here only to describe the pedal that inspired this plugin's sound.

## Features

- **Granular reverse buffer** — the input signal is captured and played
  back in reversed grains for the classic "reverse delay" texture.
- **Octave-up voice** — a pitched-up reversed voice blended in alongside
  the straight reverse and dry (forward) signals.
- **3-way Balance crossfade** — continuously blend between the dry
  (Forward), Reverse, and Octave-up Reverse voices.
- **Feedback loop** — feed the reversed signal back into the buffer for
  building, decaying repeats.
- **Morphing filter** — a single control that sweeps from highpass,
  through a clean pass-through, to lowpass, shaping the feedback/reverse
  tone.
- **Vibrato modulation** — a post-mix modulated delay (sine LFO) adds
  pitch-wobble movement, with a Rate control that can free-run in Hz or
  sync to host tempo.
- **WebView UI** — a React-based plugin interface running in an embedded
  browser view, bound live to all 9 parameters.
- All 9 parameters are host-automatable and their state is saved/restored
  with your DAW project.

## Installing (Linux VST3)

This plugin ships as a single `.vst3` bundle (a directory, not a flat
file). Linux VST3 hosts scan these standard locations:

- **Per-user (recommended, no root required):**
  `~/.vst3/`
- **System-wide (all users):**
  `/usr/lib/vst3/`

To install, copy (or move) the whole `Reverse Mode C.vst3` folder into one
of those directories, e.g.:

```sh
mkdir -p ~/.vst3
cp -r "Reverse Mode C.vst3" ~/.vst3/
```

Then rescan plugins in your DAW:

- **Reaper:** Options → Preferences → Plug-ins → VST → "Re-scan" (or
  "Clear cache/re-scan"). The plugin will appear as "Reverse Mode C" under
  its manufacturer, "Reverse Mode C Project".
- **Bitwig Studio:** Settings → Locations → Plug-in Locations, confirm
  `~/.vst3` (or `/usr/lib/vst3`) is listed, then rescan. See **Known
  issues** below — Bitwig has not been verified by the plugin author.
- **Any standard Linux VST3 host:** most hosts scan `~/.vst3` and
  `/usr/lib/vst3` automatically; use your host's plugin-rescan option if
  the plugin doesn't appear immediately.

## Parameters

| Parameter | Range | Description |
|---|---|---|
| **Mix** | 0–100% | Dry/wet blend between the unprocessed input and the full effect (reverse/octave/feedback/filter/vibrato chain). |
| **Time** | 0.1–4.0 s | Length of the reverse buffer — how far back audio is captured before being played back reversed. Longer times give longer reverse "grains." |
| **Feedback** | 0–100% | Amount of the reversed output fed back into the buffer, for building/decaying repeats. |
| **Balance** | -100% to +100% | 3-way crossfade: full left (-100%) is the dry Forward voice only, center (0%) is the Reverse voice only, full right (+100%) is the Octave-up Reverse voice only. Sweeping through the middle blends continuously between all three. |
| **Filter** | 0–100% | Morphing filter on the wet signal: 0% is an aggressive highpass, 50% is a clean pass-through (no filtering), 100% is an aggressive lowpass. |
| **Mod Depth** | 0–100% | Depth of the post-mix vibrato modulation (0% = no modulation, fully transparent). |
| **Mod Rate** | 0.1–10.0 Hz | Speed of the vibrato LFO. In Free mode this is an absolute rate in Hz; in Sync mode it's a multiplier of the host's tempo (beats per second). |
| **Mod Sync / Free** | On / Off | Toggles whether Mod Rate is interpreted as tempo-synced (On) or a free-running Hz value (Off). Falls back to 120 BPM if the host reports no tempo. |
| **Bypass** | On / Off | Hard-bypasses the entire effect, passing the input through unprocessed. |

## Known issues

- **Primarily tested in Reaper.** Day-to-day development and listening
  tests for this release were done in Reaper on Linux. Other hosts should
  work (the plugin uses standard JUCE VST3 plumbing and has an automated
  test suite covering parameter layout, DSP, and the WebView editor), but
  they have not been manually verified.
- **Bitwig Studio was not verified.** Loading, UI rendering/resizing, and
  automation in Bitwig Studio were not tested in this development cycle.
  If you try it and run into issues, please file a report.
- **Full listening-test verification across all parameter combinations**
  was not completed for this release. The DSP is covered by an extensive
  automated test suite, but exhaustive by-ear verification of every
  parameter combination (e.g. extreme Feedback + Filter + Mod Depth
  settings together) was not performed.
- Linux VST3 only — no Windows or macOS builds are provided at this time.

## Building from source

Requires CMake 3.22+, a C++20 compiler, and Node.js/npm (for the WebView
UI). JUCE and Catch2 are fetched automatically by CMake.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(nproc)"
```

The built `.vst3` is installed automatically to `~/.vst3/` by the build
(`COPY_PLUGIN_AFTER_BUILD`), and also available under
`build/ReverseModeC_artefacts/Release/VST3/`.

To run the test suite:

```sh
./build/tests/reverse_mode_c_tests
```
