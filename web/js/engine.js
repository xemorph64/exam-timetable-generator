// Engine integration layer.
//
// One C++ engine instance runs in worker.js. Humans use it through the Engine
// console (menu numbers, prompts); the GUI uses the machine protocol on the
// same stdin/stdout: it sends "@<id> <verb> [payload]" only while the engine
// sits at the main menu, and waits for the matching
// "@@BEGIN <id>\n{json}\n@@END <id>" block. Nothing is treated as a success
// unless that block arrives, parses, and says ok:true.

const MENU_PROMPT = "Enter choice (1-16): ";
const CALL_TIMEOUT_MS = 90000;

export class EngineError extends Error {
  constructor(message, kind) {
    super(message);
    this.kind = kind;  // 'rejected' | 'busy' | 'timeout' | 'stopped' | 'protocol'
  }
}

const stripAnsi = (text) => text.replace(/\x1b\[[\d;]*[A-Za-z]/g, "");

export class Engine extends EventTarget {
  constructor() {
    super();
    this.status = "stopped";
    this.worker = null;
    this.nextId = 1;
    this.pending = null;
    this.queue = Promise.resolve();
    this.fsCalls = new Map();
  }

  // ---- lifecycle -----------------------------------------------------------
  start() {
    if (this.worker) this.worker.terminate();
    this.rejectPending(new EngineError("The engine was restarted.", "stopped"));
    for (const call of this.fsCalls.values()) call.reject(new EngineError("The engine was restarted.", "stopped"));
    this.fsCalls.clear();
    this.decoder = new TextDecoder();
    this.carry = "";
    this.tail = "";
    this.answeredSinceText = false;
    this.consoleInput = false;
    this.generation = (this.generation || 0) + 1;
    this.setStatus("loading");
    this.emit("reset");

    this.worker = new Worker("worker.js");
    this.worker.onmessage = (event) => this.onMessage(event.data);
    this.worker.onerror = (event) => this.fail(`Engine worker error: ${event.message || "unknown"}`);
  }

  onMessage(message) {
    switch (message.type) {
      case "out":
        this.consume(this.decoder.decode(message.bytes, { stream: true }));
        break;
      case "waiting":
        this.onWaiting();
        break;
      case "exit":
        this.rejectPending(new EngineError("The engine exited.", "stopped"));
        this.setStatus("exited");
        break;
      case "abort":
        this.fail(`The engine stopped unexpectedly: ${message.reason}`);
        break;
      case "fs": {
        const call = this.fsCalls.get(message.id);
        if (!call) break;
        this.fsCalls.delete(message.id);
        if (message.ok) call.resolve(message.data);
        else call.reject(new EngineError(`File operation failed: ${message.error}`, "rejected"));
        break;
      }
      default:
        break;
    }
  }

  fail(reason) {
    this.rejectPending(new EngineError(reason, "stopped"));
    this.setStatus("crashed");
    this.emit("output", { text: `\n[${reason}]\n`, system: true });
  }

  // ---- output parsing ------------------------------------------------------
  consume(text) {
    this.carry += text;
    while (this.carry) {
      const begin = this.pending ? this.carry.indexOf(`@@BEGIN ${this.pending.id}\n`) : -1;
      if (begin === -1) {
        // Hold back a possible partial marker at the very end of the chunk.
        const partial = this.pending ? this.carry.lastIndexOf("@@") : -1;
        const cut = partial !== -1 && partial > this.carry.length - 24 ? partial : this.carry.length;
        this.consoleText(this.carry.slice(0, cut));
        this.carry = this.carry.slice(cut);
        return;
      }
      if (begin > 0) {
        this.consoleText(this.carry.slice(0, begin));
        this.carry = this.carry.slice(begin);
      }
      const id = this.pending.id;
      const header = `@@BEGIN ${id}\n`;
      const footer = `\n@@END ${id}\n`;
      const end = this.carry.indexOf(footer, header.length);
      if (end === -1) return;  // wait for the rest of the block
      const body = this.carry.slice(header.length, end);
      this.carry = this.carry.slice(end + footer.length);
      this.onBlock(body);
    }
  }

  consoleText(text) {
    if (!text) return;
    this.answeredSinceText = false;
    this.tail = (this.tail + stripAnsi(text)).slice(-200);
    this.emit("output", { text });
  }

  onBlock(body) {
    const call = this.pending;
    this.pending = null;
    this.answeredSinceText = true;
    let data;
    try {
      data = JSON.parse(body);
    } catch {
      call.reject(new EngineError("The engine returned a response that could not be read.", "protocol"));
      return;
    }
    this.emit("traffic", { verb: call.verb, ok: data.ok === true, bytes: body.length });
    if (data.ok === true) call.resolve(data);
    else call.reject(new EngineError(data.error || "The engine rejected the request.", "rejected"));
  }

  onWaiting() {
    if (this.pending) {
      // The engine asked for input without answering: the request was not
      // understood as a protocol command. Never assume it worked.
      this.rejectPending(new EngineError("The engine did not answer the request.", "protocol"));
    }
    const atMenu = this.answeredSinceText || this.tail.endsWith(MENU_PROMPT);
    const fromConsole = this.consoleInput;
    this.consoleInput = false;
    this.setStatus(atMenu ? "menu" : "prompt");
    if (atMenu && fromConsole) this.emit("console-idle");
  }

  // ---- requests ------------------------------------------------------------
  // GUI request through the machine protocol. Serialised: one at a time.
  call(verb, payload = "") {
    const run = () => this.whenSettled().then(() => this.callNow(verb, payload));
    const promise = this.queue.then(run, run);
    this.queue = promise.catch(() => {});
    return promise;
  }

  // A response block arrives just before the engine asks for its next line;
  // wait until it is really back at a prompt (or has stopped) before sending.
  whenSettled() {
    if (this.status !== "busy" && this.status !== "loading") return Promise.resolve();
    return new Promise((resolve) => {
      const onStatus = () => {
        if (this.status === "busy" || this.status === "loading") return;
        this.removeEventListener("status", onStatus);
        resolve();
      };
      this.addEventListener("status", onStatus);
    });
  }

  callNow(verb, payload) {
    if (this.status !== "menu") {
      const message = this.status === "prompt"
        ? "The engine is waiting for an answer in the Engine console. Finish that prompt first."
        : this.status === "loading" ? "The engine is still starting." : "The engine is not running. Restart it to continue.";
      return Promise.reject(new EngineError(message, "busy"));
    }
    if (/[\r\n]/.test(payload)) return Promise.reject(new EngineError("Values cannot contain line breaks.", "rejected"));

    const id = String(this.nextId++);
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        if (this.pending && this.pending.id === id) {
          this.rejectPending(new EngineError("The engine did not answer in time. Restart it from the console.", "timeout"));
          this.setStatus("unresponsive");
        }
      }, CALL_TIMEOUT_MS);
      this.pending = {
        id,
        verb,
        resolve: (value) => { clearTimeout(timer); resolve(value); },
        reject: (error) => { clearTimeout(timer); reject(error); },
      };
      this.setStatus("busy");
      this.worker.postMessage({ type: "line", line: `@${id} ${verb}${payload ? ` ${payload}` : ""}` });
    });
  }

  // Human input from the Engine console.
  sendLine(line) {
    if (this.status !== "menu" && this.status !== "prompt") return false;
    this.consoleInput = true;
    this.emit("echo", { line });
    this.setStatus("busy");
    this.worker.postMessage({ type: "line", line });
    return true;
  }

  writeFile(path, data) { return this.fsCall("write", path, data); }
  readFile(path) { return this.fsCall("read", path); }

  fsCall(op, path, data) {
    if (!this.worker || this.status === "crashed" || this.status === "exited") {
      return Promise.reject(new EngineError("The engine is not running.", "stopped"));
    }
    const id = `fs${this.nextId++}`;
    return new Promise((resolve, reject) => {
      this.fsCalls.set(id, { resolve, reject });
      this.worker.postMessage({ type: "fs", id, op, path, data });
    });
  }

  rejectPending(error) {
    if (!this.pending) return;
    const call = this.pending;
    this.pending = null;
    call.reject(error);
  }

  setStatus(status) {
    if (this.status === status) return;
    this.status = status;
    this.emit("status", { status });
  }

  emit(type, detail = {}) {
    this.dispatchEvent(new CustomEvent(type, { detail }));
  }
}
