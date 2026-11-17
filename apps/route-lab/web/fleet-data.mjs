export async function loadFleet() {
  const response = await fetch("./atlas-demo.json");
  if (!response.ok) throw new Error("The native Cruiser replay is unavailable.");
  return response.json();
}

export function fleetRows(teams, hour, traceAt) {
  return teams.map(({ id, legs }, index) => ({ id, index, state: traceAt(legs, hour) })).sort((a, b) =>
    b.state.distance - a.state.distance ||
    (a.state.phase === "finish" && b.state.phase === "finish"
      ? a.state.event.race_arrive_hours - b.state.event.race_arrive_hours : 0) ||
    a.index - b.index);
}

export function fleetTableRow(team, values) {
  const row = document.createElement("tr");
  const name = document.createElement("th");
  name.scope = "row";
  name.textContent = team.car;
  if (team.car !== team.team) {
    const entrant = document.createElement("span");
    entrant.textContent = team.team;
    name.append(entrant);
  }
  row.append(name);
  for (const value of values) row.insertCell().textContent = value;
  return row;
}

export function fillFleetComparison(container, description, { teams, trials, seed, model, weather }) {
  const pct = (value) => `${(value * 100).toFixed(1)}%`;
  const points = (value) => `${value > 0 ? "+" : ""}${(value * 100).toFixed(1)} pp`;
  description.textContent = `${trials} matched weather draws · Seed ${seed} · ${model} ECM · ${weather} weather`;
  container.replaceChildren(...teams.map((team) => {
    const [rate, low, high] = team.finish_rate.map(pct);
    const [difference, deltaLow, deltaHigh] = team.finish_delta.map(points);
    return fleetTableRow(team, [
      `${rate} (${low} to ${high})`,
      team.id === "sunswift" ? "Reference" : `${difference} (${deltaLow} to ${deltaHigh})`,
      team.median_finish_hours === null ? "—" : `${team.median_finish_hours.toFixed(1)} h`,
    ]);
  }));
}
