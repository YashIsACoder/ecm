"use strict";

import { routes } from "./map-data.mjs";
import { drawBattery, drawRouteMap, policyStyles, traceAt } from "./replay.mjs";

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
const readouts = document.querySelector("#policy-readouts");
const map = document.querySelector("#route-map");
const battery = document.querySelector("#battery");
const mapHeading = document.querySelector("#route-map-heading");
const mapKind = document.querySelector("#map-kind");
const mapCaption = document.querySelector("#map-caption");
const paired = document.querySelector("#paired");
const diagnostics = document.querySelector("#trial-diagnostics");
const energyPlace = document.querySelector("#energy-place");
const energyBreakdown = document.querySelector("#energy-breakdown");
const logPolicy = document.querySelector("#log-policy");
const traceLog = document.querySelector("#trace-log");
const traceTrial = document.querySelector("#trace-trial");
const trialCount = document.querySelector("#trials");
const endpoint = document.querySelector("#route-lab").dataset.endpoint;
const recordedMode = new URLSearchParams(location.search).get("replay") === "1";
const numbers = new Intl.NumberFormat("en-AU", { maximumFractionDigits: 0 });

const labels = { steady: "Steady", push: "Push", "reserve-first": "Reserve-first" };
const presets = { reference: "5.5", sr7: "38", sr8: "45" };
let current;
let busy = false;
let timer;
let historyKey;

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

function render(data, settings, recorded = false) {
  current = data;
  summary.replaceChildren();
  paired.replaceChildren();
  diagnostics.replaceChildren();
  for (const policy of data.policies) {
    const row = namedRow(summary, policy.name);
    addCell(row, percent(policy.finishes / data.trials), policy.completion_ci.map(percent).join("–"));
    addCell(row, percent(policy.reserve_breaches / data.trials), policy.reserve_ci.map(percent).join("–"));
    addCell(row, policy.median_hours === null ? "—" : `${policy.median_hours.toFixed(2)} h`);
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

function resetReplay() {
  pauseReplay();
  replay.hidden = false;
  historyKey = undefined;
  const { draw, logs, route } = current.trace;
  const mapRoute = routes[route];
  mapHeading.textContent = mapRoute.title;
  mapKind.textContent = mapRoute.geographic ? "CORRIDOR MAP" : "SCHEMATIC";
  mapCaption.textContent = mapRoute.geographic
    ? "Approximate checkpoints on the synthetic course. Coastline by Natural Earth."
    : "Illustrative layout for the fictional rolling-country route.";
  drawLabel.textContent = `DRAW ${draw} OF ${numbers.format(current.trials)}`;
  leg.max = Math.max(1e-6, ...Object.values(logs).map((events) => events.at(-1).elapsed_hours));
  leg.value = 0;
  readouts.replaceChildren();
  for (const name of Object.keys(labels)) {
    const row = document.createElement("button");
    row.type = "button";
    row.className = "policy-readout";
    row.dataset.policy = name;
    const label = document.createElement("span");
    label.textContent = labels[name];
    const meta = document.createElement("small");
    row.append(label, meta);
    row.addEventListener("click", () => {
      logPolicy.value = name;
      renderReplay();
    });
    readouts.append(row);
  }
  renderReplay();
}

function renderReplay() {
  if (!current || replay.hidden) return;
  const hour = Number(leg.value);
  const horizon = Number(leg.max);
  const selected = logPolicy.value;
  const positions = Object.fromEntries(
    Object.entries(current.trace.logs).map(([name, events]) => [name, traceAt(events, hour)]),
  );
  legLabel.textContent = `${hour.toFixed(1)} / ${horizon.toFixed(1)} driving h`;
  leg.setAttribute("aria-valuetext", legLabel.textContent);
  for (const row of readouts.children) {
    const state = positions[row.dataset.policy];
    row.setAttribute("aria-pressed", String(row.dataset.policy === selected));
    const charge = state.depleted ? "depleted" : `${state.soc.toFixed(0)}% SOC`;
    row.lastElementChild.textContent = `${numbers.format(state.distance)} km · ${charge}${state.done && !state.depleted ? " · finished" : ""}`;
  }
  drawRouteMap(map, current.trace, positions, selected);
  drawBattery(battery, current.trace.logs, hour, horizon, positions);
  const key = `${selected}-${positions[selected].reached}`;
  if (key === historyKey) return;
  historyKey = key;
  renderHistory(current.trace, selected, positions[selected].reached);
}

function renderHistory({ points, logs }, selected, count) {
  const events = logs[selected].slice(0, count);
  traceLog.replaceChildren();
  renderEnergy(points, events.at(-1));
  if (!events.length) {
    const item = document.createElement("li");
    item.textContent = "Advance through the draw to log each completed leg.";
    traceLog.append(item);
  }
  for (const event of events) {
    const point = points[event.leg];
    const item = document.createElement("li");
    item.classList.toggle("depleted", Boolean(event.depleted));
    const heading = document.createElement("strong");
    heading.textContent = `${event.elapsed_hours.toFixed(2)} h · Day ${point.day} · ${event.depleted ? "Stopped before " : ""}${point.name}`;
    const details = document.createElement("span");
    const parked = event.parked_wh ? ` · parked solar ${(event.parked_wh / 1000).toFixed(2)} kWh` : "";
    details.textContent = `${event.speed_kmh.toFixed(0)} km/h target · ${event.soc_pct.toFixed(1)}% SOC · ` +
      `${event.irradiance_w_m2.toFixed(0)} W/m² · solar ${percent(event.solar_factor)} of clear sky · ` +
      `headwind ${event.headwind_mps.toFixed(1)} m/s · grade ${point.grade_pct.toFixed(2)}% · ` +
      `rolling ×${event.rolling_factor.toFixed(2)} · DC draw ${(event.demand_wh / 1000).toFixed(2)} kWh · ` +
      `driving solar ${(event.solar_wh / 1000).toFixed(2)} kWh${parked}`;
    item.append(heading, details);
    traceLog.append(item);
  }
  traceLog.scrollTop = traceLog.scrollHeight;
}

function energyList(title, terms) {
  const section = document.createElement("div");
  const heading = document.createElement("h4");
  heading.textContent = title;
  const list = document.createElement("dl");
  for (const [name, value] of terms) {
    const label = document.createElement("dt");
    label.textContent = name;
    const amount = document.createElement("dd");
    amount.textContent = value === null ? "—" : Math.abs(value) < 100
      ? `${Math.abs(value) < .05 ? "0.0" : value.toFixed(1)} Wh`
      : `${(value / 1000).toFixed(2)} kWh`;
    list.append(label, amount);
  }
  section.append(heading, list);
  return section;
}

function renderEnergy(points, event) {
  energyBreakdown.replaceChildren();
  if (!event) {
    energyPlace.textContent = labels[logPolicy.value];
    energyBreakdown.textContent = "Play or scrub past the first leg to see its energy balance.";
    return;
  }
  energyPlace.textContent = `${labels[logPolicy.value]} · ${points[event.leg].name}`;
  const wheels = [
    ["Aerodynamic drag", event.drag_wh], ["Rolling resistance", event.rolling_wh],
    ["Gravity / grade", event.climb_wh], ["Kinetic-energy change", event.kinetic_wh],
  ];
  const work = wheels.map(([, value]) => value);
  wheels.push(["Net wheel work", work.every(Number.isFinite) ? work.reduce((sum, value) => sum + value, 0) : null]);
  const known = event.traction_wh !== null && event.recovered_wh !== null;
  const dc = [
    ["Motoring draw", known ? event.traction_wh + event.recovered_wh : null],
    ["Regeneration credit", known ? -event.recovered_wh : null],
    ["Auxiliaries", event.auxiliary_wh], ["DC demand", event.demand_wh],
    ["Driving solar credit", -event.solar_wh],
    ["Parked solar credit", -event.parked_wh], ["Energy spilled", event.spilled_wh],
    ["Net battery draw", event.demand_wh - event.solar_wh - event.parked_wh + event.spilled_wh],
  ];
  energyBreakdown.append(energyList("At the wheels", wheels), energyList("DC and battery", dc));
}

function pauseReplay() {
  cancelAnimationFrame(timer);
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
    renderReplay();
  }
  if (Number(leg.max) === 0) {
    return;
  }
  play.textContent = "Pause replay";
  const start = performance.now();
  const from = Number(leg.value);
  const end = Number(leg.max);
  const advance = (now) => {
    const hours = Math.min(end, from + (now - start) * end / 20_000);
    leg.value = hours;
    renderReplay();
    if (hours === end) {
      pauseReplay();
    } else {
      timer = requestAnimationFrame(advance);
    }
  };
  timer = requestAnimationFrame(advance);
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
  renderReplay();
});
logPolicy.addEventListener("change", renderReplay);

new ResizeObserver(drawChart).observe(chart);
const replayResize = new ResizeObserver(renderReplay);
replayResize.observe(map);
replayResize.observe(battery);
runExperiment();
