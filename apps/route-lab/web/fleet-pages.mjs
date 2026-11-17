import { fillFleetComparison, fleetTableRow, loadFleet } from "./fleet-data.mjs";

const specs = document.querySelector("#fleet-specs");
const results = document.querySelector("#fleet-results");

try {
  const report = await loadFleet();
  if (specs) specs.replaceChildren(...report.teams.map((team) => fleetTableRow(team, [
    team.region, team.mass_kg, team.drag_area_m2.toFixed(3), team.array_m2.toFixed(1),
    team.pack_kwh.toFixed(2), team.pace_ceiling_kmh,
  ])));
  if (results) fillFleetComparison(results, document.querySelector("#fleet-run"), report);
} catch (error) {
  const message = document.createElement("p");
  message.setAttribute("role", "status");
  message.textContent = error.message;
  document.querySelector("main").append(message);
}
