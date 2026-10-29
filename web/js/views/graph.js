// Conflict graph: real subjects as nodes, real shared-batch edges, coloured
// by the engine's latest timetable. Drag, hover, click, or use the keyboard.
import { h, icon, button, badge, fmt, emptyState, sectionHeader, slotDot, slotName } from "../ui.js";
import { store, notify, neighbours, subjectCode } from "../store.js";
import { drawGraph, layoutFor, resetLayout } from "../graph.js";

let colorMode = "timetable";   // 'timetable' | 'none'
let showClique = false;
let selection = null;          // { type: 'node', index } | { type: 'edge', index }

export function render(root, { navigate }) {
  const state = store.state;
  if (!state) return;
  root.append(
    h("header", { class: "page-head" },
      h("div", {},
        h("h2", { class: "page-title" }, "Conflict graph"),
        h("p", { class: "lede" }, `${fmt.plural(state.subjects.length, "subject")} and ${fmt.plural(state.edges.length, "conflict")}. An edge joins two subjects taken by the same student batch, so they need different slots.`),
      ),
    ),
  );
  if (state.subjects.length === 0) {
    root.append(h("div", { class: "card" }, emptyState({
      iconName: "graph", title: "The graph is empty",
      body: "Add subjects and batches, or load a sample dataset, to see conflicts appear.",
      actions: [button("Go to Data", { variant: "primary", iconName: "arrow", onClick: () => navigate("data") })],
    })));
    return;
  }
  if (selection && ((selection.type === "node" && selection.index >= state.subjects.length) ||
      (selection.type === "edge" && selection.index >= state.edges.length))) selection = null;

  const last = state.last;
  const colors = colorMode === "timetable" && last ? last.colors : null;
  const positions = layoutFor(state, store.signature);
  const svg = drawGraph({ state, positions, colors, clique: showClique ? state.stats.clique : null });
  const canvas = h("div", { class: `graph-canvas${showClique ? " show-clique" : ""}` }, svg);
  wire(svg, positions);

  const toolbar = h("div", { class: "graph-toolbar" },
    h("div", { class: "segmented compact", role: "radiogroup", "aria-label": "Node colours" },
      segButton("Latest timetable", colorMode === "timetable", () => { colorMode = "timetable"; notify(); }, !last),
      segButton("No colours", colorMode === "none", () => { colorMode = "none"; notify(); })),
    h("label", { class: "check" },
      h("input", { type: "checkbox", id: "graph-clique", checked: showClique, onchange: (e) => { showClique = e.target.checked; notify(); } }),
      h("span", {}, `Highlight clique (lower bound ${state.stats.clique.length})`)),
    button("Re-layout", { size: "sm", variant: "ghost", iconName: "refresh", onClick: () => { resetLayout(store.signature); notify(); } }),
  );

  root.append(
    h("div", { class: "graph-layout" },
      h("section", { class: "card graph-card" }, toolbar, canvas,
        h("p", { class: "muted small" }, "Drag nodes to rearrange · Tab through subjects · Enter to select · Esc to clear. Edge thickness = shared students.")),
      h("aside", { class: "card inspector-card", "aria-live": "polite" }, inspectorPanel(colors)),
    ),
    pairsTable(),
  );
}

function segButton(label, on, onClick, disabled = false) {
  return h("button", { type: "button", class: `seg-btn${on ? " on" : ""}`, role: "radio", "aria-checked": String(on), onclick: onClick, disabled, title: disabled ? "Generate a timetable first" : null }, label);
}

function wire(svg, positions) {
  const state = store.state;
  const nodes = [...svg.querySelectorAll(".node")];
  const edges = [...svg.querySelectorAll(".edge")];
  const { width, height, pad } = svg.__geometry;

  const focusOn = (index) => {
    const near = new Set(neighbours(index));
    svg.classList.add("hovering");
    nodes.forEach((n) => {
      const i = Number(n.dataset.index);
      n.classList.toggle("hot", i === index || near.has(i));
    });
    edges.forEach((e) => e.classList.toggle("hot", Number(e.dataset.u) === index || Number(e.dataset.v) === index));
  };
  const clear = () => {
    svg.classList.remove("hovering");
    nodes.forEach((n) => n.classList.remove("hot"));
    edges.forEach((e) => e.classList.remove("hot"));
  };
  const applySelection = () => {
    nodes.forEach((n) => n.classList.toggle("selected", selection && selection.type === "node" && Number(n.dataset.index) === selection.index));
    edges.forEach((e) => e.classList.toggle("selected", selection && selection.type === "edge" && Number(e.dataset.edge) === selection.index));
  };
  applySelection();

  nodes.forEach((node) => {
    const index = Number(node.dataset.index);
    node.addEventListener("pointerenter", () => focusOn(index));
    node.addEventListener("pointerleave", clear);
    node.addEventListener("focus", () => focusOn(index));
    node.addEventListener("blur", clear);
    node.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        selection = { type: "node", index };
        notify();
      } else if (event.key === "Escape") {
        selection = null;
        notify();
      }
    });

    // Drag (pointer events work for mouse, pen and touch).
    node.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      try { node.setPointerCapture(event.pointerId); } catch { /* synthetic or lost pointer */ }
      const rect = svg.getBoundingClientRect();
      let moved = false;
      const onMove = (move) => {
        moved = true;
        const x = ((move.clientX - rect.left) / rect.width) * width;
        const y = ((move.clientY - rect.top) / rect.height) * height;
        positions[index] = {
          x: Math.min(1, Math.max(0, (x - pad) / (width - 2 * pad))),
          y: Math.min(1, Math.max(0, (y - pad) / (height - 2 * pad))),
        };
        const px = pad + positions[index].x * (width - 2 * pad);
        const py = pad + positions[index].y * (height - 2 * pad);
        node.setAttribute("transform", `translate(${px.toFixed(1)} ${py.toFixed(1)})`);
        edges.forEach((edge) => {
          const u = Number(edge.dataset.u);
          const v = Number(edge.dataset.v);
          if (u !== index && v !== index) return;
          for (const line of edge.querySelectorAll("line")) {
            if (u === index) { line.setAttribute("x1", px); line.setAttribute("y1", py); }
            else { line.setAttribute("x2", px); line.setAttribute("y2", py); }
          }
        });
      };
      const onUp = () => {
        node.removeEventListener("pointermove", onMove);
        node.removeEventListener("pointerup", onUp);
        node.removeEventListener("pointercancel", onUp);
        if (!moved) {
          selection = selection && selection.type === "node" && selection.index === index ? null : { type: "node", index };
          notify();
        }
      };
      node.addEventListener("pointermove", onMove);
      node.addEventListener("pointerup", onUp);
      node.addEventListener("pointercancel", onUp);
    });
  });

  edges.forEach((edge) => {
    edge.addEventListener("click", () => {
      selection = { type: "edge", index: Number(edge.dataset.edge) };
      notify();
    });
  });
  svg.addEventListener("click", (event) => {
    if (event.target === svg) {
      selection = null;
      notify();
    }
  });
}

function inspectorPanel(colors) {
  const state = store.state;
  const last = state.last;
  if (selection && selection.type === "node") {
    const index = selection.index;
    const subject = state.subjects[index];
    const near = neighbours(index);
    const batchNames = subject.batches.map((id) => (state.batches.find((b) => b.id === id) || {}).name).filter(Boolean);
    return [
      h("p", { class: "eyebrow" }, "Subject"),
      h("h3", { class: "inspector-title" }, subject.code),
      h("p", {}, subject.name),
      h("dl", { class: "facts" },
        fact("Students", `${fmt.int(subject.students)}${subject.derived ? " (from batches)" : ""}`),
        fact("Conflicts (degree)", fmt.int(subject.degree)),
        fact("Batches", batchNames.length ? batchNames.join(", ") : "—"),
        colors ? fact("Slot", slotDot(colors[index])) : null),
      h("p", { class: "eyebrow" }, "Conflicts with"),
      near.length
        ? h("ul", { class: "tag-list" }, near.map((other) => h("li", {},
            h("button", { type: "button", class: "link-btn", onclick: () => { selection = { type: "node", index: other }; notify(); } }, subjectCode(other)),
            colors ? h("span", { class: "muted" }, ` ${slotName(colors[other])}`) : null)))
        : h("p", { class: "muted" }, "No conflicts — this exam can go anywhere."),
      button("Clear selection", { size: "sm", variant: "ghost", onClick: () => { selection = null; notify(); } }),
    ];
  }
  if (selection && selection.type === "edge") {
    const edge = state.edges[selection.index];
    return [
      h("p", { class: "eyebrow" }, "Conflict"),
      h("h3", { class: "inspector-title" }, `${subjectCode(edge.u)} ↔ ${subjectCode(edge.v)}`),
      h("dl", { class: "facts" },
        fact("Shared students", fmt.int(edge.weight)),
        fact("Caused by", edge.reasons.join(", ") || "—"),
        colors ? fact("Slots", [slotDot(colors[edge.u]), " vs ", slotDot(colors[edge.v])]) : null),
      colors ? (colors[edge.u] !== colors[edge.v]
        ? badge("Different slots — no clash", "good", "check")
        : badge("Same slot — clash", "bad", "alert")) : null,
      button("Clear selection", { size: "sm", variant: "ghost", onClick: () => { selection = null; notify(); } }),
    ];
  }

  const s = state.stats;
  return [
    h("p", { class: "eyebrow" }, "Graph"),
    h("dl", { class: "facts" },
      fact("Vertices · edges", `${s.vertices} · ${s.edges}`),
      fact("Density", s.density.toFixed(3)),
      fact("Max / avg degree", `${s.maxDegree} / ${s.avgDegree.toFixed(2)}`),
      fact("Components", s.components),
      fact("Isolated subjects", s.isolated),
      fact("Clique lower bound", `${s.clique.length} (${s.clique.map(subjectCode).join(", ") || "—"})`)),
    colors && last
      ? [h("p", { class: "eyebrow" }, `Colours: ${last.method}`),
        h("ul", { class: "legend" }, Array.from({ length: last.slotsUsed }, (_, c) => h("li", {}, slotDot(c, `Slot ${c + 1}`))))]
      : h("p", { class: "muted" }, "Generate a timetable to colour the nodes by slot."),
    h("p", { class: "muted small" }, h("span", { "aria-hidden": "true" }, icon("info", 13)), " Select a node or edge for details."),
  ];
}

function fact(label, value) {
  return h("div", { class: "fact" }, h("dt", {}, label), h("dd", {}, value));
}

function pairsTable() {
  const state = store.state;
  return h("details", { class: "card disclosure" },
    h("summary", {}, `Conflict pairs table (${state.edges.length})`),
    state.edges.length === 0
      ? h("p", { class: "muted" }, "No conflicts: every exam could share one slot.")
      : h("div", { class: "table-wrap" },
        h("table", { class: "table" },
          h("thead", {}, h("tr", {}, h("th", {}, "Pair"), h("th", {}, "Shared by"), h("th", { class: "num" }, "Students"))),
          h("tbody", {}, state.edges.map((edge) => h("tr", {},
            h("td", {}, h("code", {}, subjectCode(edge.u)), " ↔ ", h("code", {}, subjectCode(edge.v))),
            h("td", {}, edge.reasons.join(", ")),
            h("td", { class: "num" }, fmt.int(edge.weight))))))),
  );
}
