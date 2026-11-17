import { carColors, drawCruiserAtlas, geographicPosition, sunAt } from "./atlas-map.mjs";
import { fleetRows, loadFleet } from "./fleet-data.mjs";
import { checkpointHour, playbackTimeline, raceClock, replaySpeed, routeBook, skyCondition, traceAt, translateTime } from "./replay.mjs";
import { defaultSettings } from "./scenario.mjs";

const $ = (id) => document.getElementById(id);
const canvas = $("atlas-map");
const play = $("atlas-play");
const slider = $("atlas-time");
const rate = $("atlas-rate");
const status = $("atlas-status");
const camera = $("map-camera");
const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");
const query = new URLSearchParams(location.search);
const live = query.get("view") === "race";
const number = new Intl.NumberFormat("en-AU", { maximumFractionDigits: 0 });
let report;
let kilometres;
let stops;
let timeline;
let terrain;
let selected = "sunswift";
let frame;
let motion = 0;
let order = "";
let scaleWidth = 0;
let scaleDistance = 0;
const rows = new Map();

function setText(node, text) {
  if (node.textContent !== text) node.textContent = text;
}

function formatTime(hour) {
  const { day, clock } = raceClock(hour);
  const seconds = String(Math.floor((hour % 1) * 3600 + 1e-6) % 60).padStart(2, "0");
  return { day, time: `${clock}:${seconds}` };
}

function makeRows(teams) {
  for (const team of teams) {
    const row = document.createElement("tr");
    for (let column = 0; column < 6; column += 1) row.insertCell();
    const car = document.createElement("button");
    car.type = "button";
    car.className = "car-select";
    car.style.setProperty("--car-color", carColors[team.id]);
    car.textContent = team.car;
    car.setAttribute("aria-label", `${team.car}, ${team.team}`);
    car.addEventListener("click", () => {
      selected = team.id;
      render();
    });
    row.cells[1].append(car);
    rows.set(team.id, { row, car });
  }
}

function updateScale(pxPerKm) {
  if (pxPerKm <= 0) return;
  const scale = $("map-scale");
  const maxWidth = Math.min(300, scale.parentElement.clientWidth * .8);
  const km = [1000, 500, 200, 100, 50].find((value) => value * pxPerKm <= maxWidth) ?? 20;
  const width = Math.round(km * pxPerKm);
  if (km !== scaleDistance) {
    scaleDistance = km;
    scale.replaceChildren(...[0, .25, .5, .75, 1].map((part) => {
      const label = document.createElement("span");
      label.textContent = number.format(km * part) + (part === 1 ? " km" : "");
      return label;
    }));
  }
  if (width !== scaleWidth) {
    scaleWidth = width;
    scale.style.width = `${width}px`;
  }
}

function updateStandings(racers) {
  const nextOrder = racers.map(({ id }) => id).join(",");
  if (order !== nextOrder) {
    const focus = document.activeElement;
    $("standings").append(...racers.map(({ id }) => rows.get(id).row));
    if (focus?.classList.contains("car-select") && focus !== document.activeElement) focus.focus();
    order = nextOrder;
  }
  racers.forEach(({ id, state }, rank) => {
    const { row, car } = rows.get(id);
    const legHours = state.event.race_arrive_hours - state.event.race_depart_hours;
    const solarKw = state.phase === "drive" && legHours > 0 ? state.event.solar_wh / legHours / 1000 : 0;
    car.setAttribute("aria-pressed", String(id === selected));
    setText(row.cells[0], String(rank + 1));
    setText(row.cells[2], number.format(state.distance));
    setText(row.cells[3], replaySpeed(state).toFixed(1));
    setText(row.cells[4], solarKw.toFixed(2));
    setText(row.cells[5], state.soc.toFixed(0));
  });
}

function render() {
  if (!report) return;
  const hour = Number(slider.value);
  const { day, time } = formatTime(hour);
  setText($("clock-text"), `${time} · Race day ${day}`);
  const end = formatTime(report.horizon_hours);
  setText($("atlas-duration"), `D${day} ${time} / D${end.day} ${end.time}`);
  slider.style.setProperty("--progress", `${hour / report.horizon_hours * 100}%`);
  slider.setAttribute("aria-valuetext", `Race day ${day}, ${time} ACST`);

  const racers = fleetRows(report.teams, hour, traceAt);
  const active = racers.find(({ id }) => id === selected);
  const { event, phase } = active.state;
  const phaseText = {
    drive: skyCondition(event), control: "REST", overnight: active.state.externalChargeWh > 0 ? "Overnight top-up" : "Parked",
    finish: "Finished", depleted: "Battery depleted",
  }[phase];
  setText($("atlas-focus"), `${report.teams[active.index].car} · ${phaseText}`);
  updateStandings(racers);
  const sun = sunAt(report.solar, hour, geographicPosition(kilometres, active.state.distance));
  setText($("sun-angle"), `Sun ${sun.elevation.toFixed(1)}° · AZ ${Math.round(sun.azimuth) % 360}°`);
  const follow = camera.getAttribute("aria-pressed") === "true";
  updateScale(drawCruiserAtlas(canvas, { kilometres, stops, racers, active, sun, terrain, follow,
    motion: reducedMotion.matches ? 0 : motion }));

  for (const [index, stop] of stops.entries()) {
    const button = $("atlas-stops").children[index].firstElementChild;
    const arrival = checkpointHour(report.teams[active.index].legs, stop.leg);
    button.disabled = arrival === undefined;
    if (stop.leg === event.leg && (phase === "control" || phase === "finish")) {
      button.setAttribute("aria-current", "location");
    } else {
      button.removeAttribute("aria-current");
    }
  }
}

function pause() {
  cancelAnimationFrame(frame);
  frame = undefined;
  play.classList.remove("is-playing");
  play.setAttribute("aria-label", "Play race");
}

function toggle() {
  if (frame !== undefined) {
    pause();
    return;
  }
  if (!report) return;
  if (Number(slider.value) >= report.horizon_hours) slider.value = 0;
  const end = timeline.at(-1).offset;
  const offset = translateTime(timeline, Number(slider.value), "hour", "offset");
  const duration = 56_000 / Number(rate.value);
  const start = performance.now();
  let previous = start;
  play.classList.add("is-playing");
  play.setAttribute("aria-label", "Pause race");
  const advance = (now) => {
    const to = Math.min(end, offset + (now - start) * end / duration);
    motion += Math.min(50, Math.max(0, now - previous)) / 1000;
    previous = now;
    slider.value = translateTime(timeline, to, "offset", "hour");
    render();
    if (to >= end) pause();
    else frame = requestAnimationFrame(advance);
  };
  frame = requestAnimationFrame(advance);
}

function makeStops() {
  $("atlas-stops").replaceChildren(...stops.map((stop) => {
    const item = document.createElement("li");
    const jump = document.createElement("button");
    const distance = document.createElement("small");
    jump.type = "button";
    distance.textContent = `${number.format(stop.km)} km`;
    jump.append(stop.name, distance);
    jump.addEventListener("click", () => {
      const team = report.teams.find(({ id }) => id === selected);
      const arrival = checkpointHour(team.legs, stop.leg);
      if (arrival === undefined) return;
      pause();
      slider.value = arrival;
      render();
    });
    item.append(jump);
    return item;
  }));
}

async function start() {
  if (Object.keys(defaultSettings).some((field) => query.has(field)) || location.hash === "#setup") {
    location.replace(`./experiment.html${location.search}${location.hash}`);
    return;
  }
  const fleet = await loadFleet();
  const { points } = fleet;
  kilometres = [0, ...points.map(({ km }) => km)];
  stops = routeBook(points, "Darwin");
  report = fleet;
  timeline = playbackTimeline(Object.fromEntries(fleet.teams.map(({ id, legs }) => [id, legs])));
  slider.max = fleet.horizon_hours;
  slider.value = live ? 0 : Math.min(54, fleet.horizon_hours);
  slider.disabled = false;
  play.disabled = false;
  $("route-distance").textContent = `${number.format(kilometres.at(-1))} km`;
  $("route-stops").textContent = String(stops.length - 2);
  status.textContent = `Simulated positions · draw ${fleet.draw} · seed ${fleet.seed}`;
  makeRows(fleet.teams);
  makeStops();
  const image = new Image();
  image.addEventListener("load", () => {
    terrain = image;
    render();
  });
  image.src = "./atlas-relief.webp";
  render();
  if (live) {
    document.querySelector("nav [aria-current]").removeAttribute("aria-current");
    document.querySelector('nav a[href="./?view=race"]').setAttribute("aria-current", "page");
    if (!reducedMotion.matches) toggle();
  }
}

play.addEventListener("click", toggle);
slider.addEventListener("input", () => {
  pause();
  render();
});
rate.addEventListener("change", () => {
  if (frame === undefined) return;
  pause();
  toggle();
});
camera.addEventListener("click", () => {
  const next = camera.getAttribute("aria-pressed") !== "true";
  camera.setAttribute("aria-pressed", String(next));
  camera.textContent = next ? "Overview" : "Follow car";
  render();
});
reducedMotion.addEventListener("change", render);
new ResizeObserver(render).observe(canvas);
start().catch((error) => { status.textContent = error.message; });
