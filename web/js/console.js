// Engine console: the real program's stdin/stdout, exactly as the terminal
// build shows it. GUI requests are hidden unless "Show GUI requests" is on.
import { engine } from "./store.js";
import { h, icon, button } from "./ui.js";

const MAX_NODES = 5000;

class AnsiOutput {
  constructor(pre) {
    this.pre = pre;
    this.classes = [];
    this.escape = null;
  }

  write(text, extraClass) {
    let pending = "";
    const emit = () => {
      if (!pending) return;
      const classes = extraClass ? [...this.classes, extraClass] : this.classes;
      if (classes.length === 0) this.pre.append(document.createTextNode(pending));
      else this.pre.append(h("span", { class: classes.join(" ") }, pending));
      pending = "";
    };
    for (const ch of text) {
      if (this.escape !== null) {
        this.escape += ch;
        if (/[A-Za-z]/.test(ch)) {
          const match = /^\[([\d;]*)m$/.exec(this.escape);
          if (match) {
            emit();
            this.applySgr(match[1]);
          }
          this.escape = null;
        }
      } else if (ch === "\x1b") {
        this.escape = "";
      } else {
        pending += ch;
      }
    }
    emit();
    while (this.pre.childNodes.length > MAX_NODES) this.pre.firstChild.remove();
  }

  applySgr(params) {
    for (const code of (params || "0").split(";").map(Number)) {
      if (code === 0) this.classes = [];
      else if (code === 1) this.classes = [...this.classes.filter((c) => c !== "b"), "b"];
      else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
        this.classes = [...this.classes.filter((c) => !/^c\d+$/.test(c)), `c${code}`];
      }
    }
  }

  clear() {
    this.pre.textContent = "";
  }
}

export function setupConsole({ root, onToggle }) {
  const pre = h("pre", { class: "console-output", "aria-live": "off" });
  const output = new AnsiOutput(pre);
  const scroller = h("div", { class: "console-scroll", tabindex: "0", "aria-label": "Engine output" }, pre);
  const input = h("input", {
    id: "console-input", type: "text", autocomplete: "off", spellcheck: "false",
    placeholder: "Type a menu number or answer, then Enter", "aria-label": "Engine input",
  });
  const traffic = h("input", { type: "checkbox", id: "console-traffic" });
  const statusText = h("span", { class: "console-status" });
  const history = [];
  let historyIndex = 0;

  const scrollDown = () => requestAnimationFrame(() => { scroller.scrollTop = scroller.scrollHeight; });

  engine.addEventListener("output", (event) => {
    output.write(event.detail.text, event.detail.system ? "sys" : null);
    scrollDown();
  });
  engine.addEventListener("echo", (event) => {
    output.write(`${event.detail.line}\n`, "echo");
    scrollDown();
  });
  engine.addEventListener("traffic", (event) => {
    if (!traffic.checked) return;
    const { verb, ok, bytes } = event.detail;
    output.write(`  ⇄ GUI request "${verb}" → ${ok ? "ok" : "error"} (${(bytes / 1024).toFixed(1)} kB JSON)\n`, "traffic");
    scrollDown();
  });
  engine.addEventListener("reset", () => {
    output.clear();
    output.write("Starting the C++ engine (WebAssembly)...\n", "sys");
  });

  const sync = () => {
    const status = engine.status;
    const open = status === "menu" || status === "prompt";
    input.disabled = !open;
    statusText.textContent = {
      loading: "Starting…", menu: "Waiting for a menu choice", prompt: "Waiting for your answer",
      busy: "Running…", exited: "Program exited", crashed: "Engine crashed", unresponsive: "Not responding",
    }[status] || status;
    statusText.dataset.status = status;
  };
  engine.addEventListener("status", sync);

  const form = h(
    "form",
    {
      class: "console-input",
      onsubmit: (event) => {
        event.preventDefault();
        const line = input.value;
        if (/[\r\n]/.test(line)) return;
        if (engine.sendLine(line)) {
          if (line) {
            history.push(line);
            historyIndex = history.length;
          }
          input.value = "";
        }
      },
    },
    h("span", { class: "console-caret", "aria-hidden": "true" }, "›"),
    input,
    button("Send", { type: "submit", variant: "primary", size: "sm" }),
  );
  input.addEventListener("keydown", (event) => {
    if (event.key === "ArrowUp" && historyIndex > 0) {
      input.value = history[--historyIndex];
      event.preventDefault();
    } else if (event.key === "ArrowDown" && historyIndex < history.length) {
      historyIndex++;
      input.value = history[historyIndex] || "";
      event.preventDefault();
    }
  });

  root.append(
    h(
      "div",
      { class: "console-head" },
      h("div", { class: "console-title" }, icon("terminal"), h("strong", {}, "Engine console"), statusText),
      h(
        "div",
        { class: "console-tools" },
        h("label", { class: "check" }, traffic, h("span", {}, "Show GUI requests")),
        button("Clear view", { size: "sm", variant: "ghost", onClick: () => output.clear() }),
        button("Restart engine", { size: "sm", variant: "ghost", iconName: "refresh", onClick: () => engine.start(), title: "Starts a fresh engine (in-memory data is cleared)" }),
        h("button", { class: "icon-btn", type: "button", "aria-label": "Close console", onclick: () => onToggle(false) }, icon("x")),
      ),
    ),
    scroller,
    form,
    h("p", { class: "console-hint" }, "This is the real program's input and output — the same menu as the terminal build. While a prompt is open here, the graphical pages wait."),
  );
  sync();
  return { focus: () => input.focus() };
}
