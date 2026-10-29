// Application state. Everything shown in the GUI comes from engine responses;
// the store only caches them and tells views when to re-render.
import { Engine, EngineError } from "./engine.js";
import { toast } from "./ui.js";

export const engine = new Engine();

export const store = {
  state: null,        // latest "state" response (subjects, batches, slots, edges, stats, last)
  signature: "",      // identity of the conflict graph; results are dropped when it changes
  compare: null,      // latest "compare" response for this graph
  lab: {},            // algorithm key -> result for this graph
  sample: null,       // { name, signature } of the last built-in sample loaded
  tests: null,        // latest self-test response
  busy: new Set(),    // keys of actions in flight
};

const listeners = new Set();
export function subscribe(listener) {
  listeners.add(listener);
  return () => listeners.delete(listener);
}
export function notify() {
  listeners.forEach((listener) => listener());
}

function signatureOf(state) {
  return JSON.stringify([state.subjects.map((s) => [s.id, s.code]), state.edges.map((e) => [e.u, e.v])]);
}

export async function refresh() {
  const state = await engine.call("state");
  const signature = signatureOf(state);
  if (signature !== store.signature) {
    store.signature = signature;
    store.compare = null;
    store.lab = {};
  }
  store.state = state;
  notify();
  return state;
}

// Runs an engine action with a busy flag and a visible error on failure.
// Returns the response, or null when it failed.
export async function act(key, task, { quiet = false } = {}) {
  store.busy.add(key);
  notify();
  try {
    return await task();
  } catch (error) {
    if (!quiet) toast(error instanceof EngineError || error instanceof Error ? error.message : String(error), "error");
    return null;
  } finally {
    store.busy.delete(key);
    notify();
  }
}

export const isBusy = (key) => store.busy.has(key);

// A data change followed by a fresh state read, so the GUI always shows what
// the engine actually holds.
export async function mutate(verb, payload = "") {
  const response = await engine.call(verb, payload);
  await refresh();
  return response;
}

export async function loadSample(letter) {
  const response = await engine.call("sample", letter.toLowerCase());
  await refresh();
  store.sample = { name: `Sample ${letter.toUpperCase()}`, description: response.description, signature: store.signature };
  notify();
  return response;
}

export function datasetName() {
  const state = store.state;
  if (!state || state.subjects.length === 0) return "Empty dataset";
  if (store.sample && store.sample.signature === store.signature) return store.sample.name;
  return "Custom dataset";
}

export const isSampleB = () => datasetName() === "Sample B";

// Index helpers over the current state.
export function subjectCode(index) {
  const subject = store.state && store.state.subjects[index];
  return subject ? subject.code : `#${index}`;
}

export function neighbours(index) {
  const list = [];
  for (const edge of store.state.edges) {
    if (edge.u === index) list.push(edge.v);
    else if (edge.v === index) list.push(edge.u);
  }
  return list;
}

export function availableSlots() {
  return store.state ? store.state.slots.filter((slot) => slot.available) : [];
}
