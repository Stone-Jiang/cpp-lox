const examples = {
  basics: `print "Craft playground";

var a = 6;
var b = 7;
print a * b;
print sqrt(81);`,
  functions: `fun fibonacci(n) {
  if (n <= 1) return n;
  return fibonacci(n - 1) + fibonacci(n - 2);
}

print fibonacci(12);`,
  classes: `class Counter {
  init(start) {
    this.value = start;
  }

  increment() {
    this.value = this.value + 1;
    return this.value;
  }
}

var counter = Counter(40);
print counter.increment();
print counter.increment();`,
  complex: `var z = 3 + 4i;
print z;
print mag(z);
print sqrt'(z);`
};

const editor = document.querySelector("#editor");
const lineNumbers = document.querySelector("#lineNumbers");
const output = document.querySelector("#output");
const exitCode = document.querySelector("#exitCode");
const runButton = document.querySelector("#runCode");
const status = document.querySelector("#status");
const statusText = document.querySelector("#statusText");
const replHistory = document.querySelector("#replHistory");
const replInput = document.querySelector("#replInput");

function setStatus(state, text) {
  status.dataset.state = state;
  statusText.textContent = text;
}

function updateLines() {
  const count = Math.max(1, editor.value.split("\n").length);
  lineNumbers.textContent = Array.from({ length: count }, (_, index) => index + 1).join("\n");
  lineNumbers.scrollTop = editor.scrollTop;
}

async function postJson(path, payload = {}) {
  const response = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
  return data;
}

async function runSource() {
  runButton.disabled = true;
  setStatus("busy", "Running");
  output.textContent = "";
  exitCode.textContent = "";
  try {
    const result = await postJson("/api/run", { source: editor.value });
    output.textContent = result.output || "Program completed without output.";
    exitCode.textContent = `exit ${result.exitCode}`;
    exitCode.classList.toggle("error", result.exitCode !== 0);
    setStatus(result.exitCode === 0 ? "ready" : "error", result.exitCode === 0 ? "Ready" : "Failed");
  } catch (error) {
    output.textContent = `[playground] ${error.message}`;
    exitCode.textContent = "error";
    exitCode.classList.add("error");
    setStatus("error", "Disconnected");
  } finally {
    runButton.disabled = false;
  }
}

function switchConsole(view) {
  document.querySelectorAll(".console-tab").forEach(tab => tab.classList.toggle("active", tab.dataset.view === view));
  document.querySelectorAll(".console-view").forEach(panel => panel.classList.toggle("active", panel.id === `${view}View`));
  if (view === "repl") replInput.focus();
}

function addReplEntry(command, result, failed = false) {
  const entry = document.createElement("div");
  entry.className = "repl-entry";
  const commandLine = document.createElement("div");
  commandLine.className = "repl-command";
  commandLine.textContent = `> ${command}`;
  const resultLine = document.createElement("div");
  resultLine.className = `repl-result${failed ? " repl-error" : ""}`;
  resultLine.textContent = result || "";
  entry.append(commandLine, resultLine);
  replHistory.append(entry);
  replHistory.scrollTop = replHistory.scrollHeight;
}

editor.value = localStorage.getItem("craft-playground-source") || examples.basics;
updateLines();

editor.addEventListener("input", () => {
  updateLines();
  localStorage.setItem("craft-playground-source", editor.value);
});
editor.addEventListener("scroll", updateLines);
editor.addEventListener("keydown", event => {
  if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
    event.preventDefault();
    runSource();
  }
  if (event.key === "Tab") {
    event.preventDefault();
    const start = editor.selectionStart;
    editor.setRangeText("  ", start, editor.selectionEnd, "end");
    editor.dispatchEvent(new Event("input"));
  }
});

runButton.addEventListener("click", runSource);
document.querySelector("#clearEditor").addEventListener("click", () => {
  editor.value = "";
  editor.dispatchEvent(new Event("input"));
  editor.focus();
});
document.querySelector("#clearConsole").addEventListener("click", () => {
  output.textContent = "";
  replHistory.replaceChildren();
  exitCode.textContent = "";
});

document.querySelectorAll(".example").forEach(button => button.addEventListener("click", () => {
  document.querySelectorAll(".example").forEach(item => item.classList.remove("active"));
  button.classList.add("active");
  editor.value = examples[button.dataset.example];
  editor.dispatchEvent(new Event("input"));
  editor.focus();
}));

document.querySelectorAll(".console-tab").forEach(tab => tab.addEventListener("click", () => switchConsole(tab.dataset.view)));

document.querySelector("#replForm").addEventListener("submit", async event => {
  event.preventDefault();
  const source = replInput.value.trim();
  if (!source) return;
  replInput.value = "";
  replInput.disabled = true;
  setStatus("busy", "Evaluating");
  try {
    const result = await postJson("/api/repl", { source });
    addReplEntry(source, result.output);
    setStatus("ready", "Ready");
  } catch (error) {
    addReplEntry(source, error.message, true);
    setStatus("error", "REPL error");
  } finally {
    replInput.disabled = false;
    replInput.focus();
  }
});

document.querySelector("#resetRepl").addEventListener("click", async () => {
  try {
    await postJson("/api/repl/reset");
    replHistory.replaceChildren();
    setStatus("ready", "REPL reset");
  } catch (error) {
    addReplEntry("reset", error.message, true);
  }
  replInput.focus();
});
