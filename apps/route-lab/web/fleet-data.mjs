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
