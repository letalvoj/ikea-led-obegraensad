import type { Component, JSX } from "solid-js";

import { useStore } from "../contexts/store";
import { IS_BLE } from "../transport";

// Catalogue furniture for the UI: the masthead, the concrete floor slabs of the
// cut-away house, product labels with shelf price tags, and line drawings in the
// style of flat-pack assembly instructions.

export const Slab: Component<{ class?: string }> = (props) => (
  <div class={`slab shrink-0 ${props.class ?? ""}`} aria-hidden="true" />
);

export const Masthead: Component = () => {
  const [store] = useStore();
  const connected = () => store.connectionState() === 1;

  return (
    <header class="shrink-0 bg-brand text-white">
      <div class="flex items-center gap-4 px-4 lg:px-6 h-14 lg:h-16">
        <a href="#/" class="flex items-baseline gap-3 min-w-0" aria-label="Home">
          <span class="text-accent font-bold text-xl lg:text-2xl tracking-tight">OBEGRÄNSAD</span>
          <span class="hidden sm:inline text-xs text-white/80 truncate">
            LED wall lamp, 16×16 pixels
          </span>
        </a>
        <span class="ml-auto flex items-center gap-2 text-xs font-bold uppercase tracking-wide">
          <span
            class={`w-2.5 h-2.5 rounded-full ${connected() ? "bg-accent" : "bg-white/40"}`}
            aria-hidden="true"
          />
          {IS_BLE ? "Bluetooth" : "WiFi"}
        </span>
      </div>
    </header>
  );
};

/** A room's wall to hang the lamp on: yellow paint, a wooden floor, a slab below. */
export const Wall: Component<{ children: JSX.Element }> = (props) => (
  <div class="min-h-full flex flex-col bg-wall">
    <div class="flex-1 grid place-items-center p-6 lg:p-10">{props.children}</div>
    {/* the sofa the lamp hangs over, only where there is height to spare */}
    <svg
      viewBox="-12 -2 290 84"
      class="hidden [@media(min-height:1000px)]:block w-72 self-center -mb-px"
      aria-hidden="true"
    >
      <StripedSofa x={0} y={0} />
    </svg>
    <div class="h-5 shrink-0 bg-floor" aria-hidden="true" />
    <Slab />
  </div>
);

/** "BILLY bookcase, white. See p. 77. 395:-/st", as printed next to the product. */
export const ProductLabel: Component<{ name: string; page: number; description: string }> = (
  props,
) => (
  <div class="flex items-end gap-5 text-ink">
    <div class="leading-tight">
      <div class="text-sm">
        <span class="font-bold uppercase">{props.name}</span> {props.description}
      </div>
      <div class="text-xs">See p. {props.page}.</div>
    </div>
    <div class="text-4xl font-bold tracking-tighter leading-none">
      0:-<span class="text-base tracking-normal">/st</span>
    </div>
  </div>
);

export const PriceTag: Component<{ children: JSX.Element; class?: string }> = (props) => (
  <span class={`price-tag text-3xl ${props.class ?? ""}`}>{props.children}</span>
);

const stroke = {
  fill: "none",
  stroke: "currentColor",
  "stroke-width": 2,
  "stroke-linecap": "round",
  "stroke-linejoin": "round",
} as const;

/** The lamp on a wall, a plug trailing down to the socket. */
export const LampDrawing: Component<{ class?: string }> = (props) => (
  <svg viewBox="0 0 120 120" class={props.class} aria-hidden="true">
    <rect x="30" y="6" width="52" height="75" rx="2" {...stroke} />
    <g fill="currentColor">
      {/* a few lit pixels: the dude, of course */}
      <rect x="52" y="52" width="4" height="4" />
      <rect x="48" y="57" width="12" height="4" />
      <rect x="52" y="62" width="4" height="4" />
      <rect x="48" y="67" width="4" height="4" />
      <rect x="56" y="67" width="4" height="4" />
    </g>
    <path d="M56 81 C56 98 92 90 92 104" {...stroke} />
    <rect x="86" y="104" width="12" height="10" rx="2" {...stroke} />
  </svg>
);

/** A laptop broadcasting to the lamp. */
export const PairDrawing: Component<{ class?: string }> = (props) => (
  <svg viewBox="0 0 120 120" class={props.class} aria-hidden="true">
    <rect x="78" y="10" width="30" height="43" rx="2" {...stroke} />
    <path d="M14 70 h52 v32 h-52 z M6 108 h68" {...stroke} />
    <path d="M50 52 a14 14 0 0 1 14 -14 M42 52 a22 22 0 0 1 22 -22" {...stroke} />
    <path d="M34 80 l6 6 l-6 6 M40 86 h-8 m4 -12 v24 l6 -6 l-12 -12" {...stroke} />
  </svg>
);

/** A hand typing the pairing code on a keypad. */
export const CodeDrawing: Component<{ class?: string }> = (props) => (
  <svg viewBox="0 0 120 120" class={props.class} aria-hidden="true">
    <rect x="20" y="16" width="80" height="30" rx="3" {...stroke} />
    <g fill="currentColor">
      {[0, 1, 2, 3, 4, 5].map((i) => (
        <circle cx={31 + i * 11.6} cy="31" r="3" />
      ))}
    </g>
    {[0, 1, 2].map((row) =>
      [0, 1, 2].map((col) => (
        <rect x={32 + col * 20} y={56 + row * 18} width="14" height="12" rx="2" {...stroke} />
      )),
    )}
    <path d="M86 96 l10 -18 c3 -5 10 -2 8 3 l-4 10 h8 c4 0 6 4 4 8 l-8 16 h-20 z" {...stroke} />
  </svg>
);

// Box Dude in his box, as the lamp would show him (16 rows, '#' lit, '+' dim).
const LAMP_PIXELS = [
  "################",
  "#..............#",
  "#.+.+.+.+......#",
  "#.+.+.+.+......#",
  "#.+.+.+.+......#",
  "#..............#",
  "#..............#",
  "#..............#",
  "#..............#",
  "#..............#",
  "#.........#....#",
  "#........###...#",
  "#.........#....#",
  "#........#.#...#",
  "#..............#",
  "################",
];

// the real panel is taller than it is wide, like the live preview (9:13)
const PANEL_ASPECT = 13 / 9;

const LampPixels: Component<{ x: number; y: number; cell: number }> = (props) => (
  <g>
    <rect
      x={props.x - 3}
      y={props.y - 3}
      width={props.cell * 16 + 6}
      height={props.cell * PANEL_ASPECT * 16 + 6}
      fill="#1b1b1b"
    />
    {LAMP_PIXELS.flatMap((row, y) =>
      [...row].map((c, x) =>
        c === "." ? null : (
          <rect
            x={props.x + x * props.cell + 0.6}
            y={props.y + y * props.cell * PANEL_ASPECT + 0.6}
            width={props.cell - 1.2}
            height={props.cell * PANEL_ASPECT - 1.2}
            fill={c === "#" ? "#fff8e0" : "#8a8470"}
            opacity={c === "#" && (x === 0 || x === 15 || y === 0 || y === 15) ? 0.35 : 1}
          />
        ),
      ),
    )}
  </g>
);

const Bookcase: Component<{ x: number; y: number }> = (props) => (
  <g transform={`translate(${props.x} ${props.y})`}>
    <rect width="120" height="210" fill="#fbfaf6" />
    {[0, 1, 2, 3].map((shelf) => (
      <g transform={`translate(8 ${8 + shelf * 50})`}>
        {BOOK_COLORS.map((color, i) => (
          <rect
            x={i * 11.4 + ((shelf + i) % 3)}
            y={(shelf * 7 + i * 5) % 9}
            width="10"
            height={42 - ((shelf * 7 + i * 5) % 9)}
            fill={color}
          />
        ))}
      </g>
    ))}
    <path d="M0 50 h120 M0 100 h120 M0 150 h120" stroke="#e3dfd3" stroke-width="3" />
  </g>
);

const BOOK_COLORS = [
  "#c0392b",
  "#2c5d9e",
  "#e8e1d0",
  "#3a3a3a",
  "#d98c2b",
  "#6b8f3a",
  "#8e6fb0",
  "#c0392b",
  "#2c5d9e",
];

const StripedSofa: Component<{ x: number; y: number }> = (props) => (
  <g transform={`translate(${props.x} ${props.y})`}>
    <rect width="264" height="60" rx="8" fill="#fff" />
    {[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map((i) => (
      <rect x={6 + i * 24} y="0" width="12" height="60" fill="#d8352a" />
    ))}
    <rect x="-10" y="18" width="30" height="52" rx="8" fill="#fff" stroke="#d8352a" />
    <rect x="244" y="18" width="30" height="52" rx="8" fill="#fff" stroke="#d8352a" />
    <rect x="10" y="70" width="6" height="8" fill="#3a3a3a" />
    <rect x="248" y="70" width="6" height="8" fill="#3a3a3a" />
  </g>
);

/**
 * A room from the catalogue's cut-away house, between two concrete floor slabs:
 * the lamp on a yellow wall with its price callout, a striped sofa, and the
 * neighbours' blue room with a bookcase behind the column. The headline (children)
 * goes over the empty wall on the left.
 */
export const RoomHero: Component<{ children?: JSX.Element }> = (props) => (
  <figure class="relative overflow-hidden flex flex-col">
    <Slab />
    <svg viewBox="0 0 800 300" class="block w-full h-auto" aria-hidden="true">
      <rect width="800" height="300" fill="#f3cf3a" />
      <rect x="600" width="200" height="300" fill="#7f9fd2" />
      <rect x="588" width="14" height="300" fill="#8c8a84" />
      <rect y="268" width="800" height="32" fill="#c79d6b" />
      <Bookcase x={642} y={58} />

      <StripedSofa x={40} y={190} />
      <path d="M560 268 v-150 l-24 -28" stroke="#3a3a3a" stroke-width="4" fill="none" />
      <path d="M512 92 a26 18 0 0 1 48 -14 z" fill="#fbfaf6" />
      <ellipse cx="560" cy="270" rx="16" ry="4" fill="#3a3a3a" />

      <LampPixels x={400} y={34} cell={5.4} />
      <g font-family="Verdana, Geneva, sans-serif" fill="#111">
        <text x="400" y="186" font-size="11">
          <tspan font-weight="bold">OBEGRÄNSAD</tspan> LED wall lamp
        </text>
        <text x="400" y="200" font-size="10">
          See p. 1.
        </text>
        <text x="400" y="236" font-size="34" font-weight="bold" letter-spacing="-1.5">
          0:-
          <tspan font-size="15" letter-spacing="0">
            /st
          </tspan>
        </text>
      </g>
    </svg>
    <Slab />
    <figcaption class="order-first sm:order-none pb-4 sm:absolute sm:left-0 sm:top-0 sm:w-1/2 sm:p-5 lg:p-8 sm:text-white sm:[text-shadow:0_2px_12px_#0005]">
      {props.children}
    </figcaption>
  </figure>
);

export const AssemblyStep: Component<{
  step: number;
  children: JSX.Element;
  drawing: JSX.Element;
}> = (props) => (
  <li class="flex sm:flex-col items-center gap-4 sm:gap-3 sm:text-center">
    <div class="relative w-24 shrink-0 sm:w-full sm:max-w-40 aspect-square text-ink">
      <span class="absolute -left-1 -top-1 w-7 h-7 sm:w-8 sm:h-8 rounded-full border-2 border-ink bg-white grid place-items-center font-bold text-sm">
        {props.step}
      </span>
      {props.drawing}
    </div>
    <p class="text-sm leading-snug">{props.children}</p>
  </li>
);
