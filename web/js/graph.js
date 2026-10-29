// Conflict-graph drawing: a deterministic force-directed layout (Fruchterman-
// Reingold) plus an SVG renderer. Nodes and edges are exactly the engine's
// subjects and conflict edges; colours are exactly a colouring it returned.
import { slotColor, slotName } from "./ui.js";

const SVG_NS = "http://www.w3.org/2000/svg";
const layouts = new Map();  // graph signature -> positions[] in [0,1]^2

function seededRandom(seed) {
  let state = seed >>> 0;
  return () => {
    state = (state * 1664525 + 1013904223) >>> 0;
    return state / 4294967296;
  };
}

export function layoutFor(state, signature) {
  if (layouts.has(signature)) return layouts.get(signature);
  const n = state.subjects.length;
  const random = seededRandom(n * 7919 + state.edges.length);
  const pos = state.subjects.map((_, i) => {
    const angle = (2 * Math.PI * i) / Math.max(n, 1);
    return { x: 0.5 + 0.35 * Math.cos(angle) + (random() - 0.5) * 0.02, y: 0.5 + 0.35 * Math.sin(angle) + (random() - 0.5) * 0.02 };
  });
  if (n === 1) pos[0] = { x: 0.5, y: 0.5 };

  const k = 0.75 / Math.sqrt(Math.max(n, 1));
  const iterations = 420;
  for (let step = 0; step < iterations && n > 1; step++) {
    const temperature = 0.08 * (1 - step / iterations) + 0.002;
    const disp = pos.map(() => ({ x: 0, y: 0 }));
    for (let i = 0; i < n; i++) {
      for (let j = i + 1; j < n; j++) {
        let dx = pos[i].x - pos[j].x;
        let dy = pos[i].y - pos[j].y;
        const dist = Math.max(Math.hypot(dx, dy), 0.005);
        const force = (k * k) / dist;
        dx /= dist; dy /= dist;
        disp[i].x += dx * force; disp[i].y += dy * force;
        disp[j].x -= dx * force; disp[j].y -= dy * force;
      }
    }
    for (const edge of state.edges) {
      const a = pos[edge.u];
      const b = pos[edge.v];
      let dx = a.x - b.x;
      let dy = a.y - b.y;
      const dist = Math.max(Math.hypot(dx, dy), 0.005);
      const force = (dist * dist) / k;
      dx /= dist; dy /= dist;
      disp[edge.u].x -= dx * force; disp[edge.u].y -= dy * force;
      disp[edge.v].x += dx * force; disp[edge.v].y += dy * force;
    }
    for (let i = 0; i < n; i++) {
      // gentle gravity keeps disconnected pieces on screen
      disp[i].x += (0.5 - pos[i].x) * 1.4 * k;
      disp[i].y += (0.5 - pos[i].y) * 1.4 * k;
      const length = Math.max(Math.hypot(disp[i].x, disp[i].y), 1e-9);
      const move = Math.min(length, temperature);
      pos[i].x += (disp[i].x / length) * move;
      pos[i].y += (disp[i].y / length) * move;
    }
  }

  // Fit into the unit square. Sparse outliers are pulled in radially
  // (r -> r^0.7) so a dense core is not squeezed by one far-away vertex.
  if (n > 1) {
    const cx = pos.reduce((sum, p) => sum + p.x, 0) / n;
    const cy = pos.reduce((sum, p) => sum + p.y, 0) / n;
    const radii = pos.map((p) => Math.hypot(p.x - cx, p.y - cy));
    const maxR = Math.max(...radii, 1e-6);
    pos.forEach((p, i) => {
      const scale = radii[i] > 0 ? Math.pow(radii[i] / maxR, 0.7) / (radii[i] / maxR) : 1;
      p.x = cx + (p.x - cx) * scale;
      p.y = cy + (p.y - cy) * scale;
    });
    const xs = pos.map((p) => p.x);
    const ys = pos.map((p) => p.y);
    const minX = Math.min(...xs), maxX = Math.max(...xs);
    const minY = Math.min(...ys), maxY = Math.max(...ys);
    const spanX = Math.max(maxX - minX, 1e-6);
    const spanY = Math.max(maxY - minY, 1e-6);
    for (const p of pos) {
      // stretch each axis independently: the canvas is wider than tall
      p.x = 0.5 + ((p.x - (minX + maxX) / 2) / spanX) * 0.94;
      p.y = 0.5 + ((p.y - (minY + maxY) / 2) / spanY) * 0.94;
    }
  }
  layouts.set(signature, pos);
  return pos;
}

export function resetLayout(signature) {
  layouts.delete(signature);
}

function svgEl(tag, attrs = {}) {
  const element = document.createElementNS(SVG_NS, tag);
  for (const [key, value] of Object.entries(attrs)) {
    if (value !== undefined && value !== null) element.setAttribute(key, value);
  }
  return element;
}

// Builds the SVG. `colors` may be null (no colouring shown).
export function drawGraph({ state, positions, colors = null, mini = false, clique = null, label }) {
  const width = mini ? 520 : 1000;
  const height = mini ? 360 : 680;
  const pad = mini ? 34 : 56;
  const n = state.subjects.length;
  const radius = mini ? (n <= 8 ? 17 : n <= 16 ? 13 : 9) : n <= 10 ? 24 : n <= 20 ? 19 : 15;
  const svg = svgEl("svg", {
    viewBox: `0 0 ${width} ${height}`,
    class: `graph${mini ? " graph-mini" : ""}`,
    role: "img",
    "aria-label": label || `Conflict graph with ${n} subjects and ${state.edges.length} conflicts`,
  });
  const toX = (p) => pad + p.x * (width - 2 * pad);
  const toY = (p) => pad + p.y * (height - 2 * pad);
  const maxWeight = Math.max(1, ...state.edges.map((e) => e.weight));
  const inClique = new Set(clique || []);

  const edgeLayer = svgEl("g", { class: "edges" });
  state.edges.forEach((edge, index) => {
    const a = positions[edge.u];
    const b = positions[edge.v];
    const clash = colors && colors[edge.u] >= 0 && colors[edge.u] === colors[edge.v];
    const group = svgEl("g", {
      class: `edge${clash ? " clash" : ""}${inClique.has(edge.u) && inClique.has(edge.v) ? " in-clique" : ""}`,
      "data-edge": index, "data-u": edge.u, "data-v": edge.v,
    });
    const coords = { x1: toX(a), y1: toY(a), x2: toX(b), y2: toY(b) };
    group.append(
      svgEl("line", { ...coords, class: "edge-line", "stroke-width": (mini ? 1.2 : 1.4) + (mini ? 1.5 : 3) * (edge.weight / maxWeight) }),
    );
    if (!mini) group.append(svgEl("line", { ...coords, class: "edge-hit" }));
    edgeLayer.append(group);
  });

  const nodeLayer = svgEl("g", { class: "nodes" });
  state.subjects.forEach((subject, index) => {
    const p = positions[index];
    const color = colors ? colors[index] : -1;
    const group = svgEl("g", {
      class: `node${color >= 0 ? " colored" : ""}${inClique.has(index) ? " in-clique" : ""}`,
      "data-index": index,
      transform: `translate(${toX(p).toFixed(1)} ${toY(p).toFixed(1)})`,
      style: color >= 0 ? `--slot:${slotColor(color)}` : null,
    });
    if (!mini) {
      group.setAttribute("tabindex", "0");
      group.setAttribute("role", "button");
      group.setAttribute(
        "aria-label",
        `${subject.code} ${subject.name}, ${subject.degree} conflicts${color >= 0 ? `, slot ${color + 1}` : ""}`,
      );
    }
    group.append(svgEl("circle", { r: radius + 4, class: "node-halo" }));
    group.append(svgEl("circle", { r: radius, class: "node-dot" }));
    const text = svgEl("text", { class: "node-label", "text-anchor": "middle", dy: "0.35em", "font-size": Math.max(8, radius * 0.62) });
    text.textContent = subject.code.length > 6 ? `${subject.code.slice(0, 5)}…` : subject.code;
    group.append(text);
    if (color >= 0) {
      const tag = svgEl("text", { class: "node-slot", "text-anchor": "middle", y: radius + (mini ? 11 : 15), "font-size": mini ? 9.5 : 11.5 });
      tag.textContent = slotName(color);
      group.append(tag);
    }
    nodeLayer.append(group);
  });

  svg.append(edgeLayer, nodeLayer);
  svg.__geometry = { width, height, pad };
  return svg;
}
