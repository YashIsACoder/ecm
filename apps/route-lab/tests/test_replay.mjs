import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { defaultSettings, readSetupFile, settingsFromQuery, setupQuery } from "../web/scenario.mjs";
import { checkpointHour, routeBook, playbackTimeline, raceClock, replaySpeed, skyCondition, traceAt, translateTime } from "../web/replay.mjs";

const { result: { trace: { logs, points } } } = JSON.parse(readFileSync(new URL("../web/demo.json", import.meta.url)));
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
