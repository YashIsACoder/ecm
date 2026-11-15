import { australia, routes } from "./map-data.mjs";

export const policyStyles = {
  steady: { color: "--teal", dash: [] },
  push: { color: "--rust", dash: [7, 4] },
  "reserve-first": { color: "--blue", dash: [2, 4] },
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
    const fraction = overnight ? (hours - endControl) / (end - endControl) : 0;
    return {
      distance: event.distance_km,
      soc: overnight ? interpolate(event.soc_pct, next.depart_soc_pct, fraction) : event.soc_pct,
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
    ctx.fillStyle = "#ecf8e924";
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
  const scale = Math.min((width - 34) / (42 * .89), (height - 25) / 33);
  const centreX = width / 2 - scale * (width > 560 ? 3 : 0);
  return ([longitude, latitude]) => [
    centreX + (longitude - 133) * .89 * scale,
    height / 2 - (latitude + 27) * scale,
  ];
}

export function drawRouteMap(canvas, trace, positions, selected, night) {
  const frame = prepare(canvas);
  if (!frame) return;
  const { ctx, width, height, color } = frame;
  const route = routes[trace.route];
  const project = mapProjection(route.geographic, width, height);
  const points = route.points.map(project);
  const kilometres = [0, ...trace.points.map((point) => point.km)];
  const weather = positions[selected].event;
  const sun = Math.max(0, Math.min(1, weather.irradiance_w_m2 / 1000));
  const sky = ctx.createLinearGradient(0, 0, width, height);
  sky.addColorStop(0, night ? "#11192c" : sun > .58 ? "#3a4026" : "#253837");
  sky.addColorStop(1, night ? "#0b131b" : "#101e18");
  ctx.fillStyle = sky;
  ctx.fillRect(0, 0, width, height);
  for (let x = 20; x < width; x += 25) {
    for (let y = 20; y < height; y += 25) {
      ctx.fillStyle = "#80a4912c";
      ctx.fillRect(x, y, 1, 1);
    }
  }
  if (route.geographic) {
    for (const polygon of australia) {
      polyline(ctx, polygon.map(project));
      ctx.closePath();
      ctx.fillStyle = night ? "#1b2932" : sun > .58 ? "#334334" : "#253b33";
      ctx.fill();
      ctx.strokeStyle = "#486755";
      ctx.lineWidth = 1;
      ctx.stroke();
    }
  } else {
    ctx.strokeStyle = "#49645345";
    for (let ring = 1; ring <= 5; ring += 1) {
      ctx.beginPath();
      ctx.ellipse(width * .52, height * .49, width * .078 * ring, height * .088 * ring, -.24, 0, Math.PI * 2);
      ctx.stroke();
    }
  }
  const active = positions[selected];
  if (active.phase === "drive" || active.phase === "control") {
    // A moving local layer shows attenuation on the selected leg, over the map's day/night palette.
    const [x, y] = positionAt(points, kilometres, active.distance);
    const cloud = 1 - weather.solar_factor;
    const radius = 33 + 85 * cloud;
    const shading = ctx.createRadialGradient(x, y, 3, x, y, radius);
    shading.addColorStop(0, night ? "#8798c533" : cloud > .28 ? "#adc6d158" : "#f1c45e58");
    shading.addColorStop(1, "#00000000");
    ctx.fillStyle = shading;
    ctx.fillRect(x - radius, y - radius, 2 * radius, 2 * radius);
  }
  polyline(ctx, points);
  ctx.strokeStyle = "#0c1913";
  ctx.lineWidth = 7;
  ctx.stroke();
  ctx.strokeStyle = "#b7d576";
  ctx.lineWidth = 2;
  ctx.stroke();
  for (const [index, [x, y]] of points.entries()) {
    const control = trace.points[index - 1]?.control_minutes > 0;
    ctx.beginPath();
    ctx.arc(x, y, control ? 4 : 2.1, 0, Math.PI * 2);
    ctx.fillStyle = control ? "#ffd781" : "#ecf5d2";
    ctx.fill();
  }
  ctx.font = "600 11px system-ui, sans-serif";
  ctx.textBaseline = "middle";
  const labels = width < 420 && route.geographic ? [0, 12, 17, 28] : route.labels;
  for (const [index, point] of labels.entries()) {
    const [x, y] = points[point];
    const label = point === 0 ? route.start : trace.points[point - 1].name;
    const left = index % 2 === 1 || x > width * .8;
    ctx.textAlign = left ? "right" : "left";
    ctx.strokeStyle = "#111f17";
    ctx.lineWidth = 3;
    ctx.strokeText(label, x + (left ? -10 : 10), y + (point === 0 ? -8 : 0));
    ctx.fillStyle = color("--ink");
    ctx.fillText(label, x + (left ? -10 : 10), y + (point === 0 ? -8 : 0));
  }
  const markers = Object.entries(positions).map(([policy, state], index) => {
    const [x, y, angle] = positionAt(points, kilometres, state.distance);
    const offset = (index - 1) * (width < 420 ? 18 : 22);
    const carX = x - Math.sin(angle) * offset;
    const carY = y + Math.cos(angle) * offset;
    const paint = color(policyStyles[policy].color);
    drawCar(ctx, carX, carY, angle, paint, width < 420 ? .78 : .9, state.depleted, policy === selected);
    return { x: carX, y: carY, paint, state };
  });
  ctx.font = "bold 9px ui-monospace, monospace";
  ctx.textAlign = "center";
  for (const [index, marker] of markers.entries()) {
    const { x, y, paint, state } = marker;
    if (!["control", "overnight"].includes(state.phase)) continue;
    const label = state.phase === "control" ? "REST" : "PARKED";
    const tagX = Math.max(29, Math.min(width - 29, x + (index - 1) * 30));
    const tagY = Math.max(10, y - 24 - (index % 2) * 15);
    const half = ctx.measureText(label).width / 2 + 5;
    polyline(ctx, [[x, y - 8], [tagX, tagY + 5]]);
    ctx.strokeStyle = paint;
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.fillStyle = "#151c19";
    ctx.fillRect(tagX - half, tagY - 7, half * 2, 14);
    ctx.strokeRect(tagX - half, tagY - 7, half * 2, 14);
    ctx.fillStyle = "#ffdd92";
    ctx.fillText(label, tagX, tagY);
  }
}

export function drawBattery(canvas, logs, hours, horizon, positions) {
  const frame = prepare(canvas);
  if (!frame) return;
  const { ctx, width, height, color } = frame;
  const left = 40, right = width - 14, top = 13, bottom = height - 29;
  const x = (time) => left + (right - left) * time / horizon;
  const y = (soc) => bottom - (bottom - top) * soc / 100;
  ctx.font = "10px system-ui, sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  for (const charge of [0, 15, 50, 100]) {
    polyline(ctx, [[left, y(charge)], [right, y(charge)]]);
    ctx.setLineDash(charge === 15 ? [3, 4] : []);
    ctx.strokeStyle = color(charge === 15 ? "--rust" : "--line");
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.fillStyle = color("--muted");
    ctx.fillText(`${charge}%`, left - 7, y(charge));
  }
  ctx.setLineDash([]);
  ctx.textAlign = "center";
  for (const fraction of [0, .25, .5, .75, 1]) {
    const hour = fraction * horizon;
    ctx.fillText(`${hour.toFixed(1)} h`, x(hour), bottom + 13);
  }
  for (const [name, events] of Object.entries(logs)) {
    const points = [];
    for (const event of events) {
      points.push([x(event.race_depart_hours), y(event.depart_soc_pct)],
        [x(event.race_arrive_hours), y(event.soc_pct)]);
      if (event.control_hours) {
        points.push([x(event.race_arrive_hours + event.control_hours), y(event.soc_pct)]);
      }
    }
    const finish = events.at(-1).race_arrive_hours;
    polyline(ctx, points);
    ctx.strokeStyle = color(policyStyles[name].color);
    ctx.setLineDash(policyStyles[name].dash);
    ctx.lineWidth = 1.7;
    ctx.stroke();
    ctx.beginPath();
    ctx.arc(x(Math.min(hours, finish)), y(positions[name].soc), 3, 0, Math.PI * 2);
    ctx.fillStyle = ctx.strokeStyle;
    ctx.fill();
  }
  polyline(ctx, [[x(hours), top], [x(hours), bottom]]);
  ctx.setLineDash([2, 4]);
  ctx.strokeStyle = color("--muted");
  ctx.lineWidth = 1;
  ctx.stroke();
}
