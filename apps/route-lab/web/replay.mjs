import { australia, routes } from "./map-data.mjs";

export const policyStyles = {
  steady: { color: "--teal", paint: "#79ad87", dash: [] },
  push: { color: "--rust", paint: "#df906f", dash: [7, 4] },
  "reserve-first": { color: "--blue", paint: "#81aec5", dash: [2, 4] },
};

const interpolate = (start, end, ratio) => start + (end - start) * ratio;

export function raceClock(hours) {
  const minutes = Math.floor((hours + 8) * 60 + 1e-7);
  const day = Math.floor(minutes / 1440) + 1;
  const hour = Math.floor(minutes / 60) % 24;
  const clock = [hour, minutes % 60].map((part) => String(part).padStart(2, "0")).join(":");
  return { day, clock, night: hour < 7 || hour >= 19 };
}

export function traceAt(events, hours) {
  for (const [index, event] of events.entries()) {
    const previous = events[index - 1];
    if (hours < event.race_arrive_hours) {
      const span = event.race_arrive_hours - event.race_depart_hours;
      const ratio = span > 0 ? Math.max(0, (hours - event.race_depart_hours) / span) : 0;
      return {
        distance: interpolate(previous?.distance_km ?? 0, event.distance_km, ratio),
        soc: interpolate(event.depart_soc_pct, event.soc_pct, ratio),
        reached: index, phase: "drive", event,
      };
    }
    const next = events[index + 1];
    if (!next || hours >= next.race_depart_hours) continue;
    const endControl = event.race_arrive_hours + event.control_hours;
    const overnight = hours >= endControl && event.overnight_hours > 0;
    const end = overnight ? next.race_depart_hours : Math.min(endControl, next.race_depart_hours);
    const externalChargeWh = overnight ? (next.external_charge_wh ?? 0) : 0;
    const fraction = overnight && externalChargeWh === 0 ? (hours - endControl) / (end - endControl) : 0;
    return {
      distance: event.distance_km,
      soc: interpolate(event.soc_pct, next.depart_soc_pct, fraction),
      externalChargeWh,
      reached: index + 1, phase: overnight ? "overnight" : "control",
      restRemaining: Math.max(0, end - hours), event,
    };
  }
  const event = events.at(-1);
  return { distance: event.distance_km, soc: event.soc_pct, reached: events.length,
    depleted: Boolean(event.depleted), phase: event.depleted ? "depleted" : "finish", event };
}

export function playbackTimeline(logs) {
  const all = Object.values(logs);
  const boundaries = new Set([0]);
  for (const events of all) {
    for (const event of events) {
      boundaries.add(event.race_depart_hours);
      boundaries.add(event.race_arrive_hours);
      if (event.control_hours && event.overnight_hours) {
        boundaries.add(event.race_arrive_hours + event.control_hours);
      }
    }
  }
  const points = [...boundaries].sort((a, b) => a - b);
  const timeline = [{ hour: 0, offset: 0 }];
  for (const [index, hour] of points.slice(1).entries()) {
    const start = points[index];
    const duration = hour - start;
    const phases = all.map((events) => traceAt(events, (start + hour) / 2).phase);
    const weight = phases.includes("control") ? duration * 3
      : phases.includes("drive") ? duration : Math.min(duration, .65);
    timeline.push({ hour, offset: timeline.at(-1).offset + weight });
  }
  return timeline;
}

export function translateTime(timeline, time, from, to) {
  const next = timeline.findIndex((point) => time < point[from]);
  if (next === -1) return timeline.at(-1)[to];
  if (next === 0) return timeline[0][to];
  const [a, b] = [timeline[next - 1], timeline[next]];
  return interpolate(a[to], b[to], (time - a[from]) / (b[from] - a[from]));
}

export function positionAt(points, kilometres, distance) {
  const index = kilometres.findIndex((km, index) => index > 0 && km > distance);
  const end = index === -1 ? points.length - 1 : index;
  const [a, b] = [points[end - 1], points[end]];
  const fraction = Math.max(0, Math.min(1, (distance - kilometres[end - 1]) / (kilometres[end] - kilometres[end - 1])));
  return [interpolate(a[0], b[0], fraction), interpolate(a[1], b[1], fraction),
    Math.atan2(b[1] - a[1], b[0] - a[0])];
}

function prepare(canvas) {
  const { clientWidth: width, clientHeight: height } = canvas;
  if (!width || !height) return null;
  const ratio = window.devicePixelRatio || 1;
  const pixelsWide = Math.round(width * ratio);
  const pixelsHigh = Math.round(height * ratio);
  if (canvas.width !== pixelsWide) canvas.width = pixelsWide;
  if (canvas.height !== pixelsHigh) canvas.height = pixelsHigh;
  const ctx = canvas.getContext("2d");
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  ctx.clearRect(0, 0, width, height);
  const css = getComputedStyle(canvas);
  return { ctx, width, height, color: (value) => css.getPropertyValue(value).trim() };
}

function polyline(ctx, points) {
  ctx.beginPath();
  points.forEach(([x, y], index) => index === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y));
}

function drawCar(ctx, x, y, angle, color, size, depleted, selected) {
  ctx.save();
  ctx.translate(x, y);
  ctx.rotate(angle + Math.PI / 2);
  ctx.scale(size, size);
  ctx.globalAlpha = depleted ? .58 : 1;

  if (selected) {
    ctx.fillStyle = "#faf3d466";
    ctx.beginPath();
    ctx.arc(0, 0, 20, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.fillStyle = "#070f11";
  for (const offset of [-8, 5]) {
    ctx.beginPath();
    ctx.roundRect(offset, -8, 3, 16, 1.4);
    ctx.fill();
  }

  ctx.beginPath();
  ctx.moveTo(0, -16);
  ctx.bezierCurveTo(7, -14, 9, -5, 9, 6);
  ctx.quadraticCurveTo(8, 14, 0, 15);
  ctx.quadraticCurveTo(-8, 14, -9, 6);
  ctx.bezierCurveTo(-9, -5, -7, -14, 0, -16);
  ctx.fillStyle = color;
  ctx.fill();
  ctx.strokeStyle = "#081612";
  ctx.lineWidth = 1.2;
  ctx.stroke();

  ctx.beginPath();
  ctx.roundRect(-4.5, -9, 9, 9, 3.7);
  ctx.fillStyle = "#183046";
  ctx.fill();
  ctx.beginPath();
  ctx.roundRect(-5.5, 2, 11, 9, 1.2);
  ctx.fillStyle = "#142824";
  ctx.fill();
  ctx.strokeStyle = "#7b9e96";
  ctx.lineWidth = .55;
  ctx.beginPath();
  ctx.moveTo(0, 2.5);
  ctx.lineTo(0, 10.5);
  ctx.moveTo(-5.1, 6.5);
  ctx.lineTo(5.1, 6.5);
  ctx.stroke();
  ctx.fillStyle = "#fbf8df";
  ctx.fillRect(-3.9, -12.1, 2.3, .9);
  ctx.fillRect(1.6, -12.1, 2.3, .9);
  ctx.restore();
}

function mapProjection(geographic, width, height) {
  if (!geographic) return ([x, y]) => [x * width, y * height];
  const scale = Math.min((width - 24) / (42 * .89), (height - 54) / 34);
  const centreX = width * (width > 900 ? .51 : .50);
  return ([longitude, latitude]) => [
    centreX + (longitude - 133) * .89 * scale,
    height / 2 - (latitude + 27) * scale,
  ];
}

export function routeBook(points, start) {
  return [
    { name: start, km: 0, leg: -1 },
    ...points.slice(0, -1).flatMap((point, leg) => point.control_minutes > 0 ? [{ ...point, leg }] : []),
    { ...points.at(-1), leg: points.length - 1 },
  ];
}

export function checkpointHour(events, leg) {
  if (leg === -1) return 0;
  const event = events[leg];
  return event && !event.depleted ? event.race_arrive_hours : undefined;
}

export function skyCondition(event) {
  const rain = event.rain_mm_h ?? 0;
  if (rain >= 2) return "Rain";
  if (rain > 0) return "Light rain";
  if (event.solar_factor > .83) return "Mostly clear";
  if (event.solar_factor > .60) return "Partly cloudy";
  return "Overcast";
}

function drawWeather(ctx, centre, weather, night, hour) {
  const [x, y] = centre;
  const cloud = 1 - weather.solar_factor;
  const rain = weather.rain_mm_h ?? 0;
  const radius = 38 + 92 * cloud + Math.min(32, rain * 2);
  const gradient = ctx.createRadialGradient(x, y, 2, x, y, radius);
  gradient.addColorStop(0, rain > 0 ? "#90b7da76" : night ? "#8291bd57"
    : cloud > .28 ? "#bfd0d263" : "#e9c45e58");
  gradient.addColorStop(1, "#00000000");
  ctx.fillStyle = gradient;
  ctx.fillRect(x - radius, y - radius, 2 * radius, 2 * radius);

  if (rain <= 0) return;
  ctx.save();
  ctx.translate(x, y);
  ctx.beginPath();
  ctx.ellipse(0, 0, radius * .77, radius * .58, 0, 0, Math.PI * 2);
  ctx.clip();
  const drops = Math.min(45, 11 + Math.round(rain * 3));
  ctx.strokeStyle = night ? "#cae1fd86" : "#406d905f";
  ctx.lineWidth = 1;
  ctx.beginPath();
  for (let drop = 0; drop < drops; drop += 1) {
    const span = radius * 2;
    const localX = (drop * 29.41 + hour * weather.headwind_mps * 1.5 + span * 1000) % span - radius;
    const localY = (drop * 41.7 + hour * 840) % span - radius;
    ctx.moveTo(localX, localY);
    ctx.lineTo(localX - 5, localY + 11);
  }
  ctx.stroke();
  ctx.restore();
}

const atlasPalettes = {
  day: {
    water: "#e5e9de", landNorth: "#eddfb7", landSouth: "#dfd7ba", coast: "#7b8b75",
    grid: "#4c695522", place: "#354437", halo: "#eee7cc", route: "#915139", travelled: "#a4422c",
  },
  night: {
    water: "#273c3a", landNorth: "#475244", landSouth: "#394b43", coast: "#839b7b",
    grid: "#dfe2b221", place: "#f1e9ca", halo: "#31443e", route: "#bd9c7d", travelled: "#ffc69c",
  },
};

function drawAtlas(ctx, width, height, project, geographic, palette, overview) {
  ctx.fillStyle = palette.water;
  ctx.fillRect(0, 0, width, height);

  if (!geographic) {
    ctx.beginPath();
    for (let tick = 20; tick < Math.max(width, height); tick += 30) {
      ctx.moveTo(tick, 0);
      ctx.lineTo(tick, height);
      ctx.moveTo(0, tick);
      ctx.lineTo(width, tick);
    }
    ctx.strokeStyle = palette.grid;
    ctx.stroke();
    return;
  }

  const land = ctx.createLinearGradient(0, 0, 0, height);
  land.addColorStop(0, palette.landNorth);
  land.addColorStop(1, palette.landSouth);
  for (const polygon of australia) {
    polyline(ctx, polygon.map(project));
    ctx.closePath();
    ctx.fillStyle = land;
    ctx.fill();
    ctx.strokeStyle = palette.coast;
    ctx.lineWidth = 1;
    ctx.stroke();
  }

  ctx.beginPath();
  for (let longitude = 100; longitude <= 165; longitude += 5) {
    ctx.moveTo(...project([longitude, -5]));
    ctx.lineTo(...project([longitude, -48]));
  }
  for (let latitude = -10; latitude >= -45; latitude -= 5) {
    ctx.moveTo(...project([100, latitude]));
    ctx.lineTo(...project([165, latitude]));
  }
  ctx.strokeStyle = palette.grid;
  ctx.stroke();

  if (!overview || width < 620) return;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.font = "italic 11px Georgia, serif";
  ctx.fillStyle = palette.place;
  ctx.globalAlpha = .7;
  ctx.fillText("Timor Sea", ...project([123, -12.5]));
  ctx.fillText("Coral Sea", ...project([151, -17]));
  ctx.fillText("Great Australian Bight", ...project([129, -34.7]));
  ctx.font = "9px ui-monospace, monospace";
  for (let longitude = 115; longitude <= 150; longitude += 5) {
    const [x] = project([longitude, -10]);
    ctx.fillText(`${longitude}° E`, x, 14);
  }
  ctx.font = "10px ui-monospace, monospace";
  ctx.fillText("W E S T E R N", ...project([120.7, -24.7]));
  ctx.fillText("A U S T R A L I A", ...project([120.7, -25.6]));
  ctx.fillText("Q U E E N S L A N D", ...project([145.8, -24]));
  ctx.globalAlpha = 1;
}

export function drawRouteMap(canvas, trace, positions, selected, night, hour = 0, camera = "overview") {
  const frame = prepare(canvas);
  if (!frame) return;
  const { ctx, width, height } = frame;
  const route = routes[trace.route];
  const overview = mapProjection(route.geographic, width, height);
  const kilometres = [0, ...trace.points.map((point) => point.km)];
  const active = positions[selected];
  const [focusX, focusY] = positionAt(route.points.map(overview), kilometres, active.distance);
  const project = camera === "follow" ? (point) => {
    const [x, y] = overview(point);
    return [width / 2 + (x - focusX) * 2.7, height / 2 + (y - focusY) * 2.7];
  } : overview;
  const points = route.points.map(project);
  const weather = active.event;
  const palette = night ? atlasPalettes.night : atlasPalettes.day;
  drawAtlas(ctx, width, height, project, route.geographic, palette, camera === "overview");
  if (active.phase === "drive" || active.phase === "control") {
    drawWeather(ctx, positionAt(points, kilometres, active.distance), weather, night, hour);
  }
  polyline(ctx, points);
  ctx.strokeStyle = palette.halo;
  ctx.lineWidth = 4.5;
  ctx.stroke();
  ctx.strokeStyle = palette.route;
  ctx.lineWidth = 1.6;
  ctx.stroke();

  const traveled = points.filter((point, index) => kilometres[index] < active.distance);
  traveled.push(positionAt(points, kilometres, active.distance));
  polyline(ctx, traveled);
  ctx.strokeStyle = palette.travelled;
  ctx.lineWidth = 2.8;
  ctx.stroke();

  for (const [index, [x, y]] of points.entries()) {
    const stop = index === 0 || index === points.length - 1 || trace.points[index - 1].control_minutes > 0;
    if (!stop) continue;
    ctx.beginPath();
    ctx.arc(x, y, 3.5, 0, Math.PI * 2);
    ctx.fillStyle = palette.halo;
    ctx.fill();
    ctx.strokeStyle = palette.route;
    ctx.lineWidth = 1.25;
    ctx.stroke();
  }
  ctx.font = "600 12px Georgia, serif";
  ctx.textBaseline = "middle";
  const labels = camera === "follow"
    ? [...new Set([...route.labels, active.event.leg + 1])].sort((a, b) => a - b)
    : width < 420 && route.geographic ? [0, 12, 17, 28] : route.labels;
  for (const [index, point] of labels.entries()) {
    const [x, y] = points[point];
    const label = point === 0 ? route.start : trace.points[point - 1].name;
    const left = index % 2 === 1 || x > width * .8;
    ctx.textAlign = left ? "right" : "left";
    ctx.strokeStyle = palette.halo;
    ctx.lineWidth = 3;
    ctx.strokeText(label, x + (left ? -10 : 10), y + (point === 0 ? -8 : 0));
    ctx.fillStyle = palette.place;
    ctx.fillText(label, x + (left ? -10 : 10), y + (point === 0 ? -8 : 0));
  }
  const markers = Object.entries(positions).map(([policy, state], index) => {
    const [x, y, angle] = positionAt(points, kilometres, state.distance);
    const offset = (index - 1) * (width < 420 ? 18 : 22);
    const carX = x - Math.sin(angle) * offset;
    const carY = y + Math.cos(angle) * offset;
    const paint = policyStyles[policy].paint;
    drawCar(ctx, carX, carY, angle, paint, width < 420 ? .78 : .9, state.depleted, policy === selected);
    return { x: carX, y: carY, paint, state };
  });
  ctx.font = "bold 9px ui-monospace, monospace";
  ctx.textAlign = "center";
  for (const [index, marker] of markers.entries()) {
    const { x, y, paint, state } = marker;
    if (!["control", "overnight"].includes(state.phase) || x < 0 || x > width || y < 0 || y > height) continue;
    const label = state.phase === "control" ? "REST" : state.externalChargeWh > 0 ? "TOP-UP" : "PARKED";
    const tagX = Math.max(32, Math.min(width - 32, x + (index - 1) * 30));
    const tagY = Math.max(10, y - 24 - (index % 2) * 15);
    const half = ctx.measureText(label).width / 2 + 5;
    polyline(ctx, [[x, y - 8], [tagX, tagY + 5]]);
    ctx.strokeStyle = paint;
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.fillStyle = "#27372e";
    ctx.fillRect(tagX - half, tagY - 7, half * 2, 14);
    ctx.fillStyle = "#fff2d1";
    ctx.fillText(label, tagX, tagY);
  }
}

export function replaySpeed({ event, phase }) {
  return phase === "drive" ? (event.mean_speed_kmh ?? event.speed_kmh) : 0;
}

export function drawTelemetryHistory(canvas, logs, hours, horizon, positions, mode) {
  const frame = prepare(canvas);
  if (!frame) return;
  const { ctx, width, height, color } = frame;
  const left = 40, right = width - 14, top = 13, bottom = height - 29;
  const x = (time) => left + (right - left) * time / horizon;
  const speedMode = mode === "speed";
  const peak = speedMode ? Math.max(20, ...Object.values(logs).flatMap(
    (events) => events.map((event) => event.mean_speed_kmh ?? event.speed_kmh),
  )) : 100;
  const upper = Math.ceil(peak / 20) * 20;
  const y = (value) => bottom - (bottom - top) * value / upper;
  ctx.font = "10px system-ui, sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  for (const value of speedMode ? [0, upper / 2, upper] : [0, 15, 50, 100]) {
    const reserve = !speedMode && value === 15;
    polyline(ctx, [[left, y(value)], [right, y(value)]]);
    ctx.setLineDash(reserve ? [3, 4] : []);
    ctx.strokeStyle = color(reserve ? "--rust" : "--line");
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.fillStyle = color("--muted");
    ctx.fillText(speedMode ? String(value) : `${value}%`, left - 7, y(value));
  }
  ctx.setLineDash([]);
  ctx.textAlign = "center";
  for (const fraction of [0, .25, .5, .75, 1]) {
    const hour = fraction * horizon;
    ctx.fillText(`${hour.toFixed(1)} h`, x(hour), bottom + 13);
  }
  for (const [name, events] of Object.entries(logs)) {
    const points = [];
    for (const [index, event] of events.entries()) {
      const { race_depart_hours: depart, race_arrive_hours: arrive } = event;
      if (speedMode) {
        const speed = event.mean_speed_kmh ?? event.speed_kmh;
        points.push([x(depart), y(speed)], [x(arrive), y(speed)]);
        if (event.control_hours || event.overnight_hours || event === events.at(-1)) {
          points.push([x(arrive), y(0)]);
          if (event !== events.at(-1)) points.push([x(arrive + event.control_hours + event.overnight_hours), y(0)]);
        }
      } else {
        if (index > 0 && event.external_charge_wh > 0) {
          points.push([x(depart), y(events[index - 1].soc_pct)]);
        }
        points.push([x(depart), y(event.depart_soc_pct)], [x(arrive), y(event.soc_pct)]);
        if (event.control_hours) points.push([x(arrive + event.control_hours), y(event.soc_pct)]);
      }
    }
    const finish = events.at(-1).race_arrive_hours;
    polyline(ctx, points);
    ctx.strokeStyle = color(policyStyles[name].color);
    ctx.setLineDash(policyStyles[name].dash);
    ctx.lineWidth = 1.7;
    ctx.stroke();
    const state = positions[name];
    const value = speedMode ? replaySpeed(state) : state.soc;
    ctx.beginPath();
    ctx.arc(x(Math.min(hours, finish)), y(value), 3, 0, Math.PI * 2);
    ctx.fillStyle = ctx.strokeStyle;
    ctx.fill();
  }
  polyline(ctx, [[x(hours), top], [x(hours), bottom]]);
  ctx.setLineDash([2, 4]);
  ctx.strokeStyle = color("--muted");
  ctx.lineWidth = 1;
  ctx.stroke();
}
