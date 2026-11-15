import { australia, routes } from "./map-data.mjs";

export const policyStyles = {
  steady: { color: "--teal", dash: [] },
  push: { color: "--rust", dash: [7, 4] },
  "reserve-first": { color: "--blue", dash: [2, 4] },
};

const interpolate = (start, end, ratio) => start + (end - start) * ratio;

export function traceAt(events, hours) {
  const index = events.findIndex((event) => hours < event.elapsed_hours);
  const reached = index === -1 ? events.length : index;
  const previous = events[reached - 1];
  if (index === -1) {
    return { distance: previous.distance_km, soc: previous.soc_pct, reached, done: true,
      depleted: Boolean(previous.depleted) };
  }
  const next = events[index];
  const start = previous?.elapsed_hours ?? 0;
  const ratio = (hours - start) / (next.elapsed_hours - start);
  return {
    distance: interpolate(previous?.distance_km ?? 0, next.distance_km, ratio),
    soc: interpolate(next.depart_soc_pct, next.soc_pct, ratio),
    reached, done: false, depleted: false,
  };
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

export function drawRouteMap(canvas, trace, positions, selected) {
  const frame = prepare(canvas);
  if (!frame) return;
  const { ctx, width, height, color } = frame;
  const route = routes[trace.route];
  const project = mapProjection(route.geographic, width, height);
  const points = route.points.map(project);
  const kilometres = [0, ...trace.points.map((point) => point.km)];
  ctx.fillStyle = color("--field");
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
      ctx.fillStyle = "#20372b";
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
  polyline(ctx, points);
  ctx.strokeStyle = "#0c1913";
  ctx.lineWidth = 7;
  ctx.stroke();
  ctx.strokeStyle = "#b7d576";
  ctx.lineWidth = 2;
  ctx.stroke();
  for (const [x, y] of points) {
    ctx.beginPath();
    ctx.arc(x, y, 2.1, 0, Math.PI * 2);
    ctx.fillStyle = "#ecf5d2";
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
  Object.entries(positions).forEach(([policy, state], index) => {
    const [x, y, angle] = positionAt(points, kilometres, state.distance);
    const offset = (index - 1) * (width < 420 ? 18 : 22);
    drawCar(ctx, x - Math.sin(angle) * offset, y + Math.cos(angle) * offset, angle,
      color(policyStyles[policy].color), width < 420 ? .78 : .9, state.depleted, policy === selected);
  });
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
    let previous = 0;
    for (const event of events) {
      points.push([x(previous), y(event.depart_soc_pct)], [x(event.elapsed_hours), y(event.soc_pct)]);
      previous = event.elapsed_hours;
    }
    polyline(ctx, points);
    ctx.strokeStyle = color(policyStyles[name].color);
    ctx.setLineDash(policyStyles[name].dash);
    ctx.lineWidth = 1.7;
    ctx.stroke();
    ctx.beginPath();
    ctx.arc(x(Math.min(hours, previous)), y(positions[name].soc), 3, 0, Math.PI * 2);
    ctx.fillStyle = ctx.strokeStyle;
    ctx.fill();
  }
  polyline(ctx, [[x(hours), top], [x(hours), bottom]]);
  ctx.setLineDash([2, 4]);
  ctx.strokeStyle = color("--muted");
  ctx.lineWidth = 1;
  ctx.stroke();
}
