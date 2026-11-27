# Reverse Mode C — WebView UI

React/TypeScript frontend for the plugin's `juce::WebBrowserComponent`
editor. Built with esbuild and the official `@juce-framework/webview`
bindings for the slider/toggle relay bridge to the audio parameters.

Requires Node.js (npm). The top-level CMake build runs `npm ci` and
`npm run build` automatically and embeds `dist/index.html`, `dist/index.js`
and `dist/index.css` into the plugin binary as binary resources — nothing
here is loaded from disk at runtime.

## Local development

```sh
npm install
npm run dev     # esbuild --serve on http://localhost:3000, for iterating on
                 # layout/styling in a regular browser without a JUCE host
npm run build    # production build -> dist/
npm run typecheck
```

`npm run dev` serves the UI standalone; `Juce.getSliderState`/`getToggleState`
fall back to no-op placeholders when `window.__JUCE__` isn't present (i.e.
outside the plugin), so the page renders but controls won't move audio.
