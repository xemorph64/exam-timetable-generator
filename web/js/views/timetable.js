// Timetable: the engine's last verified colouring mapped onto the real,
// available time slots (colour c -> c-th available slot, as in the C++ code).
import { h, icon, button, badge, fmt, emptyState, slotColor, slotName, draft, keepDraft } from "../ui.js";
import { engine, store, act, isBusy, refresh, notify, neighbours, subjectCode } from "../store.js";
import { validityBadge, optimalityBadge, feasibilityBadge, effortText } from "./common.js";

const ALGORITHMS = [
  { id: "input", label: "Greedy", hint: "Input order, one pass", command: "greedy input" },
  { id: "welsh", label: "Welsh–Powell", hint: "Highest degree first", command: "greedy welsh" },
  { id: "dsatur", label: "DSATUR", hint: "Most constrained first", command: "greedy dsatur" },
  { id: "backtrack", label: "Backtracking", hint: "Fit into k slots", command: "backtrack" },
  { id: "bnb", label: "Branch & bound", hint: "Fewest slots, proven", command: "bnb" },
];
let chosen = "bnb";
let attemptNote = null;   // message about the last generate attempt that produced no timetable

async function generate() {
  const algorithm = ALGORITHMS.find((a) => a.id === chosen);
  let command = algorithm.command;
  if (chosen === "backtrack") {
    const k = Number(draft("tt-k", String(defaultK())));
    if (!Number.isInteger(k) || k < 1 || k > store.state.subjects.length) {
      attemptNote = { kind: "bad", text: `k must be a whole number from 1 to ${store.state.subjects.length}.` };
      notify();
      return;
    }
    command = `backtrack ${k}`;
  }
  await act("generate", async () => {
    const response = await engine.call("run", command);
    const result = response.result;
    if (!result.success) {
      const k = command.split(" ")[1];
      attemptNote = result.cutOff
        ? { kind: "warn", text: `Backtracking stopped at its safety limit after ${fmt.int(result.nodesExplored)} nodes, so it could not decide whether ${k} slots are enough. The timetable below is unchanged.` }
        : { kind: "bad", text: `Proved impossible: the exams cannot fit into ${k} slots (${fmt.int(result.nodesExplored)} nodes, ${fmt.int(result.backtracks)} backtracks). The timetable below is unchanged.` };
    } else {
      attemptNote = null;
    }
    await refresh();
  });
}

function defaultK() {
  return Math.max(1, store.state ? store.state.stats.clique.length : 1);
}

export function render(root, { navigate }) {
  const state = store.state;
  if (!state) return;
  root.append(
    h("header", { class: "page-head" },
      h("div", {},
        h("h2", { class: "page-title" }, "Timetable"),
        h("p", { class: "lede" }, "Exams placed into the available time slots. Hover or focus an exam to see every subject it clashes with — none of them share its slot."),
      ),
    ),
  );
  if (state.subjects.length === 0) {
    root.append(h("div", { class: "card" }, emptyState({
      iconName: "calendar", title: "Nothing to schedule",
      body: "Add subjects and batches, or load a sample dataset first.",
      actions: [button("Go to Data", { variant: "primary", iconName: "arrow", onClick: () => navigate("data") })],
    })));
    return;
  }
  root.append(generator());
  if (attemptNote) root.append(h("div", { class: `callout callout-${attemptNote.kind}`, role: "status" }, icon("info"), h("p", {}, attemptNote.text)));

  const last = state.last;
  if (!last) {
    root.append(h("div", { class: "card" }, emptyState({
      iconName: "calendar", title: "No timetable yet",
      body: "Pick an algorithm above and press Generate. The engine verifies every edge before anything is shown.",
    })));
    return;
  }
  if (state.slots.length === 0) {
    root.append(h("div", { class: "callout callout-warn" }, icon("alert"),
      h("p", {}, `The engine needs ${last.slotsUsed} slots for this colouring, but no time slots are defined yet. `),
      button("Add time slots", { size: "sm", onClick: () => navigate("data") })));
  }
  root.append(summary(last), board(last));
}

function generator() {
  const choice = (algorithm) =>
    h("label", { class: `seg${chosen === algorithm.id ? " on" : ""}` },
      h("input", {
        type: "radio", name: "tt-alg", value: algorithm.id, checked: chosen === algorithm.id,
        id: `tt-alg-${algorithm.id}`,
        onchange: () => { chosen = algorithm.id; attemptNote = null; notify(); },
      }),
      h("span", { class: "seg-label" }, algorithm.label),
      h("small", {}, algorithm.hint));
  const kInput = keepDraft(h("input", {
    id: "tt-k", type: "number", min: 1, max: store.state.subjects.length, value: draft("tt-k", String(defaultK())),
    class: "input input-sm", "aria-label": "Slot limit k",
  }));
  return h("section", { class: "card toolbar-card" },
    h("fieldset", { class: "segmented" }, h("legend", { class: "sr-only" }, "Algorithm"), ALGORITHMS.map(choice)),
    h("div", { class: "toolbar-actions" },
      chosen === "backtrack" ? h("label", { class: "inline-field" }, h("span", {}, "k ="), kInput) : null,
      button("Generate", { variant: "primary", iconName: "play", busy: isBusy("generate"), onClick: generate }),
    ),
  );
}

function summary(result) {
  return h("section", { class: "result-bar" },
    h("div", { class: "result-main" },
      h("span", { class: "result-method" }, result.method),
      h("span", { class: "result-slots" }, h("strong", {}, result.slotsUsed), result.slotsUsed === 1 ? " slot" : " slots"),
    ),
    h("div", { class: "badges" }, validityBadge(result), optimalityBadge(result), feasibilityBadge(result)),
    h("div", { class: "result-meta" },
      h("span", { title: "Measured inside the WebAssembly engine" }, `${fmt.ms(result.timeMs)} ms`),
      h("span", {}, effortText(result)),
    ),
  );
}

// Labels like "Day 2 - 09:00 AM" become a day x session grid; anything else a list.
function parseGrid(slots) {
  const parsed = slots.map((slot) => {
    const match = /^Day\s*(\d+)\s*[-–—:]\s*(.+)$/i.exec(slot.label);
    return match ? { slot, day: Number(match[1]), time: match[2].trim() } : null;
  });
  if (parsed.length === 0 || parsed.some((p) => !p)) return null;
  const days = [...new Set(parsed.map((p) => p.day))].sort((a, b) => a - b);
  const times = [...new Set(parsed.map((p) => p.time))];
  if (days.length * times.length > slots.length * 3) return null;  // too sparse to read as a grid
  return { days, times, cell: (day, time) => parsed.find((p) => p.day === day && p.time === time) };
}

function board(result) {
  const state = store.state;
  const schedule = result.schedule;
  const colorOfSlot = new Map();
  (schedule ? schedule.slotIds : []).forEach((id, color) => colorOfSlot.set(id, color));
  const placement = schedule && !schedule.feasible ? schedule.partialColors : result.colors;
  const bySlotColor = new Map();
  placement.forEach((color, index) => {
    if (color < 0) return;
    if (!bySlotColor.has(color)) bySlotColor.set(color, []);
    bySlotColor.get(color).push(index);
  });

  const inspector = h("p", { class: "inspector", "aria-live": "polite" }, "Hover or focus an exam to inspect its conflicts.");
  const container = h("div", { class: "board-wrap" });

  const chip = (index, color) => {
    const subject = state.subjects[index];
    const el = h("button", {
      type: "button", class: "exam", "data-index": index, style: { "--slot": slotColor(color) },
      "aria-label": `${subject.code} ${subject.name}, ${subject.students} students`,
    },
    h("span", { class: "exam-code" }, subject.code),
    h("span", { class: "exam-name" }, subject.name),
    h("span", { class: "exam-meta" }, `${fmt.int(subject.students)} students`));
    const show = () => highlight(container, inspector, index, placement);
    el.addEventListener("mouseenter", show);
    el.addEventListener("focus", show);
    el.addEventListener("mouseleave", () => clearHighlight(container, inspector));
    el.addEventListener("blur", () => clearHighlight(container, inspector));
    return el;
  };

  const slotCell = (slot) => {
    const color = colorOfSlot.has(slot.id) ? colorOfSlot.get(slot.id) : -1;
    const exams = color >= 0 ? bySlotColor.get(color) || [] : [];
    return h("div", { class: `slot-cell${slot.available ? "" : " disabled"}${exams.length ? "" : " free"}`, style: color >= 0 ? { "--slot": slotColor(color) } : null },
      h("div", { class: "slot-head" },
        h("span", { class: "slot-label" }, slot.label),
        color >= 0 ? h("span", { class: "slot-tag" }, h("i", { "aria-hidden": "true" }), slotName(color)) : null),
      slot.available
        ? exams.length ? h("div", { class: "exam-list" }, exams.map((index) => chip(index, color))) : h("span", { class: "slot-empty" }, "Free")
        : h("span", { class: "slot-empty" }, "Unavailable"),
    );
  };

  if (state.slots.length > 0) {
    const grid = parseGrid(state.slots);
    if (grid) {
      container.append(h("div", { class: "board grid", style: { "--cols": grid.days.length } },
        h("div", { class: "board-corner" }),
        grid.days.map((day) => h("div", { class: "board-day" }, `Day ${day}`)),
        grid.times.map((time) => [
          h("div", { class: "board-time" }, time),
          grid.days.map((day) => {
            const cell = grid.cell(day, time);
            return cell ? slotCell(cell.slot) : h("div", { class: "slot-cell void" });
          }),
        ]),
      ));
    } else {
      container.append(h("div", { class: "board list" }, state.slots.map(slotCell)));
    }
  }

  const blocks = [];
  if (schedule && !schedule.feasible && schedule.slotsAvailable > 0) {
    const refitted = schedule.outsideSlots.length - schedule.unplaced.length;
    blocks.push(h("div", { class: "callout callout-bad", role: "status" }, icon("alert"),
      h("div", {},
        h("p", {}, h("strong", {}, "Infeasible with the available slots. "),
          `This colouring needs ${schedule.slotsNeeded} slots but only ${schedule.slotsAvailable} are available (short by ${schedule.slotsNeeded - schedule.slotsAvailable}). `,
          `Showing a partial timetable: ${fmt.plural(schedule.outsideSlots.length, "subject")} fell outside the slots and the engine re-fitted ${refitted} without clashes.`),
        schedule.unplaced.length
          ? h("div", { class: "unplaced" }, h("span", {}, "Cannot be placed:"), schedule.unplaced.map((index) => h("code", {}, subjectCode(index))))
          : h("p", {}, "Every subject was re-fitted — a better colouring exists; try Branch & bound."),
      )));
  }
  return h("section", { class: "card board-card" }, blocks, container, state.slots.length ? inspector : null);
}

function highlight(container, inspector, index, placement) {
  const conflicts = neighbours(index);
  const set = new Set(conflicts);
  container.classList.add("inspecting");
  for (const el of container.querySelectorAll(".exam")) {
    const i = Number(el.dataset.index);
    el.classList.toggle("focus", i === index);
    el.classList.toggle("conflict", set.has(i));
  }
  const subject = store.state.subjects[index];
  const sameSlot = conflicts.filter((other) => placement[other] >= 0 && placement[other] === placement[index]);
  inspector.replaceChildren(
    h("strong", {}, subject.code), ` ${subject.name} — `,
    ...(conflicts.length
      ? [`conflicts with ${conflicts.map(subjectCode).join(", ")}. `,
        sameSlot.length ? h("span", { class: "bad-text" }, `${sameSlot.map(subjectCode).join(", ")} share its slot!`) : h("span", { class: "good-text" }, "None of them share its slot.")]
      : ["no conflicts; it could go in any slot."]),
  );
}

function clearHighlight(container, inspector) {
  container.classList.remove("inspecting");
  for (const el of container.querySelectorAll(".exam")) el.classList.remove("focus", "conflict");
  inspector.textContent = "Hover or focus an exam to inspect its conflicts.";
}
