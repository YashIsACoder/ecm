import { loadFleet } from "./fleet-data.mjs";

const specs = document.querySelector("#fleet-specs");
const results = document.querySelector("#fleet-results");
const run = document.querySelector("#fleet-run");
const pct = (value) => `${(value * 100).toFixed(1)}%`;
const points = (value) => `${value > 0 ? "+" : ""}${(value * 100).toFixed(1)} pp`;

function rowFor(team, values) {
  const row = document.createElement("tr");
  const name = document.createElement("th");
  name.scope = "row";
  name.textContent = team.car;
  const entrant = document.createElement("span");
  entrant.textContent = team.team;
  name.append(entrant);
  row.append(name);
  for (const value of values) {
    const field = row.insertCell();
    field.textContent = value;
  }
  return row;
}

try {
  const { teams, trials, seed, model, weather } = await loadFleet();
  if (specs) specs.replaceChildren(...teams.map((team) => rowFor(team, [
    team.region, team.mass_kg, team.drag_area_m2.toFixed(3), team.array_m2.toFixed(1),
    team.pack_kwh.toFixed(2), team.pace_ceiling_kmh,
  ])));
  if (results) {
    run.textContent = `${trials} matched weather draws · Seed ${seed} · ${model} ECM · ${weather} weather`;
    results.replaceChildren(...teams.map((team) => {
      const [rate, low, high] = team.finish_rate.map(pct);
      const [difference, deltaLow, deltaHigh] = team.finish_delta.map(points);
      return rowFor(team, [
        `${rate} (${low} to ${high})`,
        team.id === "sunswift" ? "Reference" : `${difference} (${deltaLow} to ${deltaHigh})`,
        team.median_finish_hours === null ? "—" : `${team.median_finish_hours.toFixed(1)} h`,
      ]);
    }));
  }
} catch (error) {
  const message = document.createElement("p");
  message.setAttribute("role", "status");
  message.textContent = error.message;
  document.querySelector("main").append(message);
}
