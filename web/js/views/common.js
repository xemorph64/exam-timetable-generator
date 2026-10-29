// Pieces shared by several views. Wording is careful to claim only what the
// engine actually reported (validator result, provenOptimal, cutOff, ...).
import { h, badge, fmt, slotDot } from "../ui.js";
import { store, subjectCode } from "../store.js";

export function validityBadge(result) {
  if (!result.colors || result.colors.length === 0) return badge("No timetable", "neutral");
  return result.valid
    ? badge("Verified conflict-free", "good", "shield")
    : badge(`${fmt.plural(result.clashes.length, "clash", "clashes")} found`, "bad", "alert");
}

export function optimalityBadge(result) {
  if (result.provenOptimal) return badge("Proven optimal", "accent", "target");
  if (result.cutOff) return badge("Search cut off — may not be optimal", "warn", "alert");
  return null;
}

export function feasibilityBadge(result) {
  const schedule = result.schedule;
  if (!schedule) return null;
  if (schedule.slotsAvailable === 0) return badge("No available slots", "warn", "alert");
  return schedule.feasible
    ? badge(`Fits ${schedule.slotsNeeded} of ${schedule.slotsAvailable} slots`, "neutral", "calendar")
    : badge(`Needs ${schedule.slotsNeeded} slots, ${schedule.slotsAvailable} available`, "bad", "alert");
}

export function metric(label, value, hint) {
  return h("div", { class: "metric" }, h("dt", {}, label), h("dd", {}, value), hint ? h("small", {}, hint) : null);
}

// Search effort exactly as reported by the engine.
export function effortText(result) {
  if (/^Greedy/.test(result.method)) return "Single pass, no search";
  if (/^Backtracking/.test(result.method)) {
    return `${fmt.int(result.nodesExplored)} nodes · ${fmt.int(result.backtracks)} backtracks`;
  }
  if (result.nodesExplored === 0 && result.provenOptimal) return `0 nodes · bounds met (LB = UB = ${result.slotsUsed})`;
  return `${fmt.int(result.nodesExplored)} nodes · ${fmt.int(result.nodesPruned)} pruned`;
}

// Subjects grouped by colour: [[indices of slot 0], [slot 1], ...]
export function groupByColor(colors) {
  const groups = [];
  colors.forEach((color, index) => {
    if (color < 0) return;
    while (groups.length <= color) groups.push([]);
    groups[color].push(index);
  });
  return groups;
}

export function slotGroups(colors, { highlight = new Set() } = {}) {
  return h(
    "ol",
    { class: "slot-groups" },
    groupByColor(colors).map((members, color) =>
      h(
        "li",
        {},
        slotDot(color),
        h("span", { class: "group-codes" },
          members.map((index) => h("code", { class: highlight.has(index) ? "moved" : null }, subjectCode(index)))),
      ),
    ),
  );
}

export function knownOptimum() {
  const candidates = [store.lab.bnb, store.compare && store.compare.optimal].filter(Boolean);
  const proven = candidates.find((result) => result.provenOptimal);
  return proven ? proven.slotsUsed : null;
}
