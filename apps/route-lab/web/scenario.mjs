export const defaultSettings = Object.freeze({
  route: "outback", vehicle: "sr7", model: "mapped", weather: "posterior",
  pack_kwh: null, trials: 500, seed: "2025", trace_trial: 7,
  steady_kmh: 82, push_kmh: 92, reserve_strategy: "tiers",
  reserve_fast_kmh: 92, reserve_medium_kmh: 85, reserve_slow_kmh: 75, reserve_target_pct: 35,
  array_m2: null, cell_efficiency_pct: null, system_efficiency_pct: null, parked_wh_m2: null,
  solar_multiplier: 1, headwind_offset_mps: 0, rain_multiplier: 1, gust_sd_mps: 2,
});

export function settingsFromQuery(query) {
  const settings = { ...defaultSettings };
  const params = new URLSearchParams(query);
  for (const [name, fallback] of Object.entries(defaultSettings)) {
    if (!params.has(name)) continue;
    const value = params.get(name);
    settings[name] = typeof fallback === "string" || !value.trim() || !Number.isFinite(Number(value))
      ? value : Number(value);
  }
  if (!params.has("trace_trial") && Number.isInteger(settings.trials)) {
    settings.trace_trial = Math.min(settings.trials, defaultSettings.trace_trial);
  }
  return settings;
}

export function setupQuery(settings) {
  const params = new URLSearchParams();
  for (const [name, fallback] of Object.entries(defaultSettings)) {
    const value = settings[name];
    if (value !== undefined && value !== null && value !== fallback) params.set(name, value);
  }
  const query = params.toString();
  return query ? `?${query}` : "";
}

export function readSetupFile(content) {
  const file = JSON.parse(content);
  const settings = file?.settings;
  if (file?.version !== 1 || !settings || Object.getPrototypeOf(settings) !== Object.prototype ||
      !["route", "vehicle", "model", "seed", "trials"].every((name) => Object.hasOwn(settings, name)) ||
      Object.entries(settings).some(([name, value]) => {
        if (!Object.hasOwn(defaultSettings, name)) return true;
        const fallback = defaultSettings[name];
        if (typeof fallback === "string") return typeof value !== "string";
        return !(Number.isFinite(value) || (fallback === null && value === null));
      })) {
    throw new Error("Choose a version 1 setup exported from Route Lab.");
  }
  return { ...defaultSettings, ...settings };
}
