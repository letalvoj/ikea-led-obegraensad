import type { Component } from "solid-js";

const Kbd: Component<{ children: string }> = (props) => (
  <kbd class="px-1.5 py-0.5 bg-paper rounded border border-line font-mono">{props.children}</kbd>
);

export const KeyboardShortcutsHelp: Component = () => {
  return (
    <div class="space-y-3">
      <h3 class="section-title">Keyboard Shortcuts</h3>
      <div class="text-xs text-muted space-y-3">
        <p>
          <Kbd>←</Kbd> <Kbd>→</Kbd> Navigate frames
        </p>
        <p>
          <Kbd>Space</Kbd> Play/Stop
        </p>
        <p>
          <Kbd>D</Kbd> Duplicate frame
        </p>
        <p>
          <Kbd>Del</Kbd> Delete frame
        </p>
        <p>
          <Kbd>Ctrl+Z</Kbd> Undo
        </p>
        <p>
          <Kbd>Ctrl+Shift+Z</Kbd> Redo
        </p>
      </div>
    </div>
  );
};
