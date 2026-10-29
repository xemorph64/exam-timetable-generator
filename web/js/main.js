// App shell: navigation, engine status, console drawer, theme, re-rendering.
import { engine, store, subscribe, refresh, datasetName } from "./store.js";
import { setupConsole } from "./console.js";
import { h, icon, button, toast } from "./ui.js";
import * as overview from "./views/overview.js";
import * as timetable from "./views/timetable.js";
import * as graph from "./views/graph.js";
import * as lab from "./views/lab.js";
import * as data from "./views/data.js";

const ROUTES = [
  { id: "overview", label: "Overview", iconName: "overview", view: overview },
  { id: "timetable", label: "Timetable", iconName: "calendar", view: timetable },
  { id: "graph", label: "Conflict graph", iconName: "graph", view: graph },
  { id: "lab", label: "Algorithm lab", iconName: "lab", view: lab },
  { id: "data", label: "Data", iconName: "database", view: data },
];

const STATUS = {
  loading: { label: "Starting engine", tone: "busy" },
  menu: { label: "Engine ready", tone: "ok" },
  busy: { label: "Engine running", tone: "busy" },
  prompt: { label: "Waiting in console", tone: "warn" },
  exited: { label: "Engine stopped", tone: "bad" },
  crashed: { label: "Engine crashed", tone: "bad" },
  unresponsive: { label: "Not responding", tone: "bad" },
  stopped: { label: "Engine stopped", tone: "bad" },
};

const viewRoot = document.getElementById("view");
const nav = document.getElementById("nav");
const consoleEl = document.getElementById("console");
const consoleToggle = document.getElementById("console-toggle");
let current = null;

// ---- routing ----------------------------------------------------------------
function routeFromHash() {
  const id = location.hash.replace(/^#\/?/, "");
  return ROUTES.find((route) => route.id === id) || ROUTES[0];
}

export function navigate(id) {
  location.hash = `#/${id}`;
}

function buildNav() {
  nav.replaceChildren(
    ...ROUTES.map((route) =>
      h("a", { class: "nav-item", href: `#/${route.id}`, "data-route": route.id }, icon(route.iconName), h("span", {}, route.label)),
    ),
  );
}

function onRoute() {
  const route = routeFromHash();
  const changed = route !== current;
  current = route;
  for (const link of nav.querySelectorAll("[data-route]")) {
    const active = link.dataset.route === route.id;
    link.classList.toggle("active", active);
    if (active) link.setAttribute("aria-current", "page");
    else link.removeAttribute("aria-current");
  }
  document.getElementById("page-title").textContent = route.label;
  document.title = `${route.label} · Slotwise`;
  setNavOpen(false);
  render();
  if (changed) {
    viewRoot.scrollTop = 0;
    viewRoot.focus({ preventScroll: true });
  }
}

// ---- rendering (coalesced; keeps focus, caret and scroll position) ----------
let frame = 0;
let timer = 0;
function scheduleRender() {
  if (frame || timer) return;
  // rAF batches updates; the timer covers background tabs where rAF pauses.
  const run = () => {
    cancelAnimationFrame(frame);
    clearTimeout(timer);
    frame = 0;
    timer = 0;
    render();
  };
  frame = requestAnimationFrame(run);
  timer = setTimeout(run, 120);
}

function render() {
  const active = document.activeElement;
  const focusId = active && active.id && viewRoot.contains(active) ? active.id : null;
  const selection = focusId && typeof active.selectionStart === "number" ? [active.selectionStart, active.selectionEnd] : null;
  const scroll = viewRoot.scrollTop;

  const page = h("div", { class: `page page-${current.id}` });
  current.view.render(page, { navigate });
  viewRoot.replaceChildren(page);
  viewRoot.scrollTop = scroll;

  if (focusId) {
    const target = document.getElementById(focusId);
    if (target) {
      target.focus({ preventScroll: true });
      if (selection && typeof target.setSelectionRange === "function") {
        try { target.setSelectionRange(...selection); } catch { /* not a text input */ }
      }
    }
  }
  renderChrome();
}

function renderChrome() {
  const info = STATUS[engine.status] || STATUS.stopped;
  const pill = h("span", { class: `status-dot tone-${info.tone}`, "aria-hidden": "true" });
  document.getElementById("engine-pill").replaceChildren(
    pill,
    h("span", {}, info.label),
    h("small", {}, "C++17 · WebAssembly"),
  );
  document.getElementById("topbar-status").replaceChildren(h("span", { class: `status-dot tone-${info.tone}`, title: info.label }));
  document.getElementById("progress").classList.toggle("on", engine.status === "busy" || engine.status === "loading");

  const state = store.state;
  document.getElementById("dataset-chip").textContent = state
    ? `${datasetName()} · ${state.subjects.length} subjects`
    : "";

  const banner = document.getElementById("banner");
  if (engine.status === "prompt") {
    banner.replaceChildren(
      h("div", { class: "banner banner-warn", role: "status" }, icon("info"),
        h("span", {}, "The engine is waiting for an answer in the Engine console, so graphical actions are paused."),
        button("Open console", { size: "sm", onClick: () => setConsoleOpen(true) })),
    );
  } else if (["exited", "crashed", "unresponsive"].includes(engine.status)) {
    banner.replaceChildren(
      h("div", { class: "banner banner-bad", role: "alert" }, icon("alert"),
        h("span", {}, `${STATUS[engine.status].label}. Restarting starts a fresh engine with empty data.`),
        button("Restart engine", { size: "sm", variant: "primary", iconName: "refresh", onClick: () => engine.start() })),
    );
  } else {
    banner.replaceChildren();
  }
}

// ---- console drawer -----------------------------------------------------------
let consoleApi = null;
function setConsoleOpen(open) {
  consoleEl.hidden = !open;
  consoleToggle.setAttribute("aria-expanded", String(open));
  consoleToggle.classList.toggle("active", open);
  document.getElementById("app").classList.toggle("console-open", open);
  if (open) consoleApi.focus();
}

// ---- mobile navigation ---------------------------------------------------------
function setNavOpen(open) {
  document.getElementById("sidebar").classList.toggle("open", open);
  document.getElementById("scrim").hidden = !open;
  document.getElementById("menu-btn").setAttribute("aria-expanded", String(open));
}

// ---- theme -----------------------------------------------------------------------
function currentTheme() {
  const forced = document.documentElement.dataset.theme;
  if (forced) return forced;
  return matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}
function renderThemeButton() {
  const dark = currentTheme() === "dark";
  const toggle = document.getElementById("theme-toggle");
  toggle.replaceChildren(icon(dark ? "sun" : "moon"), h("span", {}, dark ? "Light" : "Dark"));
  toggle.setAttribute("aria-label", `Switch to ${dark ? "light" : "dark"} theme`);
}

// ---- start -------------------------------------------------------------------------
function init() {
  buildNav();
  consoleToggle.replaceChildren(icon("terminal"), h("span", {}, "Engine console"), h("kbd", {}, "Ctrl `"));
  consoleToggle.addEventListener("click", () => setConsoleOpen(consoleEl.hidden));
  consoleApi = setupConsole({ root: consoleEl, onToggle: setConsoleOpen });

  document.getElementById("repo-link").replaceChildren(icon("external"), h("span", {}, "Source"));
  document.getElementById("theme-toggle").addEventListener("click", () => {
    const next = currentTheme() === "dark" ? "light" : "dark";
    document.documentElement.dataset.theme = next;
    try { localStorage.setItem("slotwise-theme", next); } catch { /* storage unavailable */ }
    renderThemeButton();
  });
  renderThemeButton();
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", renderThemeButton);

  const menuBtn = document.getElementById("menu-btn");
  menuBtn.append(icon("menu", 20));
  menuBtn.addEventListener("click", () => setNavOpen(!document.getElementById("sidebar").classList.contains("open")));
  document.getElementById("scrim").addEventListener("click", () => setNavOpen(false));

  document.addEventListener("keydown", (event) => {
    if (event.ctrlKey && (event.key === "`" || event.code === "Backquote")) {
      event.preventDefault();
      setConsoleOpen(consoleEl.hidden);
    } else if (event.key === "Escape" && document.getElementById("sidebar").classList.contains("open")) {
      setNavOpen(false);
    }
  });

  subscribe(scheduleRender);
  engine.addEventListener("status", (event) => {
    scheduleRender();
    if (event.detail.status === "menu" && !store.state) {
      refresh().catch((error) => toast(error.message, "error"));
    }
  });
  engine.addEventListener("reset", () => {
    store.state = null;
    store.compare = null;
    store.lab = {};
    store.tests = null;
    store.sample = null;
    store.signature = "";
    scheduleRender();
  });
  // Anything typed in the console may have changed the data: re-read it.
  engine.addEventListener("console-idle", () => refresh().catch(() => {}));

  window.addEventListener("hashchange", onRoute);
  onRoute();
  window.slotwise = { engine, store };  // handy for debugging from devtools
  engine.start();
}

init();
