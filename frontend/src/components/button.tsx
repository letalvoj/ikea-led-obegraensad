import type { Component, JSX } from "solid-js";

export const Button: Component<
  JSX.ButtonHTMLAttributes<HTMLButtonElement> & { widthAuto?: boolean }
> = (props) => {
  return (
    <button {...props} class={`${props.widthAuto ? "w-auto" : "w-full"} btn ${props.class || ""}`}>
      {props.children}
    </button>
  );
};

export default Button;
