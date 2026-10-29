// Hosts the C++ engine (timetable.js / timetable.wasm) in a Web Worker so long
// searches never freeze the page. The engine talks over stdin/stdout exactly
// like the native build:
//   main -> worker  {type:'line', line}            one line of stdin
//                   {type:'fs', id, op, path, data} read/write the in-memory FS
//   worker -> main  {type:'out', bytes}            stdout/stderr bytes
//                   {type:'waiting'}               engine blocked on readLine
//                   {type:'exit', code} | {type:'abort', reason} | {type:'fs', ...}
importScripts("timetable.js");

// emscripten_force_exit() unwinds the suspended C++ stack by throwing
// ExitStatus; the exit itself is already reported through onExit.
self.addEventListener("unhandledrejection", (event) => {
  if (event.reason && event.reason.name === "ExitStatus") event.preventDefault();
});

let engineModule = null;
let waiting = null;
const pendingLines = [];
let outBytes = [];

function flushOutput() {
  if (outBytes.length === 0) return;
  const bytes = new Uint8Array(outBytes);
  outBytes = [];
  postMessage({ type: "out", bytes }, [bytes.buffer]);
}

function putByte(byte) {
  if (byte === null || byte === undefined) return;
  outBytes.push(byte & 255);
  if (outBytes.length >= 65536) flushOutput();
}

self.onmessage = (event) => {
  const message = event.data;
  if (message.type === "line") {
    if (waiting) {
      const resolve = waiting;
      waiting = null;
      resolve(message.line);
    } else {
      pendingLines.push(message.line);
    }
  } else if (message.type === "fs") {
    try {
      if (!engineModule) throw new Error("engine not started");
      if (message.op === "write") {
        engineModule.FS.writeFile(message.path, message.data);
        postMessage({ type: "fs", id: message.id, ok: true });
      } else {
        const data = engineModule.FS.readFile(message.path, { encoding: "utf8" });
        postMessage({ type: "fs", id: message.id, ok: true, data });
      }
    } catch (error) {
      postMessage({ type: "fs", id: message.id, ok: false, error: String((error && error.message) || error) });
    }
  }
};

createTimetable({
  readLine() {
    flushOutput();
    if (pendingLines.length) return Promise.resolve(pendingLines.shift());
    postMessage({ type: "waiting" });
    return new Promise((resolve) => { waiting = resolve; });
  },
  preRun: [(module) => {
    engineModule = module;
    module.FS.init(null, putByte, putByte);
  }],
  onExit(code) {
    flushOutput();
    postMessage({ type: "exit", code });
  },
  onAbort(reason) {
    flushOutput();
    postMessage({ type: "abort", reason: String(reason) });
  },
}).catch((error) => postMessage({ type: "abort", reason: String(error) }));
