import { carColors, drawCruiserAtlas, loadTerrain } from "./atlas-map.mjs";
import { routes } from "./map-data.mjs";

const $ = (id) => document.getElementById(id);
const play = $("record-play");
const timeline = $("record-time");
const canvas = $("record-map");
const raceDate = new Intl.DateTimeFormat("en-AU", { timeZone: "Australia/Darwin", day: "numeric", month: "short" });
const raceTime = new Intl.DateTimeFormat("en-AU", { timeZone: "Australia/Darwin", hourCycle: "h23",
  hour: "2-digit", minute: "2-digit", second: "2-digit" });
const light = { east: -.4, north: .55, elevation: 45 };
const kilometres = routes.outback.points.map((_, index) => index);
const colors = { ...carColors, dust: "#8a5150", gaia: "#6c5189", archimede: "#776f66" };
let archive;
let checkpoints;
let terrain;
let timer;
let selected = "sunswift";
const rows = new Map();

function formatDate(instant) {
  return raceDate.format(new Date(instant));
}

function formatTime(instant) {
  return raceTime.format(new Date(instant));
}

function createRows() {
  for (const team of archive.teams) {
    const row = $("record-rows").insertRow();
    row.insertCell().textContent = team.number;
    const car = document.createElement("button");
    car.type = "button";
    car.className = "car-select";
    car.style.setProperty("--car-color", colors[team.id]);
    car.textContent = team.car;
    car.addEventListener("click", () => {
      selected = team.id;
      render();
    });
    row.insertCell().append(car);
    row.insertCell();
    row.insertCell();
    rows.set(team.id, { row, car });
  }
  const days = new Set();
  for (const [index, event] of archive.events.entries()) {
    const row = $("record-log").insertRow();
    const jump = document.createElement("button");
    jump.className = "record-jump";
    jump.type = "button";
    jump.textContent = `${formatDate(event.at)} · ${formatTime(event.at)}`;
    jump.addEventListener("click", () => seek(index));
    row.insertCell().append(jump);
    row.insertCell().textContent = archive.teams.find(({ id }) => id === event.id).car;
    row.insertCell().textContent = archive.stops[event.stop].name;
    row.insertCell().textContent = event.status;
    const source = document.createElement("a");
    source.href = archive.sources[event.source];
    source.textContent = event.source === "final" ? "Official results" : "Published timing snapshot";
    row.insertCell().append(source);
    const day = formatDate(event.at);
    if (!days.has(day)) {
      days.add(day);
      const item = document.createElement("li");
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = day;
      button.addEventListener("click", () => seek(index));
      item.append(button);
      $("record-days").append(item);
    }
  }
}

function checkpointsThrough(events, index) {
  return new Map(events.slice(0, index + 1).map((event) => [event.id, event]));
}

function render() {
  if (!archive) return;
  const index = Number(timeline.value);
  const event = archive.events[index];
  const time = `${formatDate(event.at)} · ${formatTime(event.at)} ACST`;
  $("record-clock").textContent = time;
  timeline.setAttribute("aria-valuetext", time);
  timeline.style.setProperty("--progress", `${index / Math.max(1, archive.events.length - 1) * 100}%`);
  const latest = checkpointsThrough(archive.events, index);
  const racers = [];
  for (const team of archive.teams) {
    const last = latest.get(team.id);
    const { row, car } = rows.get(team.id);
    const stop = last ? archive.stops[last.stop] : null;
    car.setAttribute("aria-pressed", String(team.id === selected));
    row.cells[2].textContent = stop?.name ?? "—";
    row.cells[3].textContent = last ? `${formatDate(last.at)} ${formatTime(last.at)}` : "—";
    if (team.id === selected) {
      const officialTime = last?.source === "final" && team.event_hours !== null
        ? ` · ${team.event_hours.toFixed(2)} event h` : "";
      const distance = stop?.km > 0 ? ` · ${stop.km.toLocaleString("en-AU")} km` : "";
      $("record-detail").textContent = `${team.car} · ${last?.status ?? "Awaiting a published checkpoint"}${distance}${officialTime}`;
    }
    if (stop && last.status !== "Did not start") {
      racers.push({ id: team.id, index: racers.length, paint: colors[team.id],
        state: { distance: stop.waypoint, phase: "logged" } });
    }
  }
  drawCruiserAtlas(canvas, { kilometres, stops: checkpoints, racers,
    active: racers.find(({ id }) => id === selected), follow: false, sun: light, terrain, motion: 0 });
}

function pause() {
  clearInterval(timer);
  timer = undefined;
  play.classList.remove("is-playing");
  play.setAttribute("aria-label", "Play recorded checkpoints");
}

function seek(index) {
  pause();
  timeline.value = index;
  render();
}

play.addEventListener("click", () => {
  if (timer !== undefined) {
    pause();
    return;
  }
  if (timeline.value === timeline.max) seek(0);
  play.classList.add("is-playing");
  play.setAttribute("aria-label", "Pause recorded checkpoints");
  timer = setInterval(() => {
    timeline.stepUp();
    render();
    if (timeline.value === timeline.max) pause();
  }, 950);
});
timeline.addEventListener("input", () => seek(timeline.value));
new ResizeObserver(render).observe(canvas);
try {
  const response = await fetch("./bwsc-2025.json");
  if (!response.ok) throw new Error("The 2025 checkpoint archive is unavailable.");
  const data = await response.json();
  terrain = await loadTerrain();
  archive = data;
  checkpoints = archive.stops.map(({ waypoint, ...stop }) => ({ ...stop, leg: waypoint - 1 }));
  createRows();
  timeline.max = archive.events.length - 1;
  timeline.disabled = false;
  play.disabled = false;
  $("record-status").textContent = `${archive.events.length} published records · 8 entries`;
  render();
} catch (error) {
  $("record-status").textContent = error.message;
}
