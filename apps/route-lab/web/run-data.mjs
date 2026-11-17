import { defaultSettings, settingsFromQuery, setupQuery } from "./scenario.mjs";

export const policyLabels = { steady: "Steady", push: "Push", "reserve-first": "Reserve-first" };
export const numbers = new Intl.NumberFormat("en-AU", { maximumFractionDigits: 0 });
export const percent = (value) => `${(100 * value).toFixed(1)}%`;
const cacheKey = "ecm.route-lab.last-run.v2";

export function runQuery(settings, recorded) {
  return recorded ? "?replay=1" : setupQuery(settings);
}

export function replayContext(query) {
  const params = new URLSearchParams(query);
  const at = Number(params.get("at"));
  const policy = params.get("policy");
  return {
    at: Number.isFinite(at) && at > 0 ? at : 0,
    policy: Object.hasOwn(policyLabels, policy) ? policy : undefined,
  };
}

export function withReplayContext(query, { at, policy }) {
  const params = new URLSearchParams(query);
  if (Number.isFinite(at) && at > 0) params.set("at", String(at));
  else params.delete("at");
  if (Object.hasOwn(policyLabels, policy)) params.set("policy", policy);
  else params.delete("policy");
  const serialized = params.toString();
  return serialized ? `?${serialized}` : "";
}

export function runSummary(data, settings) {
  const strategy = settings.reserve_strategy === "envelope" ? "Energy envelope" : "Speed tiers";
  const weather = settings.weather === "posterior" ? "Posterior weather" : "Baseline weather";
  return `${numbers.format(data.trials)} draws · seed ${settings.seed} · ${data.energy_rules === "bwsc_2025_cruiser" ? "SR7 Cruiser" : settings.vehicle.toUpperCase()} · ${settings.model} · ${weather} · ${strategy}`;
}

export async function readRun({ reuse = false } = {}) {
  const params = new URLSearchParams(location.search);
  const settings = settingsFromQuery(params);
  if (params.get("replay") === "1") return readRecorded();

  const query = setupQuery(settings);
  if (reuse) {
    try {
      const cached = JSON.parse(sessionStorage.getItem(cacheKey));
      if (cached?.query === query) return cached.run;
    } catch {
      // Storage may be unavailable. A direct analytics link can run the case again.
    }
  }

  const response = await fetch("./api/run", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(settings),
  });
  if ([404, 405, 501].includes(response.status)) return readRecorded();
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || "The experiment failed.");
  const run = { data, settings, recorded: false };
  try {
    sessionStorage.setItem(cacheKey, JSON.stringify({ query, run }));
  } catch {
    // The results still work if storage is disabled or its quota is exceeded.
  }
  return run;
}

async function readRecorded() {
  const response = await fetch("./demo.json");
  if (!response.ok) throw new Error("The recorded native run is unavailable.");
  const { result, settings } = await response.json();
  return { data: result, settings: { ...defaultSettings, ...settings }, recorded: true };
}

export function downloadFile(contents, filename, type = "text/csv") {
  const url = URL.createObjectURL(new Blob([contents], { type }));
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
