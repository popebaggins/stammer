import { useEffect, useState } from "react";
import type { ChangeEvent } from "react";
import * as Juce from "@juce-framework/webview";

const controlParameterIndexAttribute = "controlparameterindex";

interface ParamToggleProps {
  identifier: string;
  label: string;
}

// A boolean-parameter control, backed by a WebToggleButtonRelay on the C++
// side. Same bidirectional contract as ParamSlider: toggling here sends the
// new state to the audio parameter, and the backend's valueChangedEvent
// (host automation, preset load) updates this switch in turn.
export default function ParamToggle({ identifier, label }: ParamToggleProps) {
  const toggleState = Juce.getToggleState(identifier);

  const [value, setValue] = useState(toggleState.getValue());
  const [properties, setProperties] = useState(toggleState.properties);

  useEffect(() => {
    const valueListenerId = toggleState.valueChangedEvent.addListener(() => {
      setValue(toggleState.getValue());
    });
    const propertiesListenerId = toggleState.propertiesChangedEvent.addListener(
      () => setProperties(toggleState.properties),
    );

    return () => {
      toggleState.valueChangedEvent.removeListener(valueListenerId);
      toggleState.propertiesChangedEvent.removeListener(propertiesListenerId);
    };
  }, [toggleState]);

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    toggleState.setValue(event.target.checked);
    setValue(event.target.checked);
  };

  return (
    <div
      className="param-control param-control--toggle"
      {...{ [controlParameterIndexAttribute]: properties.parameterIndex }}
    >
      <label className="param-toggle-label">
        <input
          className="param-toggle"
          type="checkbox"
          checked={value}
          onChange={handleChange}
          aria-label={label}
        />
        <span className="param-toggle-switch" aria-hidden="true" />
        <span className="param-label">{properties.name || label}</span>
      </label>
    </div>
  );
}
