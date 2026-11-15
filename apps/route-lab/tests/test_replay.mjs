import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { playbackTimeline, raceClock, traceAt, translateTime } from "../web/replay.mjs";

const { result: { trace: { logs } } } = JSON.parse(readFileSync(new URL("../web/demo.json", import.meta.url)));
const steady = logs.steady;
const control = steady.find(({ control_hours }) => control_hours > 0);
const during = traceAt(steady, control.race_arrive_hours + control.control_hours / 2);
assert.equal(during.phase, "control");
assert.equal(during.distance, control.distance_km);
assert.ok(Math.abs(during.restRemaining - .25) < 1e-7);
assert.equal(traceAt(steady, steady[control.leg + 1].race_depart_hours).phase, "drive");

const overnight = steady.find(({ overnight_hours }) => overnight_hours > 0);
const parked = traceAt(steady, overnight.race_arrive_hours + overnight.control_hours + 1);
assert.equal(parked.phase, "overnight");
assert.equal(parked.distance, overnight.distance_km);
assert.equal(traceAt(steady, 24).phase, "drive");
assert.deepEqual(raceClock(13), { day: 1, clock: "21:00", night: true });
assert.deepEqual(raceClock(24), { day: 2, clock: "08:00", night: false });

const timeline = playbackTimeline(logs);
assert.ok(timeline.every(({ offset }, i) => i === 0 || timeline[i - 1].offset < offset));
const testHour = control.race_arrive_hours + .25;
const offset = translateTime(timeline, testHour, "hour", "offset");
assert.ok(Math.abs(testHour - translateTime(timeline, offset, "offset", "hour")) < 1e-7);
assert.equal(translateTime(timeline, timeline.at(-1).offset, "offset", "hour"), timeline.at(-1).hour);
