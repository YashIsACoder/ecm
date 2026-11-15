import { drawTelemetryHistory, policyStyles, raceClock, replaySpeed, traceAt } from "./replay.mjs";
import { downloadFile, numbers, percent, policyLabels as labels, readRun, runQuery, runSummary } from "./run-data.mjs";

const feedback = document.querySelector("#run-feedback");
const status = document.querySelector("#status");
const outcomes = document.querySelector("#outcomes");
const results = document.querySelector("#race-outcomes");
const summary = document.querySelector("#summary");
const paired = document.querySelector("#paired");
const diagnostics = document.querySelector("#trial-diagnostics");
const comparison = document.querySelector("#comparison");
const chart = document.querySelector("#reach");
const download = document.querySelector("#download");
const history = document.querySelector("#history");
const slider = document.querySelector("#history-time");
const clockText = document.querySelector("#history-clock");
const values = document.querySelector("#history-values");
const battery = document.querySelector("#battery");
const historyTitle = document.querySelector("#history-title");
const historyButtons = [...document.querySelectorAll(".history-switch button")];
let current;
let mode = "battery";

const pointDelta = (value) => {
  const points = 100 * value;
  return `${points > 0 ? "+" : ""}${points.toFixed(1)}`;
};

function addCell(row, value, note) {
  const cell = row.insertCell();
  cell.textContent = value;
  if (note) {
    const extra = document.createElement("span");
    extra.textContent = note;
    cell.append(extra);
  }
}

function namedRow(table, name) {
  const row = table.insertRow();
  const label = document.createElement("th");
  label.scope = "row";
  label.textContent = labels[name];
  row.append(label);
  return row;
}

function addPairedCell(row, values) {
  const [estimate, low, high] = values.map(pointDelta);
  addCell(row, `${estimate} pp`, `${low} to ${high} pp`);
}

function render({ data, settings, recorded }) {
  current = data;
  document.querySelector("#back-map").href = `./${runQuery(settings, recorded)}`;
  feedback.hidden = !recorded;
  if (recorded) feedback.textContent = "Recorded SR7 run";
  results.hidden = false;
  history.hidden = false;
  download.disabled = false;
  summary.replaceChildren();
  paired.replaceChildren();
  diagnostics.replaceChildren();
  for (const policy of data.policies) {
    const row = namedRow(summary, policy.name);
    addCell(row, percent(policy.finishes / data.trials), policy.completion_ci.map(percent).join("–"));
    addCell(row, percent(policy.reserve_breaches / data.trials), policy.reserve_ci.map(percent).join("–"));
    addCell(row, policy.median_race_hours === null ? "—" : `${policy.median_race_hours.toFixed(2)} h`,
      policy.median_hours === null ? "" : `${policy.median_hours.toFixed(2)} h driving`);
    const metrics = policy.diagnostics;
    const diagnosticRow = namedRow(diagnostics, policy.name);
    addCell(diagnosticRow, `${numbers.format(metrics.distance_km)} km`);
    addCell(diagnosticRow, `${metrics.finish_soc_pct.toFixed(1)}%`);
    addCell(diagnosticRow, `${metrics.min_soc_pct.toFixed(1)}%`);
    for (const key of ["demand_kwh", "solar_kwh", "spilled_kwh"]) {
      addCell(diagnosticRow, `${metrics[key].toFixed(2)} kWh`);
    }
    if (policy.name !== "steady") {
      const pairedRow = namedRow(paired, policy.name);
      addPairedCell(pairedRow, data.completion_vs_steady[policy.name]);
      addPairedCell(pairedRow, data.reserve_vs_steady[policy.name]);
    }
  }
  const [delta, low, high] = data.completion_vs_steady["reserve-first"].map(pointDelta);
  comparison.textContent =
    `Reserve-first vs steady: ${delta} percentage points in finish rate (95% paired interval ${low} to ${high}).`;
  status.textContent = runSummary(data, settings);
  outcomes.hidden = false;
  const { logs, draw } = data.trace;
  document.querySelector("#draw-label").textContent = `DRAW ${draw} OF ${numbers.format(data.trials)}`;
  const horizon = Math.max(...Object.values(logs).map((events) => events.at(-1).race_arrive_hours));
  const at = Number(new URLSearchParams(location.search).get("at")) || 0;
  slider.max = horizon;
  slider.value = Math.max(0, Math.min(horizon, at));
  slider.disabled = horizon === 0;
  drawChart();
  drawHistory();
}

function drawChart() {
  if (!current || outcomes.hidden) {
    return;
  }
  const width = chart.clientWidth;
  const height = chart.clientHeight;
  if (!width || !height) {
    return;
  }
  const ratio = window.devicePixelRatio || 1;
  chart.width = Math.round(width * ratio);
  chart.height = Math.round(height * ratio);
  const ctx = chart.getContext("2d");
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  const css = getComputedStyle(chart);
  const color = (name) => css.getPropertyValue(name).trim();
  const left = 42;
  const right = width - 27;
  const top = 14;
  const bottom = height - 38;
  const maxDistance = current.distances_km.at(-1);
  const x = (distance) => left + (right - left) * distance / maxDistance;
  const y = (share) => bottom - (bottom - top) * share;
  ctx.font = "11px system-ui, sans-serif";
  ctx.fillStyle = color("--muted");
  ctx.strokeStyle = color("--line");
  ctx.textBaseline = "middle";
  ctx.textAlign = "right";
  for (const share of [0, .25, .5, .75, 1]) {
    const tick = y(share);
    ctx.beginPath();
    ctx.moveTo(left, tick);
    ctx.lineTo(right, tick);
    ctx.stroke();
    ctx.fillText(`${share * 100}%`, left - 6, tick);
  }
  ctx.textAlign = "center";
  for (const fraction of [0, .25, .5, .75, 1]) {
    const distance = fraction * maxDistance;
    ctx.fillText(numbers.format(distance), x(distance), bottom + 12);
  }
  ctx.fillText("Distance (km)", (left + right) / 2, height - 7);
  current.policies.forEach((policy) => {
    ctx.strokeStyle = color(policyStyles[policy.name].color);
    ctx.setLineDash(policyStyles[policy.name].dash);
    ctx.lineWidth = 2;
    ctx.beginPath();
    policy.route_reach.forEach((share, point) => {
      const position = x(current.distances_km[point]);
      if (point === 0) {
        ctx.moveTo(position, y(share));
      } else {
        ctx.lineTo(position, y(share));
      }
    });
    ctx.stroke();
  });
}

function drawHistory() {
  if (!current) return;
  const hour = Number(slider.value);
  const { logs } = current.trace;
  const states = Object.fromEntries(Object.entries(logs).map(([name, events]) => [name, traceAt(events, hour)]));
  const { day, clock } = raceClock(hour);
  const time = `Day ${day} · ${clock}`;
  clockText.textContent = time;
  slider.setAttribute("aria-valuetext", time);
  drawTelemetryHistory(battery, logs, hour, Math.max(1e-6, Number(slider.max)), states, mode);
  values.replaceChildren(...Object.entries(states).map(([name, state]) => {
    const item = document.createElement("li");
    const value = mode === "speed" ? `${replaySpeed(state).toFixed(1)} km/h` : `${state.soc.toFixed(1)}%`;
    item.textContent = `${labels[name]} · ${value}`;
    return item;
  }));
}

historyButtons.forEach((button) => button.addEventListener("click", () => {
  mode = button.dataset.history;
  const speed = mode === "speed";
  historyTitle.textContent = speed ? "Speed history · km/h" : "Battery history";
  battery.setAttribute("aria-label", speed
    ? "Leg-average speed by elapsed race time for all three strategies. Speed is zero during rests."
    : "State of charge by elapsed race time for all three strategies, with a dashed 15 percent reserve line.");
  historyButtons.forEach((item) => item.setAttribute("aria-pressed", String(item === button)));
  drawHistory();
}));
slider.addEventListener("input", drawHistory);
download.addEventListener("click", () => downloadFile(current.csv, "solar-race-trials.csv"));
new ResizeObserver(drawChart).observe(chart);
new ResizeObserver(drawHistory).observe(battery);
try {
  render(await readRun({ reuse: true }));
} catch (error) {
  feedback.textContent = error.message;
  feedback.classList.add("error");
}
