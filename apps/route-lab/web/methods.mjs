const chart = document.querySelector("#chain-trace");
const buttons = [...document.querySelectorAll("#feature-switch button")];
const sample = document.querySelector("#sample");
const meta = document.querySelector("#run-meta");
const stats = document.querySelector("#chain-stats");

const features = {
  solar: { unit: "pp", scale: 100, description: "Offset to the clear-sky irradiance factor, in percentage points." },
  wind: { unit: "m/s", scale: 1, description: "Offset to signed route wind, in metres per second. Positive values increase headwind." },
};
const colors = ["#26624d", "#a3412c", "#255d78", "#75527c"];
let report;
let feature = "solar";

function drawTrace() {
  if (!report) return;
  const { chains, exact_mean, exact_sd } = report.features[feature];
  const index = Number(sample.value);
  const { scale } = features[feature];
  const { clientWidth: width, clientHeight: height } = chart;
  if (width < 100 || height < 60) return;
  const ratio = window.devicePixelRatio || 1;
  chart.width = Math.round(width * ratio);
  chart.height = Math.round(height * ratio);
  const ctx = chart.getContext("2d");
  ctx.scale(ratio, ratio);
  const theme = getComputedStyle(chart);
  const color = (token) => theme.getPropertyValue(token).trim();
  const left = 49, right = width - 16, top = 14, bottom = height - 27;
  const points = chains[0].samples.length;
  const values = chains.flatMap((chain) => chain.samples);
  const low = Math.min(...values, exact_mean - exact_sd);
  const high = Math.max(...values, exact_mean + exact_sd);
  const x = (i) => left + (right - left) * i / (points - 1);
  const y = (v) => bottom - (bottom - top) * (v - low) / (high - low);

  ctx.fillStyle = "#90a88e20";
  ctx.fillRect(left, y(exact_mean + exact_sd), right - left, y(exact_mean - exact_sd) - y(exact_mean + exact_sd));
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  ctx.font = "11px ui-monospace, monospace";
  for (const fraction of [0, .25, .5, .75, 1]) {
    const value = low + (high - low) * fraction;
    ctx.fillStyle = color("--muted");
    ctx.fillText((value * scale).toFixed(1), left - 8, y(value));
    ctx.fillStyle = color("--rule");
    ctx.fillRect(left, y(value), right - left, 1);
  }
  ctx.textAlign = "left";
  ctx.fillStyle = color("--muted");
  ctx.fillText("First retained", left, bottom + 15);
  ctx.textAlign = "right";
  ctx.fillText("Last plotted", right, bottom + 15);
  ctx.strokeStyle = color("--green");
  ctx.setLineDash([5, 4]);
  ctx.beginPath();
  ctx.moveTo(left, y(exact_mean));
  ctx.lineTo(right, y(exact_mean));
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.globalAlpha = .69;
  chains.forEach((chain, c) => {
    ctx.beginPath();
    chain.samples.forEach((value, i) => {
      if (i === 0) ctx.moveTo(x(i), y(value));
      else ctx.lineTo(x(i), y(value));
    });
    ctx.strokeStyle = colors[c];
    ctx.stroke();
  });
  ctx.globalAlpha = 1;
  ctx.fillStyle = color("--rust");
  ctx.fillRect(x(index), top, 1, bottom - top);
  chains.forEach((chain, c) => {
    ctx.beginPath();
    ctx.arc(x(index), y(chain.samples[index]), 3.7, 0, 2 * Math.PI);
    ctx.fillStyle = colors[c];
    ctx.fill();
  });
}

function inspect() {
  const index = Number(sample.value);
  const { unit, scale } = features[feature];
  sample.setAttribute("aria-valuetext", `Retained iteration ${index * report.plot_stride + 1}`);
  document.querySelector("#sample-number").textContent = String(index * report.plot_stride + 1);
  report.features[feature].chains.forEach((chain, c) => {
    stats.children[c].lastElementChild.textContent = `${(chain.samples[index] * scale).toFixed(2)} ${unit}`;
  });
  drawTrace();
}

function renderFeature() {
  const { unit, scale, description } = features[feature];
  const details = report.features[feature];
  const format = (mean, sd) => `${(mean * scale).toFixed(2)} ± ${(sd * scale).toFixed(2)} ${unit}`;
  buttons.forEach((button) => button.setAttribute("aria-pressed", String(button.dataset.feature === feature)));
  document.querySelector("#feature-description").textContent = description;
  document.querySelector("#sampled").textContent = format(details.sample_mean, details.sample_sd);
  document.querySelector("#exact").textContent = format(details.exact_mean, details.exact_sd);
  document.querySelector("#rhat").textContent = details.split_rhat.toFixed(3);
  inspect();
}

try {
  const response = await fetch("./forecast-demo.json");
  if (!response.ok) throw new Error();
  report = await response.json();
  const { chains } = report.features.solar;
  meta.textContent = `SEED ${report.seed} · ${report.observations} SYNTHETIC RESIDUAL PAIRS · ${chains.length} CHAINS`;
  sample.max = chains[0].samples.length - 1;
  sample.value = Math.floor(Number(sample.max) * .65);
  sample.disabled = false;
  stats.replaceChildren(...chains.map((chain, c) => {
    const card = document.createElement("div");
    const title = document.createElement("strong");
    title.textContent = `Chain ${c + 1}`;
    title.style.color = colors[c];
    const acceptance = document.createElement("small");
    acceptance.textContent = `${(100 * chain.acceptance).toFixed(1)}% accepted`;
    card.append(title, acceptance, document.createElement("span"));
    return card;
  }));
  buttons.forEach((button) => button.addEventListener("click", () => {
    feature = button.dataset.feature;
    renderFeature();
  }));
  sample.addEventListener("input", inspect);
  renderFeature();
  new ResizeObserver(drawTrace).observe(chart);
} catch {
  meta.textContent = "The native calibration report could not be loaded.";
  meta.classList.add("error");
  buttons.forEach((button) => { button.disabled = true; });
}
