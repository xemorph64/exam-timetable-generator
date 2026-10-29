// Overview: engine status, real dataset statistics, primary actions.
import { h, icon, button, badge, fmt, emptyState, sectionHeader, toast, slotColor } from "../ui.js";
import { engine, store, act, isBusy, loadSample, refresh, datasetName, isSampleB } from "../store.js";
import { validityBadge, optimalityBadge, feasibilityBadge, groupByColor } from "./common.js";

async function runDemo(navigate) {
  const ok = await act("demo", async () => {
    await loadSample("B");
    store.compare = await engine.call("compare");
    await refresh();
    return true;
  });
  if (ok) {
    toast("Sample B loaded and compared — greedy vs optimal is ready.", "success");
    navigate("lab");
  }
}

export function render(root, { navigate }) {
  const state = store.state;
  if (!state) {
    root.append(loadingState());
    return;
  }

  root.append(
    h("header", { class: "page-head" },
      h("div", {},
        h("p", { class: "eyebrow" }, datasetName()),
        h("h2", { class: "page-title" }, "Schedule every exam without a single clash"),
        h("p", { class: "lede" },
          "Subjects become vertices, shared student batches become edges, and time slots become colours. ",
          "Slotwise asks the C++ engine for the fewest colours that keep every edge's ends apart."),
      ),
      h("div", { class: "head-actions" },
        button(isSampleB() ? "Re-run Sample B demo" : "Run the Sample B demo", {
          variant: "primary", iconName: "play", busy: isBusy("demo"), onClick: () => runDemo(navigate),
          title: "Loads Sample B and compares input-order greedy with branch and bound",
        }),
      ),
    ),
  );

  if (state.subjects.length === 0) {
    root.append(
      h("div", { class: "card" },
        emptyState({
          iconName: "database",
          title: "No exams loaded yet",
          body: "Load a built-in dataset to explore, or add your own subjects, batches and time slots.",
          actions: [
            button("Load Sample A — small college", { onClick: () => act("sampleA", () => loadSample("A")), busy: isBusy("sampleA") }),
            button("Load Sample C — 30 subjects", { onClick: () => act("sampleC", () => loadSample("C")), busy: isBusy("sampleC") }),
            button("Add data manually", { variant: "ghost", iconName: "arrow", onClick: () => navigate("data") }),
          ],
        }),
      ),
      modelExplainer(),
    );
    root.append(engineCard());
    return;
  }

  const s = state.stats;
  const available = state.slots.filter((slot) => slot.available).length;
  root.append(
    h("dl", { class: "kpis" },
      kpi("Subjects", fmt.int(s.vertices), "vertices"),
      kpi("Batches", fmt.int(state.batches.length), `${fmt.int(s.totalStudents)} students`),
      kpi("Conflicts", fmt.int(s.edges), "edges"),
      kpi("Slots", `${available}/${state.slots.length}`, "available / total"),
      kpi("Clique bound", fmt.int(s.clique.length), "minimum slots needed"),
      kpi("Density", s.density.toFixed(2), `${s.components} ${s.components === 1 ? "component" : "components"}`),
    ),
  );

  root.append(h("div", { class: "grid-2" }, latestCard(navigate), compareCard(navigate)));
  root.append(engineCard());
}

function kpi(label, value, note) {
  return h("div", { class: "kpi" }, h("dt", {}, label), h("dd", {}, value), h("small", {}, note));
}

function loadingState() {
  const status = engine.status;
  const failed = ["exited", "crashed", "unresponsive"].includes(status);
  return h("div", { class: "card" },
    emptyState({
      iconName: failed ? "alert" : "bolt",
      title: failed ? "The engine is not running" : "Starting the C++ engine…",
      body: failed
        ? "Restart it to continue. A fresh engine starts with empty data."
        : "Loading the WebAssembly build of the scheduler. This takes a moment on first visit.",
      actions: failed ? [button("Restart engine", { variant: "primary", iconName: "refresh", onClick: () => engine.start() })] : [],
    }),
  );
}

function modelExplainer() {
  const step = (n, title, body) => h("li", {}, h("span", { class: "step-n" }, n), h("div", {}, h("strong", {}, title), h("p", {}, body)));
  return h("section", { class: "card" },
    sectionHeader("How Slotwise models the problem"),
    h("ol", { class: "steps" },
      step("1", "Subjects → vertices", "Each exam is a vertex of the conflict graph."),
      step("2", "Shared batches → edges", "Two exams sat by the same batch can never share a slot."),
      step("3", "Slots → colours", "A proper colouring is a clash-free timetable; fewer colours, fewer slots."),
    ),
  );
}

function latestCard(navigate) {
  const last = store.state.last;
  const card = h("section", { class: "card" });
  card.append(sectionHeader("Latest timetable", last ? last.method : "Nothing generated yet",
    last ? button("Open timetable", { size: "sm", iconName: "arrow", onClick: () => navigate("timetable") }) : null));
  if (!last) {
    card.append(
      h("p", { class: "muted" }, "Generate a timetable with any algorithm; the engine verifies every result."),
      h("div", { class: "row" },
        button("Optimal (branch and bound)", {
          variant: "primary", iconName: "target", busy: isBusy("ov-bnb"),
          onClick: () => act("ov-bnb", async () => { await engine.call("run", "bnb"); await refresh(); navigate("timetable"); }),
        }),
        button("Quick (DSATUR)", {
          iconName: "bolt", busy: isBusy("ov-dsatur"),
          onClick: () => act("ov-dsatur", async () => { await engine.call("run", "greedy dsatur"); await refresh(); navigate("timetable"); }),
        }),
      ),
    );
    return card;
  }
  const groups = groupByColor(last.colors);
  const largest = Math.max(1, ...groups.map((g) => g.length));
  card.append(
    h("div", { class: "big-number" }, h("strong", {}, last.slotsUsed), h("span", {}, last.slotsUsed === 1 ? "slot used" : "slots used")),
    h("div", { class: "badges" }, validityBadge(last), optimalityBadge(last), feasibilityBadge(last)),
    h("ul", { class: "slot-bars", "aria-label": "Exams per slot" },
      groups.map((members, color) =>
        h("li", { title: `Slot ${color + 1}: ${members.length} exams` },
          h("span", { class: "slot-bars-label" }, `S${color + 1}`),
          h("span", { class: "slot-bars-track" },
            h("span", { class: "slot-bars-fill", style: { width: `${(members.length / largest) * 100}%`, background: slotColor(color) } })),
          h("span", { class: "slot-bars-value" }, members.length),
        ),
      ),
    ),
  );
  return card;
}

function compareCard(navigate) {
  const compare = store.compare;
  const card = h("section", { class: "card card-accent" });
  card.append(sectionHeader("Greedy vs optimal", "Input-order greedy against branch and bound on this dataset"));
  if (!compare) {
    card.append(
      h("p", { class: "muted" }, isSampleB()
        ? "Sample B is a crown graph: greedy in input order needs 3 slots, but 2 suffice. See the engine prove it."
        : "Run both algorithms on the current data and see how many slots the optimal search saves."),
      h("div", { class: "row" },
        button("Compare now", {
          variant: "primary", iconName: "play", busy: isBusy("ov-compare"),
          onClick: () => act("ov-compare", async () => { store.compare = await engine.call("compare"); await refresh(); navigate("lab"); }),
        }),
      ),
    );
    return card;
  }
  const g = compare.greedy;
  const o = compare.optimal;
  const saved = g.slotsUsed - o.slotsUsed;
  card.append(
    h("div", { class: "versus" },
      h("div", {}, h("span", { class: "muted" }, "Greedy"), h("strong", {}, g.slotsUsed), h("small", {}, "slots")),
      h("span", { class: "versus-arrow", "aria-hidden": "true" }, icon("arrow", 18)),
      h("div", {}, h("span", { class: "muted" }, "Optimal"), h("strong", { class: "accent" }, o.slotsUsed), h("small", {}, "slots")),
    ),
    h("p", {}, saved > 0
      ? `Branch and bound saved ${saved} ${saved === 1 ? "slot" : "slots"} (${Math.round((100 * saved) / g.slotsUsed)}% fewer).`
      : "Greedy already found the optimal answer for this input."),
    h("div", { class: "badges" }, optimalityBadge(o), validityBadge(o)),
    h("div", { class: "row" }, button("Open in Algorithm lab", { size: "sm", iconName: "arrow", onClick: () => navigate("lab") })),
  );
  return card;
}

function engineCard() {
  const tests = store.tests;
  const card = h("section", { class: "card" });
  card.append(
    sectionHeader("Engine", "The original C++17 scheduler compiled to WebAssembly, running in a background worker.",
      button(tests ? "Run self-tests again" : "Run self-tests", {
        size: "sm", iconName: "shield", busy: isBusy("tests"),
        onClick: () => act("tests", async () => { store.tests = await engine.call("tests"); }),
      })),
  );
  if (tests) {
    const passed = tests.lines.filter((line) => line.includes("[PASS]")).length;
    const failed = tests.lines.filter((line) => line.includes("[FAIL]")).length;
    card.append(
      h("div", { class: "badges" },
        tests.failures === 0 ? badge(`${passed} of ${passed + failed} checks passed`, "good", "check") : badge(`${tests.failures} failed`, "bad", "alert")),
      h("details", { class: "disclosure" },
        h("summary", {}, "Show test output"),
        h("pre", { class: "mono-block" }, tests.lines.join("\n"))),
    );
  } else {
    card.append(h("p", { class: "muted" }, "Runs the project's own test-suite (K5, C5, K3,3, crown, wheel, data-layer checks) inside the browser engine."));
  }
  return card;
}
