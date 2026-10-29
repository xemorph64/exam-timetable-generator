// Algorithm lab: every number on this page is a field of an engine response.
import { h, icon, button, badge, fmt, emptyState, sectionHeader, slotDot, draft, keepDraft, toast } from "../ui.js";
import { engine, store, act, isBusy, refresh, loadSample, isSampleB, subjectCode, notify } from "../store.js";
import { drawGraph, layoutFor } from "../graph.js";
import { validityBadge, optimalityBadge, effortText, slotGroups, knownOptimum } from "./common.js";

const RUNS = [
  { key: "input", name: "Greedy — input order", family: "Greedy", command: () => "greedy input" },
  { key: "welsh", name: "Greedy — Welsh–Powell", family: "Greedy", command: () => "greedy welsh" },
  { key: "dsatur", name: "Greedy — DSATUR", family: "Greedy", command: () => "greedy dsatur" },
  { key: "backtrack", name: "Backtracking", family: "Exact (decision)", command: () => `backtrack ${kValue()} ${limitArgs()}` },
  { key: "bnb", name: "Branch and bound", family: "Exact (optimisation)", command: () => `bnb ${limitArgs()}` },
];

function kValue() {
  return Number(draft("lab-k", String(Math.max(1, store.state.stats.clique.length))));
}
function limitArgs() {
  return `${Number(draft("lab-nodes", "5000000"))} ${Number(draft("lab-ms", "5000"))}`;
}
function validInputs() {
  const n = store.state.subjects.length;
  const k = kValue();
  const nodes = Number(draft("lab-nodes", "5000000"));
  const ms = Number(draft("lab-ms", "5000"));
  if (!Number.isInteger(k) || k < 1 || k > n) return `k must be a whole number from 1 to ${n}.`;
  if (!Number.isInteger(nodes) || nodes < 1 || nodes > 1e9) return "Node limit must be a whole number from 1 to 1,000,000,000.";
  if (!Number.isInteger(ms) || ms < 1 || ms > 600000) return "Time limit must be a whole number of ms from 1 to 600,000.";
  return null;
}

async function runOne(run) {
  const problem = validInputs();
  if (problem && (run.key === "backtrack" || run.key === "bnb")) {
    toast(problem, "error");
    return;
  }
  const response = await engine.call("run", run.command());
  store.lab = { ...store.lab, [run.key]: { ...response.result, k: run.key === "backtrack" ? kValue() : undefined } };
}

async function runAll() {
  const problem = validInputs();
  if (problem) {
    toast(problem, "error");
    return;
  }
  await act("lab-all", async () => {
    for (const run of RUNS) await runOne(run);
    await refresh();
  });
}

export function render(root, { navigate }) {
  const state = store.state;
  if (!state) return;
  root.append(
    h("header", { class: "page-head" },
      h("div", {},
        h("h2", { class: "page-title" }, "Algorithm lab"),
        h("p", { class: "lede" }, "Run the engine's Greedy, DSATUR, Backtracking and Branch-and-Bound implementations on the current data and compare what they actually return."),
      ),
      h("div", { class: "head-actions" },
        isSampleB() ? null : button("Load Sample B", {
          iconName: "database", busy: isBusy("lab-sampleB"),
          onClick: () => act("lab-sampleB", () => loadSample("B")),
          title: "The crown graph: input-order greedy needs 3 slots, 2 suffice",
        })),
    ),
  );
  if (state.subjects.length === 0) {
    root.append(h("div", { class: "card" }, emptyState({
      iconName: "lab", title: "Nothing to analyse",
      body: "Load Sample B to see greedy use 3 slots where 2 are enough.",
      actions: [button("Load Sample B", { variant: "primary", busy: isBusy("lab-sampleB"), onClick: () => act("lab-sampleB", () => loadSample("B")) }),
        button("Go to Data", { variant: "ghost", iconName: "arrow", onClick: () => navigate("data") })],
    })));
    return;
  }
  root.append(compareSection(), runsSection(), explainSection());
}

// ---- 1. greedy vs optimal ----------------------------------------------------
function compareSection() {
  const compare = store.compare;
  const section = h("section", { class: "card" });
  section.append(sectionHeader(
    "Greedy vs optimal",
    "Input-order greedy (the naive approach) against branch and bound — the same pair the CLI's menu 9 compares.",
    button(compare ? "Run again" : "Run comparison", {
      variant: compare ? "secondary" : "primary", iconName: "play", busy: isBusy("lab-compare"),
      onClick: () => act("lab-compare", async () => { store.compare = await engine.call("compare"); await refresh(); }),
    }),
  ));
  if (!compare) {
    section.append(h("p", { class: "muted" }, isSampleB()
      ? "Sample B is loaded. Input order A1, B1, A2, B2, A3, B3 tricks greedy into a third slot."
      : "Run the comparison to see both colourings side by side on the conflict graph."));
    return section;
  }

  const g = compare.greedy;
  const o = compare.optimal;
  const saved = g.slotsUsed - o.slotsUsed;
  const moved = new Set(compare.moved.map((m) => m.subject));
  const positions = layoutFor(store.state, store.signature);

  section.append(
    h("div", { class: `verdict ${saved > 0 ? "verdict-win" : "verdict-tie"}` },
      icon(saved > 0 ? "target" : "check", 18),
      h("p", {}, saved > 0
        ? [h("strong", {}, `Optimal search saved ${saved} ${saved === 1 ? "slot" : "slots"}`), ` — ${Math.round((100 * saved) / g.slotsUsed)}% fewer than greedy (${g.slotsUsed} → ${o.slotsUsed}).`]
        : [h("strong", {}, "Greedy already found the optimal answer"), " for this input."])),
    h("div", { class: "duel" },
      duelPanel("Greedy · input order", g, positions, moved, false),
      duelPanel("Branch and bound", o, positions, moved, true),
    ),
  );

  const notes = [];
  if (o.provenOptimal && o.nodesExplored === 0) {
    notes.push(`Proven without searching: the best greedy colouring (upper bound) already uses ${o.upperBound} slots, and the clique ${store.state.stats.clique.map(subjectCode).join("–")} forces at least ${o.lowerBound}. UB = LB, so ${o.slotsUsed} is optimal.`);
  } else if (o.provenOptimal) {
    notes.push(`Bounds before searching: LB = ${o.lowerBound} (clique), UB = ${o.upperBound} (best greedy). The search explored ${fmt.int(o.nodesExplored)} nodes and pruned ${fmt.int(o.nodesPruned)} to prove ${o.slotsUsed}.`);
  } else if (o.cutOff) {
    notes.push(`The search hit its safety limit: ${o.slotsUsed} is the best found, between LB = ${o.lowerBound} and UB = ${o.upperBound}, but optimality is not proven.`);
  }
  if (compare.moved.length) {
    section.append(
      h("div", { class: "moves" },
        h("p", { class: "eyebrow" }, `${fmt.plural(compare.moved.length, "subject")} moved between slots`),
        h("ul", { class: "move-list" }, compare.moved.map((m) =>
          h("li", {}, h("code", {}, subjectCode(m.subject)), slotDot(m.from), icon("arrow", 13), slotDot(m.to))))),
    );
  }
  notes.forEach((note) => section.append(h("p", { class: "note" }, icon("info", 14), note)));
  return section;
}

function duelPanel(title, result, positions, moved, isOptimal) {
  return h("div", { class: `duel-panel${isOptimal ? " optimal" : ""}` },
    h("div", { class: "duel-head" },
      h("h3", {}, title),
      h("span", { class: "duel-slots" }, h("strong", {}, result.slotsUsed), " slots")),
    drawGraph({ state: store.state, positions, colors: result.colors, mini: true, label: `${title} colouring using ${result.slotsUsed} slots` }),
    slotGroups(result.colors, { highlight: isOptimal ? moved : new Set() }),
    h("dl", { class: "metrics" },
      metricRow("Time", `${fmt.ms(result.timeMs)} ms`),
      metricRow("Search", effortText(result)),
      metricRow("Conflict-free", result.valid ? "Yes (verified)" : "No"),
      metricRow("Optimality proven", result.provenOptimal ? "Yes" : "No")),
  );
}

function metricRow(label, value) {
  return h("div", {}, h("dt", {}, label), h("dd", {}, value));
}

// ---- 2. all algorithms ----------------------------------------------------------
function runsSection() {
  const optimum = knownOptimum();
  const lb = store.state.stats.clique.length;
  const results = RUNS.map((run) => store.lab[run.key]).filter(Boolean);
  const scaleMax = Math.max(lb, ...results.map((r) => r.slotsUsed || 0), 1);

  const field = (id, label, value, attrs = {}) => h("label", { class: "field" }, h("span", {}, label),
    keepDraft(h("input", { id, class: "input input-sm", type: "number", value: draft(id, value), ...attrs })));

  const section = h("section", { class: "card" });
  section.append(sectionHeader(
    "Every algorithm on this dataset",
    "Each row runs the C++ implementation in the WebAssembly engine. Bars show slots used; the dashed line is the clique lower bound.",
    button("Run all", { variant: "primary", iconName: "play", busy: isBusy("lab-all"), onClick: runAll }),
  ));
  section.append(h("div", { class: "lab-controls" },
    field("lab-k", "Backtracking k", String(Math.max(1, lb)), { min: 1, max: store.state.subjects.length }),
    field("lab-nodes", "Node limit", "5000000", { min: 1 }),
    field("lab-ms", "Time limit (ms)", "5000", { min: 1 }),
    optimum && optimum > 1
      ? button(`Try k = ${optimum - 1}`, { size: "sm", variant: "ghost", title: "One below the proven optimum: backtracking must prove it impossible", onClick: () => {
          const input = document.getElementById("lab-k");
          if (input) { input.value = String(optimum - 1); input.dispatchEvent(new Event("input")); }
          notify();
        } })
      : null,
  ));

  const rows = RUNS.map((run) => {
    const result = store.lab[run.key];
    const busy = isBusy(`lab-${run.key}`);
    return h("tr", {},
      h("th", { scope: "row" }, h("div", { class: "alg-name" }, run.name), h("small", { class: "muted" }, run.family)),
      h("td", { class: "bar-cell" }, result ? slotBar(result, scaleMax, lb, optimum) : h("span", { class: "muted" }, "—")),
      h("td", {}, result ? outcome(run, result, optimum) : h("span", { class: "muted" }, "Not run")),
      h("td", { class: "muted small" }, result ? effortText(result) : ""),
      h("td", { class: "num" }, result ? `${fmt.ms(result.timeMs)} ms` : ""),
      h("td", { class: "num" }, button("Run", {
        size: "sm", iconName: "play", busy, disabled: isBusy("lab-all"),
        onClick: () => act(`lab-${run.key}`, async () => { await runOne(run); await refresh(); }),
      })),
    );
  });
  section.append(
    h("div", { class: "table-wrap" },
      h("table", { class: "table runs" },
        h("thead", {}, h("tr", {},
          h("th", { scope: "col" }, "Algorithm"), h("th", { scope: "col" }, "Slots used"), h("th", { scope: "col" }, "Outcome"),
          h("th", { scope: "col" }, "Search effort"), h("th", { scope: "col", class: "num" }, "Time"), h("th", { scope: "col" }, h("span", { class: "sr-only" }, "Actions")))),
        h("tbody", {}, rows))),
    h("p", { class: "muted small" }, "Times are measured inside the engine with std::chrono. Browsers coarsen timers (often to 0.1 ms) for security, so very fast runs read as 0.100 ms or <0.001 ms."),
  );
  return section;
}

function slotBar(result, scaleMax, lb, optimum) {
  if (!result.success) {
    return h("span", { class: "bar-none" }, "No colouring");
  }
  const pct = (v) => `${(v / scaleMax) * 100}%`;
  return h("div", { class: "slotbar", title: `${result.slotsUsed} slots · clique bound ${lb}${optimum ? ` · proven optimum ${optimum}` : ""}` },
    h("div", { class: "slotbar-track" },
      h("div", { class: "slotbar-fill", style: { width: pct(result.slotsUsed) } }),
      h("div", { class: "slotbar-lb", style: { left: pct(lb) }, "aria-hidden": "true" })),
    h("strong", {}, result.slotsUsed));
}

function outcome(run, result, optimum) {
  if (run.key === "backtrack") {
    if (result.success) return badge(`Fits in k = ${result.k}`, "good", "check");
    if (result.cutOff) return badge(`Undecided for k = ${result.k} (limit hit)`, "warn", "alert");
    return badge(`Impossible with k = ${result.k} (proved)`, "bad", "x");
  }
  const parts = [validityBadge(result)];
  if (run.key === "bnb") {
    parts.push(optimalityBadge(result));
  } else if (optimum !== null) {
    parts.push(result.slotsUsed === optimum ? badge("Matches proven optimum", "accent", "target")
      : badge(`+${result.slotsUsed - optimum} over optimum`, "warn"));
  }
  return h("div", { class: "badges" }, parts);
}

// ---- 3. explanations ------------------------------------------------------------
function explainSection() {
  const card = (title, time, body) => h("article", { class: "explain" },
    h("h3", {}, title), h("p", { class: "complexity" }, time), h("p", {}, body));
  return h("section", { class: "card" },
    sectionHeader("How the engine solves it", "Graph colouring is NP-hard: fast heuristics give upper bounds, exact searches prove the minimum."),
    h("div", { class: "explain-grid" },
      card("Greedy (input / Welsh–Powell)", "O(V + E) · space O(V)",
        "Visit subjects in a fixed order and give each the smallest slot not used by an already-placed neighbour. Fast, never revises a choice — so the order decides the quality."),
      card("DSATUR", "O(V² + E log V) · space O(V + E)",
        "Always colour the subject with the most distinct slots among its neighbours (saturation), ties by degree. Exact on bipartite graphs such as Sample B."),
      card("Backtracking", "O(kᵛ) worst case · space O(V)",
        "Decides whether k slots suffice: try each slot, check safety with the adjacency matrix, undo on a dead end. Proving 'impossible' can explode on hard inputs."),
      card("Branch and bound", "O(UBᵛ) worst case · space O(V·UB)",
        "Minimises slots between bounds — UB from the best greedy, LB from a clique — with bound pruning, symmetry breaking and forward checking; stops as soon as UB = LB."),
    ),
  );
}
