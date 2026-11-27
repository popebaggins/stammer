import { useEffect } from "react";
import * as Juce from "@juce-framework/webview";

import ParamSlider from "./components/ParamSlider";
import ParamToggle from "./components/ParamToggle";
import "./App.css";

const controlParameterIndexAttribute = "controlparameterindex";

function App() {
  // Reports which control is under the mouse to the host, so a DAW's
  // right-click "assign MIDI/automation" context menu targets the right
  // parameter (see WebControlParameterIndexReceiver on the C++ side).
  useEffect(() => {
    const updater = new Juce.ControlParameterIndexUpdater(
      controlParameterIndexAttribute,
    );
    const handleMouseMove = (event: MouseEvent) => updater.handleMouseMove(event);
    document.addEventListener("mousemove", handleMouseMove);
    return () => document.removeEventListener("mousemove", handleMouseMove);
  }, []);

  return (
    <div className="plugin">
      <header className="plugin-header">
        <h1>Reverse Mode C</h1>
      </header>

      <section className="knob-row">
        <ParamSlider identifier="mix" label="Mix" />
        <ParamSlider identifier="time" label="Time" />
        <ParamSlider identifier="feedback" label="Feedback" />
        <ParamSlider identifier="balance" label="Balance" />
        <ParamSlider identifier="filter" label="Filter" />
        <ParamSlider identifier="modDepth" label="Mod Depth" />
        <ParamSlider identifier="modRate" label="Mod Rate" />
      </section>

      <section className="toggle-row">
        <ParamToggle identifier="modSync" label="Mod Sync" />
        <ParamToggle identifier="bypass" label="Bypass" />
      </section>
    </div>
  );
}

export default App;
