"use strict";

const form = document.querySelector("#experiment");
const run = document.querySelector("#run");
const status = document.querySelector("#status");
const outcomes = document.querySelector("#outcomes");
const summary = document.querySelector("#summary");
const chart = document.querySelector("#reach");
const download = document.querySelector("#download");
const comparison = document.querySelector("#comparison");
const results = document.querySelector(".results");
const pack = document.querySelector("#pack");
const vehicle = document.querySelector("#vehicle");
const runInputs = document.querySelector("#run-inputs");
const runNote = document.querySelector("#input-note");
const replay = document.querySelector("#replay");
const play = document.querySelector("#play");
const leg = document.querySelector("#leg");
const legLabel = document.querySelector("#leg-label");
const drawLabel = document.querySelector("#draw-label");
const lanes = document.querySelector("#lanes");
const logPolicy = document.querySelector("#log-policy");
const traceLog = document.querySelector("#trace-log");
const traceTrial = document.querySelector("#trace-trial");
const trialCount = document.querySelector("#trials");
const endpoint = document.querySelector("#route-lab").dataset.endpoint;
const recordedMode = new URLSearchParams(location.search).get("replay") === "1";
const numbers = new Intl.NumberFormat("en-AU", { maximumFractionDigits: 0 });

const labels = { steady: "Steady", push: "Push", "reserve-first": "Reserve-first" };
const presets = { reference: "5.5", sr7: "38", sr8: "45" };
const series = [
  { color: "--teal", dash: [] },
  { color: "--rust", dash: [7, 4] },
  { color: "--blue", dash: [2, 4] },
];

let current;
let busy = false;
let timer;

const percent = (value) => `${(100 * value).toFixed(1)}%`;
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

function render(data, settings, recorded = false) {
  current = data;
  summary.replaceChildren();
  for (const policy of data.policies) {
    const row = summary.insertRow();
    const name = document.createElement("th");
    name.scope = "row";
    name.textContent = labels[policy.name];
    row.append(name);
    addCell(row, percent(policy.finishes / data.trials), policy.completion_ci.map(percent).join("–"));
    addCell(row, percent(policy.reserve_breaches / data.trials));
    addCell(row, policy.median_hours === null ? "—" : `${policy.median_hours.toFixed(2)} h`);
  }
  const [delta, low, high] = data.completion_vs_steady["reserve-first"].map(pointDelta);
  comparison.textContent =
    `Reserve-first vs steady: ${delta} percentage points in finish rate (95% paired interval ${low} to ${high}).`;
  status.textContent = `${recorded ? "Recorded run · " : ""}${numbers.format(data.trials)} draws · seed ${settings.seed} · ${settings.vehicle.toUpperCase()} · ${settings.model}`;
  outcomes.hidden = false;
  drawChart();
  resetReplay();
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
  current.policies.forEach((policy, index) => {
    ctx.strokeStyle = color(series[index].color);
    ctx.setLineDash(series[index].dash);
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

function resetReplay() {
  pauseReplay();
  replay.hidden = false;
  const { draw, logs } = current.trace;
  drawLabel.textContent = `DRAW ${draw} OF ${numbers.format(current.trials)}`;
  leg.max = Math.max(...Object.values(logs).map((events) => events.length - 1));
  leg.value = 0;
  lanes.replaceChildren();
  for (const name of Object.keys(labels)) {
    const row = document.createElement("div");
    row.className = "lane";
    row.dataset.policy = name;
    const label = document.createElement("span");
    label.className = "lane-label";
    label.textContent = labels[name];
    const track = document.createElement("div");
    track.className = "lane-track";
    track.setAttribute("aria-hidden", "true");
    const fill = document.createElement("div");
    fill.className = "lane-fill";
    track.append(fill);
    const meta = document.createElement("span");
    meta.className = "lane-meta";
    row.append(label, track, meta);
    lanes.append(row);
  }
  renderLeg();
}

function renderLeg() {
  const step = Number(leg.value);
  const { points, logs } = current.trace;
  const total = current.distances_km.at(-1);
  legLabel.textContent = `Leg ${step + 1} of ${points.length}`;
  leg.setAttribute("aria-valuetext", legLabel.textContent);
  for (const row of lanes.children) {
    const events = logs[row.dataset.policy];
    const event = events[Math.min(step, events.length - 1)];
    row.querySelector(".lane-fill").style.width = `${Math.min(100, 100 * event.distance_km / total)}%`;
    row.querySelector(".lane-meta").textContent =
      `${numbers.format(event.distance_km)} km · ${event.depleted ? "depleted" : `${event.soc_pct.toFixed(0)}% SOC`}`;
  }
  const events = logs[logPolicy.value].slice(0, step + 1);
  traceLog.replaceChildren();
  for (const event of events) {
    const point = points[event.leg];
    const item = document.createElement("li");
    item.classList.toggle("depleted", Boolean(event.depleted));
    const heading = document.createElement("strong");
    heading.textContent = `Day ${point.day} · ${event.depleted ? "Stopped before " : ""}${point.name}`;
    const details = document.createElement("span");
    const parked = event.parked_wh ? ` · parked solar ${(event.parked_wh / 1000).toFixed(2)} kWh` : "";
    details.textContent = `${event.speed_kmh.toFixed(0)} km/h target · ${event.soc_pct.toFixed(1)}% SOC · ` +
      `solar ${percent(event.solar_factor)} of clear sky · signed headwind ${event.headwind_mps.toFixed(1)} m/s · ` +
      `rolling ×${event.rolling_factor.toFixed(2)} · DC draw ${(event.demand_wh / 1000).toFixed(2)} kWh · ` +
      `driving solar ${(event.solar_wh / 1000).toFixed(2)} kWh${parked}`;
    item.append(heading, details);
    traceLog.append(item);
  }
  traceLog.scrollTop = traceLog.scrollHeight;
}

function pauseReplay() {
  clearInterval(timer);
  timer = undefined;
  play.textContent = "Play replay";
}

function toggleReplay() {
  if (timer !== undefined) {
    pauseReplay();
    return;
  }
  if (Number(leg.value) === Number(leg.max)) {
    leg.value = 0;
    renderLeg();
  }
  if (Number(leg.max) === 0) {
    return;
  }
  play.textContent = "Pause replay";
  timer = setInterval(() => {
    leg.value = Number(leg.value) + 1;
    renderLeg();
    if (Number(leg.value) === Number(leg.max)) {
      pauseReplay();
    }
  }, 735);
}

async function loadRecorded() {
  const response = await fetch("./demo.json");
  if (!response.ok) {
    throw new Error("The recorded native run is unavailable.");
  }
  const { settings, result } = await response.json();
  for (const [field, value] of Object.entries(settings)) {
    form.elements.namedItem(field).value = value ?? "";
  }
  runInputs.disabled = true;
  runNote.textContent = "This homepage demo replays one recorded native run. The local lab can run new experiments.";
  render(result, settings, true);
}

async function runExperiment() {
  if (busy) {
    return;
  }
  const values = new FormData(form);
  const settings = Object.fromEntries(values);
  settings.trials = Number(settings.trials);
  settings.trace_trial = Number(settings.trace_trial);
  settings.pack_kwh = settings.pack_kwh === "" ? null : Number(settings.pack_kwh);
  busy = true;
  run.disabled = true;
  download.disabled = true;
  results.setAttribute("aria-busy", "true");
  status.classList.remove("error");
  status.textContent = "Running experiment…";
  pauseReplay();
  try {
    if (recordedMode) {
      await loadRecorded();
      return;
    }
    const response = await fetch(endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(settings),
    });
    if ([404, 405, 501].includes(response.status)) {
      await loadRecorded();
      return;
    }
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "The experiment failed.");
    }
    render(data, settings);
  } catch (error) {
    current = undefined;
    outcomes.hidden = true;
    replay.hidden = true;
    status.textContent = error.message;
    status.classList.add("error");
  } finally {
    busy = false;
    run.disabled = false;
    download.disabled = !current;
    results.setAttribute("aria-busy", "false");
  }
}

form.addEventListener("submit", (event) => {
  event.preventDefault();
  runExperiment();
});
vehicle.addEventListener("change", () => {
  pack.placeholder = `Preset · ${presets[vehicle.value]} kWh`;
});
trialCount.addEventListener("input", () => {
  traceTrial.max = trialCount.value;
  if (Number(traceTrial.value) > Number(trialCount.value)) {
    traceTrial.value = Number(trialCount.value) || 1;
  }
});
function downloadCsv(csv, filename) {
  const url = URL.createObjectURL(new Blob([csv], { type: "text/csv" }));
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
download.addEventListener("click", () => downloadCsv(current.csv, "solar-race-trials.csv"));
document.querySelector("#download-trace").addEventListener("click", () => {
  downloadCsv(current.trace.csv, `solar-race-draw-${current.trace.draw}.csv`);
});
play.addEventListener("click", toggleReplay);
leg.addEventListener("input", () => {
  pauseReplay();
  renderLeg();
});
logPolicy.addEventListener("change", renderLeg);

new ResizeObserver(drawChart).observe(chart);
runExperiment();
