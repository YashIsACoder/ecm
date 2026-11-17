import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { defaultSettings, readSetupFile, settingsFromQuery, setupQuery } from "../web/scenario.mjs";
import { replayContext, withReplayContext } from "../web/run-data.mjs";
import { atlasLighting, checkpointHour, drawRouteMap, routeBook, playbackTimeline, raceClock, replaySpeed, skyCondition, traceAt, translateTime } from "../web/replay.mjs";

const { result: { trace } } = JSON.parse(readFileSync(new URL("../web/demo.json", import.meta.url)));
const { logs, points } = trace;
const steady = logs.steady;
const control = steady.find(({ control_hours }) => control_hours > 0);
const stops = routeBook(points, "Darwin");
assert.equal(stops[0].km, 0);
assert.equal(stops.at(-1).name, "Adelaide");
assert.equal(checkpointHour(steady, stops[1].leg), control.race_arrive_hours);
assert.equal(checkpointHour([{ depleted: 1 }], 0), undefined);
const during = traceAt(steady, control.race_arrive_hours + control.control_hours / 2);
assert.equal(during.phase, "control");
assert.equal(replaySpeed(during), 0);
assert.equal(replaySpeed(traceAt(steady, 0)), steady[0].mean_speed_kmh);
assert.equal(during.distance, control.distance_km);
assert.ok(Math.abs(during.restRemaining - .25) < 1e-7);
assert.equal(traceAt(steady, steady[control.leg + 1].race_depart_hours).phase, "drive");

const overnight = steady.find(({ overnight_hours }) => overnight_hours > 0);
const parked = traceAt(steady, overnight.race_arrive_hours + overnight.control_hours + 1);
assert.equal(parked.phase, "overnight");
assert.equal(parked.distance, overnight.distance_km);
assert.equal(parked.soc, overnight.soc_pct);
assert.ok(parked.externalChargeWh > 0);
assert.equal(traceAt(steady, 24).phase, "drive");
assert.equal(traceAt(steady, 24).soc, 100);
assert.equal(logs.push.at(-1).depleted, 1);
assert.deepEqual(raceClock(13), { day: 1, clock: "21:00", night: true });
assert.deepEqual(raceClock(24), { day: 2, clock: "08:00", night: false });

const atClock = (hour) => atlasLighting((hour + 16) % 24);
const night = atClock(23);
assert.deepEqual(atClock(24), night);
assert.deepEqual(atlasLighting(24), atlasLighting(0));
assert.equal(atClock(6).label, "Dawn");
assert.equal(atClock(12).label, "Daylight");
assert.equal(atClock(18).label, "Dusk");
assert.equal(atClock(6).dark, false);
assert.equal(night.dark, true);
assert.notEqual(atClock(5.5).palette.water, atClock(6).palette.water);
assert.notEqual(atClock(17.5).palette.water, atClock(18).palette.water);
assert.notEqual(atClock(18).palette.landNorth, atClock(12).palette.landNorth);

const timeline = playbackTimeline(logs);
assert.ok(timeline.every(({ offset }, i) => i === 0 || timeline[i - 1].offset < offset));
const testHour = control.race_arrive_hours + .25;
const offset = translateTime(timeline, testHour, "hour", "offset");
assert.ok(Math.abs(testHour - translateTime(timeline, offset, "offset", "hour")) < 1e-7);
assert.equal(translateTime(timeline, timeline.at(-1).offset, "offset", "hour"), timeline.at(-1).hour);

const setup = { ...defaultSettings, trials: 20, seed: "18446744073709551615", array_m2: 0,
  headwind_offset_mps: -3.25, reserve_strategy: "envelope" };
assert.deepEqual(settingsFromQuery(setupQuery(setup)), setup);
assert.equal(settingsFromQuery("?trials=4").trace_trial, 4);
assert.deepEqual(readSetupFile(JSON.stringify({ version: 1, settings: setup })), setup);
assert.throws(() => readSetupFile(JSON.stringify({ version: 1, settings: { ...setup, seed: 15 } })));
assert.equal(skyCondition({ rain_mm_h: 4, solar_factor: .95 }), "Rain");
assert.equal(skyCondition({ rain_mm_h: 0, solar_factor: .72 }), "Partly cloudy");
assert.deepEqual(replayContext(withReplayContext("?replay=1&at=38", { at: 4.125, policy: "push" })),
  { at: 4.125, policy: "push" });
assert.equal(withReplayContext("?trials=8&at=4&policy=steady", { at: 0 }), "?trials=8");
assert.deepEqual(replayContext("?at=Infinity&policy=__proto__"), { at: 0, policy: undefined });

// Capture only clipped paths, which belong to the rain layer.
globalThis.window = { devicePixelRatio: 1 };
globalThis.getComputedStyle = () => ({ getPropertyValue: () => "" });
let clipped = false;
const drops = [];
const gradient = () => ({ addColorStop() {} });
const ctx = new Proxy({
  clip: () => { clipped = true; },
  restore: () => { clipped = false; },
  moveTo: (x, y) => {
    if (clipped) drops.push([x, y]);
  },
  createLinearGradient: gradient,
  createRadialGradient: gradient,
}, { get: (target, method) => target[method] ?? (() => {}) });
const canvas = { clientWidth: 900, clientHeight: 540, getContext: () => ctx };
const positions = Object.fromEntries(Object.entries(logs).map(([policy, events]) => [policy, traceAt(events, 0)]));
const rainFrame = (rain, motion) => {
  drops.length = 0;
  const selected = positions.steady;
  const wetPositions = { ...positions, steady: { ...selected, event: { ...selected.event, rain_mm_h: rain } } };
  drawRouteMap(canvas, trace, wetPositions, "steady", { lighting: atClock(8), motion, camera: "overview" });
  return drops.slice();
};
assert.equal(rainFrame(0, .25).length, 0);
const drizzle = rainFrame(.4, 0);
const shower = rainFrame(4, 0);
assert.ok(drizzle.length > 0 && shower.length > drizzle.length);
assert.deepEqual(rainFrame(4, 0), shower);
assert.notDeepEqual(rainFrame(4, .25), shower);
