import type { Component, ParentProps } from "solid-js";

export const ScreenInfo: Component<ParentProps> = (props) => (
  <div class="grid p-4 sm:p-8 h-full justify-center items-center">
    <div class="text-center h-full flex justify-center items-center">
      <div>{props.children}</div>
    </div>
  </div>
);
