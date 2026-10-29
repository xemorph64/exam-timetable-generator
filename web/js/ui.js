// Small DOM helpers shared by every view (no framework).

export function h(tag, attrs = {}, ...children) {
  const element = document.createElement(tag);
  for (const [key, value] of Object.entries(attrs || {})) {
    if (value === undefined || value === null || value === false) continue;
    if (key === "class") element.className = value;
    else if (key === "text") element.textContent = value;
    else if (key === "style" && typeof value === "object") {
      // setProperty also handles custom properties such as --slot.
      for (const [prop, v] of Object.entries(value)) element.style.setProperty(prop.replace(/[A-Z]/g, (c) => `-${c.toLowerCase()}`), v);
    }
    else if (key.startsWith("on") && typeof value === "function") element.addEventListener(key.slice(2), value);
    else if (key === "dataset") Object.assign(element.dataset, value);
    else if (value === true) element.setAttribute(key, "");
    else element.setAttribute(key, value);
  }
  append(element, children);
  return element;
}

function append(parent, children) {
  for (const child of children.flat(Infinity)) {
    if (child === null || child === undefined || child === false) continue;
    parent.append(child instanceof Node ? child : document.createTextNode(String(child)));
  }
}

// ---- icons (24px stroke icons, drawn for this project) ----------------------
const ICONS = {
  overview: '<rect x="3" y="3" width="7" height="9" rx="1.5"/><rect x="14" y="3" width="7" height="5" rx="1.5"/><rect x="14" y="12" width="7" height="9" rx="1.5"/><rect x="3" y="16" width="7" height="5" rx="1.5"/>',
  calendar: '<rect x="3" y="4.5" width="18" height="16.5" rx="2"/><path d="M8 2.5v4M16 2.5v4M3 10h18"/>',
  graph: '<circle cx="5.5" cy="12" r="2.5"/><circle cx="18" cy="5.5" r="2.5"/><circle cx="18" cy="18.5" r="2.5"/><path d="M7.8 10.9 15.7 6.6M7.8 13.1l7.9 4.3M18 8v8"/>',
  lab: '<path d="M9 3h6M10 3v6.2L4.6 18.3A1.8 1.8 0 0 0 6.2 21h11.6a1.8 1.8 0 0 0 1.6-2.7L14 9.2V3"/><path d="M7 15.5h10"/>',
  database: '<ellipse cx="12" cy="5.5" rx="8" ry="3"/><path d="M4 5.5v13c0 1.7 3.6 3 8 3s8-1.3 8-3v-13"/><path d="M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3"/>',
  terminal: '<rect x="2.5" y="4" width="19" height="16" rx="2"/><path d="m7 9 3 3-3 3M13 15h4"/>',
  sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
  moon: '<path d="M20.5 13.2A8.5 8.5 0 1 1 10.8 3.5a6.6 6.6 0 0 0 9.7 9.7z"/>',
  external: '<path d="M14 4h6v6M10 14 20 4M18 13.5V19a1.5 1.5 0 0 1-1.5 1.5h-11A1.5 1.5 0 0 1 4 19V7.5A1.5 1.5 0 0 1 5.5 6H11"/>',
  play: '<path d="M7 4.8v14.4a.8.8 0 0 0 1.2.7l11.3-7.2a.8.8 0 0 0 0-1.4L8.2 4.1a.8.8 0 0 0-1.2.7z"/>',
  refresh: '<path d="M20 11.5A8 8 0 1 0 17.7 17M20 4.5v7h-7"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  trash: '<path d="M4 7h16M9.5 7V4.5h5V7M6.5 7l.9 12.2A1.5 1.5 0 0 0 8.9 20.5h6.2a1.5 1.5 0 0 0 1.5-1.3L17.5 7M10 11v6M14 11v6"/>',
  check: '<path d="m5 12.5 4.5 4.5L19 7.5"/>',
  x: '<path d="M17.5 6.5l-11 11M6.5 6.5l11 11"/>',
  alert: '<path d="M12 9v4.5M12 17h.01"/><path d="M10.3 4 2.6 17.5A2 2 0 0 0 4.3 20.5h15.4a2 2 0 0 0 1.7-3L13.7 4a2 2 0 0 0-3.4 0z"/>',
  info: '<circle cx="12" cy="12" r="9"/><path d="M12 16.5v-5M12 8h.01"/>',
  upload: '<path d="M20 15v3.5a1.5 1.5 0 0 1-1.5 1.5h-13A1.5 1.5 0 0 1 4 18.5V15M16.5 8.5 12 4 7.5 8.5M12 4v11.5"/>',
  download: '<path d="M20 15v3.5a1.5 1.5 0 0 1-1.5 1.5h-13A1.5 1.5 0 0 1 4 18.5V15M7.5 10.5 12 15l4.5-4.5M12 15V3.5"/>',
  menu: '<path d="M4 7h16M4 12h16M4 17h16"/>',
  chevron: '<path d="m7 10 5 5 5-5"/>',
  shield: '<path d="M12 21s7.5-3.6 7.5-9.4V5.8L12 3 4.5 5.8v5.8C4.5 17.4 12 21 12 21z"/><path d="m9 12 2.2 2.2L15.5 10"/>',
  bolt: '<path d="M13 2.5 4.5 13.5H12l-1 8 8.5-11H12l1-8z"/>',
  target: '<circle cx="12" cy="12" r="8.5"/><circle cx="12" cy="12" r="4.5"/><circle cx="12" cy="12" r=".8"/>',
  arrow: '<path d="M5 12h14M13 6l6 6-6 6"/>',
};

export function icon(name, size = 16) {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("width", size);
  svg.setAttribute("height", size);
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "1.8");
  svg.setAttribute("stroke-linecap", "round");
  svg.setAttribute("stroke-linejoin", "round");
  svg.setAttribute("aria-hidden", "true");
  svg.classList.add("icon");
  svg.innerHTML = ICONS[name] || "";
  return svg;
}

// ---- slots: colour is secondary; every slot also carries its number ---------
export const SLOT_COLORS = 8;
export const slotColor = (c) => (c >= 0 && c < SLOT_COLORS ? `var(--s${c + 1})` : "var(--s-other)");
export const slotName = (c) => `S${c + 1}`;

export function slotDot(c, label = slotName(c)) {
  return h("span", { class: "slot-tag", style: { "--slot": slotColor(c) } }, h("i", { "aria-hidden": "true" }), label);
}

// ---- formatting -----------------------------------------------------------
export const fmt = {
  int: (n) => Number(n).toLocaleString("en-US"),
  ms(value) {
    const v = Number(value);
    if (v === 0) return "<0.001";
    if (v < 1) return v.toFixed(3);
    if (v < 100) return v.toFixed(2);
    return v.toFixed(0);
  },
  pct: (v) => `${Math.round(v)}%`,
  plural: (n, word, many = `${word}s`) => `${fmt.int(n)} ${n === 1 ? word : many}`,
};

// ---- buttons, badges, toasts, dialogs --------------------------------------
export function button(label, { variant = "secondary", iconName, onClick, busy = false, disabled = false, title, type = "button", size } = {}) {
  return h(
    "button",
    {
      type,
      class: `btn btn-${variant}${size ? ` btn-${size}` : ""}${busy ? " is-busy" : ""}`,
      onclick: onClick,
      disabled: disabled || busy,
      title,
      "aria-busy": busy ? "true" : null,
    },
    busy ? h("span", { class: "spinner", "aria-hidden": "true" }) : iconName ? icon(iconName) : null,
    h("span", {}, label),
  );
}

export function badge(text, kind = "neutral", iconName) {
  return h("span", { class: `badge badge-${kind}` }, iconName ? icon(iconName, 13) : null, text);
}

export function toast(message, kind = "info") {
  const region = document.getElementById("toasts");
  const item = h(
    "div",
    { class: `toast toast-${kind}`, role: kind === "error" ? "alert" : "status" },
    icon(kind === "error" ? "alert" : kind === "success" ? "check" : "info"),
    h("span", {}, message),
  );
  region.append(item);
  setTimeout(() => {
    item.classList.add("leaving");
    setTimeout(() => item.remove(), 250);
  }, kind === "error" ? 6500 : 3500);
}

export function confirmDialog({ title, body, confirmLabel = "Confirm", danger = false }) {
  return new Promise((resolve) => {
    const dialog = h("dialog", { class: "dialog", "aria-labelledby": "dialog-title" });
    const close = (value) => {
      dialog.close();
      dialog.remove();
      resolve(value);
    };
    dialog.append(
      h("h2", { id: "dialog-title" }, title),
      h("p", {}, body),
      h(
        "div",
        { class: "dialog-actions" },
        button("Cancel", { onClick: () => close(false) }),
        button(confirmLabel, { variant: danger ? "danger" : "primary", onClick: () => close(true) }),
      ),
    );
    dialog.addEventListener("cancel", (event) => {
      event.preventDefault();
      close(false);
    });
    document.body.append(dialog);
    dialog.showModal();
  });
}

// Remembers what the user typed across re-renders (keyed by input id).
const drafts = new Map();
export function draft(id, fallback = "") {
  return drafts.has(id) ? drafts.get(id) : fallback;
}
export function keepDraft(input) {
  input.addEventListener("input", () => drafts.set(input.id, input.type === "checkbox" ? input.checked : input.value));
  return input;
}
export function clearDrafts(...ids) {
  ids.forEach((id) => drafts.delete(id));
}

export function emptyState({ iconName, title, body, actions = [] }) {
  return h(
    "div",
    { class: "empty" },
    h("div", { class: "empty-icon" }, icon(iconName, 22)),
    h("h3", {}, title),
    h("p", {}, body),
    actions.length ? h("div", { class: "empty-actions" }, actions) : null,
  );
}

export function sectionHeader(title, description, ...actions) {
  return h(
    "div",
    { class: "section-head" },
    h("div", {}, h("h2", {}, title), description ? h("p", {}, description) : null),
    actions.length ? h("div", { class: "section-actions" }, actions) : null,
  );
}
