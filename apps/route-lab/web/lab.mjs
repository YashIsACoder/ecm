"use strict";

import { routes } from "./map-data.mjs";
import { checkpointHour, drawRouteMap, playbackTimeline, raceClock, replaySpeed, routeBook, skyCondition, traceAt, translateTime } from "./replay.mjs";

import { mountSetup } from "./configure.mjs";
import { settingsFromQuery, setupQuery } from "./scenario.mjs";
import { downloadFile, numbers, percent, policyLabels as labels, readRun, runQuery, runSummary } from "./run-data.mjs";

const feedback = document.querySelector("#run-feedback");
const replay = document.querySelector("#replay");
const play = document.querySelector("#play");
const nextRest = document.querySelector("#next-rest");
const nextCharge = document.querySelector("#next-charge");
const raceSpec = document.querySelector("#race-spec");
const raceDay = document.querySelector("#race-day");
const clockDisplay = document.querySelector("#race-clock");
const activePolicy = document.querySelector("#active-policy");
const phaseDisplay = document.querySelector("#race-phase");
const placeDisplay = document.querySelector("#race-place");
const solarDisplay = document.querySelector("#race-solar");
const cloudDisplay = document.querySelector("#race-cloud");
const windDisplay = document.querySelector("#race-wind");
const speedDisplay = document.querySelector("#race-speed");
const paceDisplay = document.querySelector("#race-target");
const powerDisplay = document.querySelector("#race-output");
const planDisplay = document.querySelector("#race-plan");
const saveScenario = document.querySelector("#save-scenario");
const mapSky = document.querySelector("#map-sky");
const rainDisplay = document.querySelector("#race-rain");
const wetnessDisplay = document.querySelector("#race-wetness");
const capDisplay = document.querySelector("#race-cap");
const rate = document.querySelector("#replay-rate");
const clockMode = document.querySelector("#replay-clock-mode");
const leg = document.querySelector("#leg");
const legLabel = document.querySelector("#leg-label");
const drawLabel = document.querySelector("#draw-label");
const readouts = document.querySelector("#policy-readouts");
const map = document.querySelector("#route-map");
const mapHeading = document.querySelector("#route-map-heading");
const controlStops = document.querySelector("#control-stops");
const cameraButtons = [...document.querySelectorAll("[data-camera]")];
const mapProgress = document.querySelector("#map-progress");
const chargeMeter = document.querySelector("#selected-charge");
const chargeLabel = document.querySelector("#selected-charge-label");
const rainChance = document.querySelector("#race-rain-chance");
const mapCaption = document.querySelector("#map-caption");
const energyPlace = document.querySelector("#energy-place");
const energyBreakdown = document.querySelector("#energy-breakdown");
const logPolicy = document.querySelector("#log-policy");
const traceLog = document.querySelector("#trace-log");

let current;
let timer;
let historyKey;
let timeline;
let scenario;
let camera = "overview";
let stops;
const phaseLabels = { drive: "On course", control: "REST", overnight: "Parked", finish: "Finished", depleted: "Depleted" };

function resetReplay() {
  pauseReplay();
  replay.hidden = false;
  historyKey = undefined;
  const { draw, logs, route } = current.trace;
  const mapRoute = routes[route];
  mapHeading.textContent = mapRoute.title;
  mapCaption.textContent = mapRoute.geographic
    ? "Gold markers indicate control stops. Approximate route geometry. Coastline by Natural Earth."
    : "Gold markers indicate control stops on this fictional route.";
  const context = drawLabel.textContent;
  drawLabel.textContent = `Draw ${draw} · ${context}`;
  timeline = playbackTimeline(logs);
  leg.max = timeline.at(-1).hour;
  leg.disabled = Number(leg.max) === 0;
  play.disabled = leg.disabled;
  leg.value = 0;
  stops = routeBook(current.trace.points, mapRoute.start).map(({ name, km, leg: stopLeg }) => {
    const item = document.createElement("li");
    const button = document.createElement("button");
    button.type = "button";
    const place = document.createElement("strong");
    place.textContent = name;
    const distance = document.createElement("small");
    distance.textContent = `${numbers.format(km)} km`;
    button.append(place, distance);
    button.addEventListener("click", () => jumpTo(checkpointHour(current.trace.logs[logPolicy.value], stopLeg)));
    item.append(button);
    return { item, button, leg: stopLeg };
  });
  controlStops.replaceChildren(...stops.map(({ item }) => item));
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
  const { day, clock, night } = raceClock(hour);
  raceDay.textContent = `Day ${day}`;
  clockDisplay.textContent = clock;
  legLabel.textContent = `${hour.toFixed(1)} / ${horizon.toFixed(1)} h`;
  leg.setAttribute("aria-valuetext", `Day ${day}, ${clock}. ${hour.toFixed(1)} race hours elapsed.`);
  const analytics = new URL(analyticsLinks[1].href);
  if (hour > 0) analytics.searchParams.set("at", hour.toFixed(6));
  else analytics.searchParams.delete("at");
  if (analytics.href !== analyticsLinks[1].href) analyticsLinks[1].href = analytics.href;
  renderTelemetry(selected, positions[selected]);
  nextRest.disabled = nextControlHour() === undefined;
  nextCharge.disabled = nextChargeHour() === undefined;
  for (const row of readouts.children) {
    const state = positions[row.dataset.policy];
    row.setAttribute("aria-pressed", String(row.dataset.policy === selected));
    const charge = state.depleted ? "depleted" : `${state.soc.toFixed(0)}% SOC`;
    const pace = state.phase === "drive" ? `${numbers.format(replaySpeed(state))} km/h` : phaseLabels[state.phase];
    row.lastElementChild.textContent = `${numbers.format(state.distance)} km · ${charge} · ${pace}`;
  }
  const remaining = current.distances_km.at(-1) - positions[selected].distance;
  mapProgress.textContent = `${numbers.format(remaining)} km to finish`;
  drawRouteMap(map, current.trace, positions, selected, night, hour, camera);
  for (const { button, leg: stopLeg } of stops) {
    const arrival = checkpointHour(current.trace.logs[selected], stopLeg);
    button.disabled = arrival === undefined;
    const state = positions[selected];
    button.dataset.passed = String(arrival !== undefined && hour >= arrival);
    if (state.event.leg === stopLeg && (state.phase === "control" || state.phase === "finish")) {
      button.setAttribute("aria-current", "location");
    } else {
      button.removeAttribute("aria-current");
    }
  }
  const key = `${selected}-${positions[selected].reached}`;
  if (key === historyKey) return;
  historyKey = key;
  renderHistory(current.trace, selected, positions[selected].reached);
}

function nextControlHour() {
  return current.trace.logs[logPolicy.value].find((event) =>
    event.control_hours > 0 && event.race_arrive_hours > Number(leg.value) + 1e-6,
  )?.race_arrive_hours;
}

function nextChargeHour() {
  const events = current.trace.logs[logPolicy.value];
  const now = Number(leg.value);
  const arrival = events.find((event, index) =>
    event.overnight_hours > 0 && events[index + 1]?.external_charge_wh > 0 &&
    event.race_arrive_hours + event.control_hours > now + 1e-6);
  return arrival ? arrival.race_arrive_hours + arrival.control_hours + 1e-6 : undefined;
}

function renderTelemetry(policy, state) {
  const { event, phase, restRemaining, externalChargeWh = 0 } = state;
  const place = current.trace.points[event.leg].name;
  activePolicy.textContent = labels[policy];
  phaseDisplay.textContent = externalChargeWh > 0 ? "Overnight top-up" : phaseLabels[phase];
  phaseDisplay.dataset.phase = phase;
  const remaining = Math.max(0, Math.ceil((restRemaining ?? 0) * 60 - 1e-4));
  const reasons = {
    drive: `Toward ${place}`,
    control: `${place} control · ${remaining} min until release`,
    overnight: `${place} · departure at ${raceClock(Number(leg.value) + (restRemaining ?? 0) + 1e-6).clock}`,
    finish: `Finished at ${place}`,
    depleted: `Battery depleted before ${place}`,
  };
  const topUp = externalChargeWh > 0 ? ` · ${(externalChargeWh / 1000).toFixed(2)} kWh before departure` : "";
  placeDisplay.textContent = reasons[phase] + topUp;
  const driving = phase === "drive";
  const duration = event.race_arrive_hours - event.race_depart_hours;
  speedDisplay.textContent = `${numbers.format(replaySpeed(state))} km/h`;
  paceDisplay.textContent = driving ? `${numbers.format(event.speed_kmh)} km/h` : "—";
  chargeMeter.value = state.soc;
  chargeLabel.textContent = `${state.soc.toFixed(1)}%`;
  powerDisplay.textContent = driving && duration > 0 ? `${numbers.format(event.solar_wh / duration)} W` : "—";
  planDisplay.hidden = !driving || event.planned_arrival_soc_pct == null;
  if (!planDisplay.hidden) {
    const target = event.planned_arrival_soc_pct;
    planDisplay.textContent = target > 100 ? "Arrival energy target exceeds pack capacity"
      : `Planned arrival charge ${target.toFixed(1)}%`;
  }
  const weatherAvailable = phase === "drive" || phase === "control";
  solarDisplay.textContent = weatherAvailable ? `${event.irradiance_w_m2.toFixed(0)} W/m²` : "—";
  cloudDisplay.textContent = weatherAvailable ? percent(event.solar_factor) : "—";
  const rain = event.rain_mm_h ?? 0;
  const wetness = event.road_wetness ?? 0;
  const sky = weatherAvailable ? skyCondition(event) : phaseLabels[phase];
  mapSky.parentElement.dataset.rain = String(weatherAvailable && rain > 0);
  mapSky.textContent = sky;
  const rainRate = rain < .05 ? "<0.1" : rain.toFixed(1);
  rainDisplay.textContent = !weatherAvailable ? "—" : rain > 0 ? `${rainRate} mm/h` : "Dry";
  wetnessDisplay.textContent = weatherAvailable ? percent(wetness) : "—";
  const rainProbability = event.rain_probability ?? 0;
  const probabilityLabel = rainProbability > 0 && rainProbability < .001 ? "<0.1%" : percent(rainProbability);
  rainChance.textContent = weatherAvailable ? probabilityLabel : "—";
  capDisplay.textContent = driving ? `${numbers.format(event.effective_pace_cap_kmh ?? event.speed_kmh)} km/h` : "—";
  const wind = event.headwind_mps;
  windDisplay.textContent = weatherAvailable
    ? `${Math.abs(wind).toFixed(1)} m/s ${wind < 0 ? "tail" : "head"}` : "—";
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
    const { day, clock } = raceClock(event.race_arrive_hours);
    const stop = event.control_hours ? ` · REST ${Math.round(event.control_hours * 60)} min` : "";
    const night = event.overnight_hours ? ` · overnight ${event.overnight_hours.toFixed(1)} h` : "";
    heading.textContent = `Day ${day} ${clock} · ${event.depleted ? "Stopped before " : ""}${point.name}${stop}${night}`;
    const details = document.createElement("span");
    const wet = event.road_wetness > 0 ? ` · wetness ${percent(event.road_wetness)}` : "";
    const rain = event.rain_mm_h > 0 ? ` · rain ${event.rain_mm_h.toFixed(1)} mm/h` : "";
    const topUp = event.external_charge_wh > 0 ? ` · overnight top-up ${(event.external_charge_wh / 1000).toFixed(2)} kWh` : "";
    const parked = event.parked_wh ? ` · parked solar ${(event.parked_wh / 1000).toFixed(2)} kWh` : "";
    const target = event.planned_arrival_soc_pct == null ? "" : ` / ${event.planned_arrival_soc_pct.toFixed(1)}% planned`;
    details.textContent = `${(event.mean_speed_kmh ?? event.speed_kmh).toFixed(1)} km/h avg · ${event.speed_kmh.toFixed(1)} km/h target · ${event.soc_pct.toFixed(1)}% SOC${target} · ` +
      `${event.irradiance_w_m2.toFixed(0)} W/m² · solar ${percent(event.solar_factor)} of clear sky · ` +
      `headwind ${event.headwind_mps.toFixed(1)} m/s${rain}${wet} · grade ${point.grade_pct.toFixed(2)}% · ` +
      `rolling ×${event.rolling_factor.toFixed(2)} · DC draw ${(event.demand_wh / 1000).toFixed(2)} kWh · ` +
      `driving solar ${(event.solar_wh / 1000).toFixed(2)} kWh${parked}${topUp}`;
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
    ["Parked solar credit", -event.parked_wh],
    ["External overnight charge", -(event.external_charge_wh ?? 0)],
    ["Energy spilled", event.spilled_wh],
    ["Net battery draw", event.demand_wh - event.solar_wh - event.parked_wh
      - (event.external_charge_wh ?? 0) + event.spilled_wh],
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
  const schedule = clockMode.value === "uniform"
    ? [{ hour: 0, offset: 0 }, { hour: Number(leg.max), offset: Number(leg.max) }]
    : timeline;
  const offset = translateTime(schedule, Number(leg.value), "hour", "offset");
  const end = schedule.at(-1);
  const duration = 32_000 / Number(rate.value);
  const advance = (now) => {
    const to = Math.min(end.offset, offset + (now - start) * end.offset / duration);
    leg.value = translateTime(schedule, to, "offset", "hour");
    renderReplay();
    if (to === end.offset) {
      pauseReplay();
    } else {
      timer = requestAnimationFrame(advance);
    }
  };
  timer = requestAnimationFrame(advance);
}

const setup = document.querySelector("#setup");
const setupHost = document.querySelector("#setup-host");
const analyticsLinks = [
  document.querySelector("#analytics-nav"), document.querySelector("#analytics-link"),
];
let setupLoading = false;

async function openSetup() {
  if (!setup.open || setupLoading || setupHost.querySelector("form")) return;
  setupLoading = true;
  try {
    const response = await fetch("./configure.html");
    if (!response.ok) throw new Error("Setup could not be loaded.");
    const page = new DOMParser().parseFromString(await response.text(), "text/html");
    const fields = document.importNode(page.querySelector(".setup-fields"), true);
    mountSetup(fields, scenario?.settings ?? settingsFromQuery(location.search));
    setupHost.replaceChildren(fields);
  } catch {
    const fallback = setupHost.querySelector("a");
    fallback.href = `./configure.html${setupQuery(scenario?.settings ?? settingsFromQuery(location.search))}`;
    fallback.textContent = "Open configuration page";
  } finally {
    setupLoading = false;
  }
}

setup.addEventListener("toggle", openSetup);
if (location.hash === "#setup") setup.open = true;

async function runExperiment() {
  try {
    const { data, settings, recorded } = await readRun();
    current = data;
    const cruiser = data.energy_rules === "bwsc_2025_cruiser";
    nextCharge.hidden = !cruiser;
    if (cruiser) {
      const pack = settings.pack_kwh === null ? "55 MJ pack (15.28 kWh)" : `${settings.pack_kwh} kWh pack`;
      raceSpec.textContent = `2025 Cruiser · ${pack} · Overnight charging`;
    }
    scenario = { version: 1, settings, ...data.reproduce };
    analyticsLinks.forEach((link) => {
      link.href = `./analytics.html${runQuery(settings, recorded)}`;
    });
    drawLabel.textContent = (recorded ? "RECORDED · " : "") + runSummary(data, settings);
    saveScenario.disabled = false;
    feedback.hidden = true;
    resetReplay();
  } catch (error) {
    feedback.textContent = error.message;
    feedback.classList.add("error");
  }
}

document.querySelector("#download-trace").addEventListener("click", () => {
  downloadFile(current.trace.csv, `solar-race-draw-${current.trace.draw}.csv`);
});
saveScenario.addEventListener("click", () => {
  downloadFile(JSON.stringify(scenario, null, 2) + "\n", "solar-race-setup.json", "application/json");
});
play.addEventListener("click", toggleReplay);
for (const control of [rate, clockMode]) {
  control.addEventListener("change", () => {
    if (timer === undefined) return;
    pauseReplay();
    toggleReplay();
  });
}
function jumpTo(hour) {
  if (hour === undefined) return;
  pauseReplay();
  leg.value = hour;
  renderReplay();
}

nextRest.addEventListener("click", () => jumpTo(nextControlHour()));
nextCharge.addEventListener("click", () => jumpTo(nextChargeHour()));
cameraButtons.forEach((button) => button.addEventListener("click", () => {
  camera = button.dataset.camera;
  cameraButtons.forEach((mode) => mode.setAttribute("aria-pressed", String(mode === button)));
  renderReplay();
}));
leg.addEventListener("input", () => {
  pauseReplay();
  renderReplay();
});
logPolicy.addEventListener("change", renderReplay);

new ResizeObserver(renderReplay).observe(map);
runExperiment();
