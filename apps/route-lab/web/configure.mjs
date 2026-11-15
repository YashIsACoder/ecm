import { readSetupFile, settingsFromQuery, setupQuery } from "./scenario.mjs";

export function mountSetup(container, initial) {
  const form = container.querySelector("#experiment");
  const { vehicle, reserve_strategy: strategy, trials, trace_trial: trace } = form.elements;
  const status = container.querySelector("#scenario-status");
  const fileInput = container.querySelector("#load-scenario");
  const presets = { reference: [5.5, 6, 24], sr7: [38, 4.8, 22], sr8: [45, 4.5, 23] };
  const solarFields = ["pack_kwh", "array_m2", "cell_efficiency_pct"];

  function updateSetup() {
    const preset = presets[vehicle.value];
    if (preset) solarFields.forEach((name, i) => {
      form.elements.namedItem(name).placeholder = `Preset · ${preset[i]}`;
    });
    const envelope = strategy.value === "envelope";
    form.querySelector("#middle-tier").hidden = envelope;
    form.querySelector("#strategy-note").textContent = envelope
      ? "Carries energy for upcoming legs and tests speeds using the current weather."
      : "Drops a speed tier when the current leg would cut into the charge target.";
  }

  function applySettings(settings) {
    for (const [field, value] of Object.entries(settings)) {
      form.elements.namedItem(field).value = value ?? "";
    }
    trace.max = trials.value;
    updateSetup();
  }

  function showStatus(message, error = false) {
    status.hidden = !message;
    status.textContent = message;
    status.classList.toggle("error", error);
  }

  applySettings(initial);
  vehicle.addEventListener("change", updateSetup);
  strategy.addEventListener("change", updateSetup);
  trials.addEventListener("input", () => {
    trace.max = trials.value || String(3000);
    if (trials.value && trace.valueAsNumber > trials.valueAsNumber) trace.value = trials.value;
  });

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    const settings = Object.fromEntries(new FormData(form));
    for (const name of Object.keys(settings)) {
      if (form.elements.namedItem(name).type === "number") {
        settings[name] = settings[name] === "" ? null : Number(settings[name]);
      }
    }

    const { reserve_fast_kmh: high, reserve_medium_kmh: mid, reserve_slow_kmh: low, seed } = settings;
    if (high < low || (strategy.value === "tiers" && !(high >= mid && mid >= low))) {
      showStatus("Check that reserve speeds descend from upper to lower.", true);
      form.elements.reserve_fast_kmh.focus();
      return;
    }
    if (BigInt(seed) >= 2n ** 64n) {
      showStatus("Use a seed from 0 to 18446744073709551615.", true);
      form.elements.seed.focus();
      return;
    }
    location.assign(`./${setupQuery(settings)}`);
  });

  fileInput.addEventListener("change", async () => {
    const file = fileInput.files[0];
    if (!file) return;
    try {
      if (file.size > 8192) throw new Error("A run setup must be smaller than 8 kB.");
      applySettings(readSetupFile(await file.text()));
      showStatus("Setup loaded. Run the experiment to apply it.");
    } catch (error) {
      showStatus(error.message, true);
    } finally {
      fileInput.value = "";
    }
  });
}

const standalone = document.querySelector(".setup-page");
if (standalone) {
  const initial = settingsFromQuery(location.search);
  mountSetup(standalone, initial);
  document.querySelector("#back-results").href = `./${setupQuery(initial)}`;
}
