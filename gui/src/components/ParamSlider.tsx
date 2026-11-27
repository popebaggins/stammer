import { useEffect, useState } from "react";
import type { ChangeEvent, CSSProperties } from "react";
import * as Juce from "@juce-framework/webview";

// Custom attributes must be all-lowercase to survive React's DOM prop
// handling; this is what AudioProcessorEditor::getControlParameterIndex
// reads back via WebControlParameterIndexReceiver, letting the host's
// right-click "show automation"/MIDI-learn context menu target the right
// parameter when it's over this control.
const controlParameterIndexAttribute = "controlparameterindex";

interface ParamSliderProps {
  identifier: string;
  label: string;
}

// A continuous-parameter control, backed by a WebSliderRelay on the C++
// side (see PluginEditor.cpp). Binds bidirectionally: dragging the slider
// here pushes a normalised value to the audio parameter, and a value
// pushed from the backend (host automation, a preset load, undo) re-renders
// this slider via the valueChangedEvent listener below.
export default function ParamSlider({ identifier, label }: ParamSliderProps) {
  const sliderState = Juce.getSliderState(identifier);

  const [value, setValue] = useState(sliderState.getNormalisedValue());
  const [properties, setProperties] = useState(sliderState.properties);

  useEffect(() => {
    const valueListenerId = sliderState.valueChangedEvent.addListener(() => {
      setValue(sliderState.getNormalisedValue());
    });
    const propertiesListenerId = sliderState.propertiesChangedEvent.addListener(
      () => setProperties(sliderState.properties),
    );

    return () => {
      sliderState.valueChangedEvent.removeListener(valueListenerId);
      sliderState.propertiesChangedEvent.removeListener(propertiesListenerId);
    };
  }, [sliderState]);

  const handleInput = (event: ChangeEvent<HTMLInputElement>) => {
    const newValue = Number(event.target.value);
    sliderState.setNormalisedValue(newValue);
    setValue(newValue);
  };

  const beginGesture = () => sliderState.sliderDragStarted();
  const endGesture = () => sliderState.sliderDragEnded();

  const numSteps = Math.max(properties.numSteps, 2);
  const displayValue = sliderState.getScaledValue();
  const decimals = numSteps > 100 ? 2 : 0;

  return (
    <div
      className="param-control"
      {...{ [controlParameterIndexAttribute]: properties.parameterIndex }}
    >
      <label className="param-label" htmlFor={identifier}>
        {properties.name || label}
      </label>
      <input
        id={identifier}
        className="param-slider"
        type="range"
        min={0}
        max={1}
        step={1 / (numSteps - 1)}
        value={value}
        style={{ "--fill": `${value * 100}%` } as CSSProperties}
        onChange={handleInput}
        onMouseDown={beginGesture}
        onMouseUp={endGesture}
        onTouchStart={beginGesture}
        onTouchEnd={endGesture}
        onKeyDown={beginGesture}
        onKeyUp={endGesture}
        aria-label={label}
      />
      <span className="param-value">
        {displayValue.toFixed(decimals)}
        {properties.label ? ` ${properties.label}` : ""}
      </span>
    </div>
  );
}
