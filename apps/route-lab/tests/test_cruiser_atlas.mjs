import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { drawCruiserAtlas, geographicPosition, sunAt } from "../web/atlas-map.mjs";
import { fleetRows } from "../web/fleet-data.mjs";
import { routeBook, traceAt } from "../web/replay.mjs";

const read = (name) => JSON.parse(readFileSync(new URL(`../web/${name}`, import.meta.url)));
const { teams, horizon_hours: horizon, solar, points } = read("atlas-demo.json");
const kilometres = [0, ...points.map(({ km }) => km)];
const darwin = geographicPosition(kilometres, 0);
assert.deepEqual(geographicPosition(kilometres, kilometres.at(-1)), [138.6, -34.93]);
const midday = sunAt(solar, 4, darwin);
const morning = sunAt(solar, 0, darwin);
const night = sunAt(solar, 16, darwin);
assert.ok(midday.elevation > morning.elevation && morning.east > 0 && night.elevation < 0);
assert.ok(sunAt(solar, 9, darwin).east < 0);
assert.ok(Math.abs(Math.hypot(midday.east, midday.north, midday.up) - 1) < 1e-10);
assert.equal(fleetRows(teams, 0, traceAt).length, 5);
const finalStates = fleetRows(teams, horizon, traceAt);
assert.ok(finalStates.every(({ state }, index) =>
  index === 0 || state.distance <= finalStates[index - 1].state.distance));

globalThis.window = { devicePixelRatio: 1 };
const layers = [];
const labels = [];
const gradient = () => ({ addColorStop() {} });
const ctx = new Proxy({
  createLinearGradient: gradient,
  createRadialGradient: gradient,
  drawImage: (_texture, _x, y) => { layers.push(y); },
  fillText: (text, x, y) => {
    assert.ok(Number.isFinite(x) && Number.isFinite(y));
    labels.push(text);
  },
  measureText: (text) => ({ width: text.length * 6 }),
}, { get: (target, method) => target[method] ?? (() => {}) });
const canvas = { clientWidth: 1120, clientHeight: 760, getContext: () => ctx };
const control = teams[0].legs.find(({ control_hours }) => control_hours > 0);
const racers = fleetRows(teams, control.race_arrive_hours + .1, traceAt);
const active = racers.find(({ id }) => id === "sunswift");
const scene = { kilometres, stops: routeBook(points, "Darwin"), racers, active, sun: morning,
  terrain: { naturalWidth: 1120, naturalHeight: 4480 }, follow: false, motion: 0 };

assert.ok(drawCruiserAtlas(canvas, scene) > 0);
assert.ok(labels.includes("REST") && labels.includes("Adelaide"));
assert.ok(layers.includes(896));
layers.length = 0;
assert.ok(drawCruiserAtlas(canvas, { ...scene, sun: sunAt(solar, 9, darwin), follow: true }) > 0);
assert.ok(layers.includes(1792));
