// Browser front end for the WebAssembly build.
// The C++ program writes bytes to stdout (rendered here, ANSI colours included)
// and asks for input through Module.readLine(), which returns a Promise that
// resolves when the user submits a line.

(function () {
  const terminal = document.getElementById("terminal");
  const output = document.getElementById("output");
  const form = document.getElementById("form");
  const input = document.getElementById("input");
  const send = document.getElementById("send");
  const restart = document.getElementById("restart");
  const MAX_NODES = 6000;  // trim very long sessions

  // ---- output with a tiny ANSI SGR parser ---------------------------------
  const decoder = new TextDecoder();
  let classes = [];      // active span classes
  let escape = null;     // partial escape sequence, or null
  let pending = "";      // text waiting to be appended
  let flushQueued = false;

  function spanFor(text) {
    if (classes.length === 0) return document.createTextNode(text);
    const span = document.createElement("span");
    span.className = classes.join(" ");
    span.textContent = text;
    return span;
  }

  function emit() {
    if (pending) {
      output.appendChild(spanFor(pending));
      pending = "";
    }
  }

  function applySgr(params) {
    for (const code of (params || "0").split(";").map(Number)) {
      if (code === 0) classes = [];
      else if (code === 1) classes = classes.filter(c => c !== "b").concat("b");
      else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
        classes = classes.filter(c => !/^c\d+$/.test(c)).concat("c" + code);
      }
    }
  }

  function write(text) {
    for (const ch of text) {
      if (escape !== null) {
        escape += ch;
        if (/[A-Za-z]/.test(ch)) {
          const match = /^\[([\d;]*)m$/.exec(escape);
          if (match) { emit(); applySgr(match[1]); }
          escape = null;
        }
      } else if (ch === "\x1b") {
        escape = "";
      } else {
        pending += ch;
      }
    }
    if (!flushQueued) {
      flushQueued = true;
      requestAnimationFrame(flush);
    }
  }

  function flush() {
    flushQueued = false;
    emit();
    while (output.childNodes.length > MAX_NODES) output.removeChild(output.firstChild);
    terminal.scrollTop = terminal.scrollHeight;
  }

  function writeByte(byte) {
    if (byte === null) return;
    write(decoder.decode(new Uint8Array([byte & 255]), { stream: true }));
  }

  function echo(line) {
    emit();
    const span = document.createElement("span");
    span.className = "echo";
    span.textContent = line + "\n";
    output.appendChild(span);
    flush();
  }

  // ---- input ---------------------------------------------------------------
  const queued = [];     // lines submitted before the program asked
  let waiting = null;    // resolver of the current readLine() promise
  const history = [];
  let historyIndex = 0;

  function submit(line) {
    if (input.disabled) return;
    echo(line);
    if (line) { history.push(line); historyIndex = history.length; }
    if (waiting) { const resolve = waiting; waiting = null; resolve(line); }
    else queued.push(line);
  }

  function readLine() {
    if (queued.length) return Promise.resolve(queued.shift());
    flush();
    return new Promise(resolve => { waiting = resolve; });
  }

  form.addEventListener("submit", event => {
    event.preventDefault();
    submit(input.value);
    input.value = "";
    input.focus();
  });

  input.addEventListener("keydown", event => {
    if (event.key === "ArrowUp" && historyIndex > 0) {
      input.value = history[--historyIndex];
      event.preventDefault();
    } else if (event.key === "ArrowDown" && historyIndex < history.length) {
      historyIndex++;
      input.value = history[historyIndex] || "";
      event.preventDefault();
    }
  });

  document.querySelectorAll("[data-send]").forEach(button => {
    button.addEventListener("click", () => { submit(button.dataset.send); input.focus(); });
  });

  terminal.addEventListener("click", () => {
    if (!window.getSelection().toString()) input.focus();
  });

  restart.addEventListener("click", () => location.reload());

  function finished(message) {
    flush();
    write("\n" + message + "\n");
    flush();
    input.disabled = true;
    send.disabled = true;
    restart.hidden = false;
  }

  // ---- start ---------------------------------------------------------------
  window.startTimetable = function () {
    output.textContent = "";
    createTimetable({
      readLine,
      preRun: [module => module.FS.init(null, writeByte, writeByte)],
      onExit: () => finished("[Program exited - press Restart to run it again]"),
      onAbort: reason => finished("[Program crashed: " + reason + "]"),
    }).then(() => {
      input.disabled = false;
      send.disabled = false;
      input.focus();
    }).catch(error => finished("[Could not start the program: " + error + "]"));
  };
})();
