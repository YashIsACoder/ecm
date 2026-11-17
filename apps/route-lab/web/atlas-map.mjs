import { australia, routes } from "./map-data.mjs";
import { drawCar, drawWeather, policyStyles, positionAt } from "./replay.mjs";

const terrainBounds = { west: 111, east: 155, south: -44, north: -9 };
const course = routes.outback;
const paper = "#f3e9ce";
const radians = Math.PI / 180;
const clamp = (value, low, high) => Math.max(low, Math.min(high, value));
const smooth = (value) => {
  const t = clamp(value, 0, 1);
  return t * t * (3 - 2 * t);
};

export const carColors = {
  sunswift: "#2b62af", vtc: "#b54129", solaride: "#368044", onda: "#c2861f", apollo: "#237c81", custom: "#6b4687",
};

export function loadTerrain() {
  return new Promise((resolve) => {
    const image = new Image();
    image.addEventListener("load", () => resolve(image), { once: true });
    image.addEventListener("error", () => resolve(null), { once: true });
    image.src = "./atlas-relief.webp";
  });
}

export function sunAt(ephemeris, hour, [longitude, latitude]) {
  const { vectors, step_minutes: step } = ephemeris;
  const index = clamp(hour * 60 / step, 0, vectors.length - 1);
  const low = Math.floor(index);
  const ratio = index - low;
  const next = vectors[Math.min(low + 1, vectors.length - 1)];
  const vector = vectors[low].map((value, axis) => value + (next[axis] - value) * ratio);
  const length = Math.hypot(...vector);
  const [x, y, z] = vector.map((value) => value / length);
  const lat = latitude * radians;
  const lon = longitude * radians;
  const meridian = x * Math.cos(lon) + y * Math.sin(lon);
  const east = y * Math.cos(lon) - x * Math.sin(lon);
  const north = z * Math.cos(lat) - meridian * Math.sin(lat);
  const up = z * Math.sin(lat) + meridian * Math.cos(lat);
  return {
    east, north, up, azimuth: (Math.atan2(east, north) / radians + 360) % 360,
    elevation: Math.asin(clamp(up, -1, 1)) / radians,
  };
}

export function geographicPosition(kilometres, distance) {
  return positionAt(course.points, kilometres, distance).slice(0, 2);
}

function projection(width, height, focus) {
  const marginX = width < 650 ? 12 : 22;
  const marginY = width < 650 ? 18 : 34;
  const xScale = (width - 2 * marginX) / 43;
  const yScale = (height - 2 * marginY) / 30.4;
  return focus
    ? ([lon, lat]) => [width / 2 + (lon - focus[0]) * xScale * 2.7, height / 2 - (lat - focus[1]) * yScale * 2.7]
    : ([lon, lat]) => [marginX + (lon - 111.5) * xScale, marginY + (-8.6 - lat) * yScale];
}

function line(ctx, points) {
  ctx.beginPath();
  for (const [index, point] of points.entries()) {
    if (index === 0) ctx.moveTo(...point);
    else ctx.lineTo(...point);
  }
}

function mapBackground(ctx, width, height, night) {
  ctx.fillStyle = paper;
  ctx.fillRect(0, 0, width, height);
  const sea = ctx.createLinearGradient(0, 0, width, 0);
  sea.addColorStop(0, paper);
  sea.addColorStop(.08, "#c6d1be");
  sea.addColorStop(.94, "#b7cdbd");
  sea.addColorStop(1, paper);
  ctx.fillStyle = sea;
  ctx.fillRect(0, 0, width, height);
  if (night === 0) return;
  ctx.globalAlpha = night * .72;
  const shadow = ctx.createLinearGradient(0, 0, width, 0);
  shadow.addColorStop(0, "#253c3700");
  shadow.addColorStop(.1, "#253c37");
  shadow.addColorStop(.93, "#253c37");
  shadow.addColorStop(1, "#253c3700");
  ctx.fillStyle = shadow;
  ctx.fillRect(0, 0, width, height);
  ctx.globalAlpha = 1;
}

function drawTerrain(ctx, project, texture, sun, night) {
  ctx.beginPath();
  for (const polygon of australia) {
    ctx.moveTo(...project(polygon[0]));
    for (const point of polygon.slice(1)) ctx.lineTo(...project(point));
    ctx.closePath();
  }
  ctx.save();
  ctx.clip();
  const [x, y] = project([terrainBounds.west, terrainBounds.north]);
  const [right, bottom] = project([terrainBounds.east, terrainBounds.south]);
  ctx.fillStyle = "#ecd2a0";
  ctx.fillRect(x, y, right - x, bottom - y);
  if (texture) {
    const { naturalWidth: columns, naturalHeight } = texture;
    const rows = naturalHeight / 5;
    const paint = (layer) => ctx.drawImage(texture, 0, rows * layer, columns, rows,
      x, y, right - x, bottom - y);
    paint(0);
    ctx.globalCompositeOperation = "multiply";
    const strength = .26 + .67 * smooth((sun.elevation + 3) / 12);
    const slopes = [sun.east, -sun.east, sun.north, -sun.north];
    for (const [index, slope] of slopes.entries()) {
      ctx.globalAlpha = Math.max(0, slope) * strength;
      if (ctx.globalAlpha > .01) paint(index + 1);
    }
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = "source-over";
  }
  const twilight = smooth((14 - sun.elevation) / 18) * (1 - night);
  ctx.fillStyle = `rgba(161, 85, 34, ${twilight * .18})`;
  ctx.fillRect(x, y, right - x, bottom - y);
  ctx.fillStyle = `rgba(27, 46, 43, ${night * .76})`;
  ctx.fillRect(x, y, right - x, bottom - y);
  ctx.restore();
  ctx.strokeStyle = night > .55 ? "#aab394" : "#61755d";
  ctx.lineWidth = .9;
  ctx.stroke();
}

const borders = [
  [[129, -14.9], [129, -31.7]], [[129, -26], [141, -26]],
  [[138, -16.8], [138, -26]], [[141, -26], [141, -37.9]],
  [[141, -29], [153, -29]],
];
const mapLabels = [
  ["T I M O R   S E A", 121.6, -12.7], ["C O R A L   S E A", 150.7, -14.7],
  ["W E S T E R N", 119.5, -24.3], ["A U S T R A L I A", 119.5, -25.3],
  ["N O R T H E R N", 131.3, -18.1], ["T E R R I T O R Y", 131.1, -19.0],
  ["Q U E E N S L A N D", 146, -22.3], ["S O U T H", 140.1, -27.6],
  ["A U S T R A L I A", 140.3, -28.5], ["G R E A T  A U S T R A L I A N  B I G H T", 127.7, -34.8],
  ["A U S T R A L I A", 127.0, -23.9],
];

function drawGrid(ctx, width, height, project, dark, overview) {
  ctx.strokeStyle = dark ? "#e4e4ae43" : "#4b665344";
  ctx.fillStyle = dark ? "#f7e7c9" : "#344e43";
  ctx.lineWidth = .6;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.font = "italic 12px Georgia, serif";
  for (let lon = 115; lon <= 155; lon += 5) {
    line(ctx, [project([lon, -8.9]), project([lon, -39])]);
    ctx.stroke();
    if (overview && (width > 530 || lon % 10 === 0)) ctx.fillText(`${lon}° E`, project([lon, 0])[0], height < 370 ? 9 : 19);
  }
  for (let lat = -10; lat >= -35; lat -= 5) {
    const west = project([111.5, lat]);
    line(ctx, [west, project([155, lat])]);
    ctx.stroke();
    if (overview && width >= 720) ctx.fillText(`${-lat}° S`, west[0] + 1, west[1] - 9);
  }
  ctx.setLineDash([3, 3]);
  for (const border of borders) {
    line(ctx, border.map(project));
    ctx.stroke();
  }
  ctx.setLineDash([]);
  if (!overview || width < 550) return;
  for (const [label, lon, lat] of mapLabels) {
    ctx.font = `${lon === 127.0 ? "600 18px" : "italic 11px"} Georgia, serif`;
    ctx.fillText(label, ...project([lon, lat]));
  }
}

function drawCourse(ctx, points, stops, dark, width) {
  line(ctx, points);
  ctx.strokeStyle = dark ? "#f2a282" : "#bc482b";
  ctx.lineWidth = 2.1;
  ctx.stroke();
  ctx.textBaseline = "middle";
  for (const [index, stop] of stops.entries()) {
    const [x, y] = points[stop.leg + 1];
    ctx.beginPath();
    ctx.arc(x, y, 4.4, 0, 2 * Math.PI);
    ctx.fillStyle = "#faf0cf";
    ctx.fill();
    ctx.strokeStyle = "#3a4630";
    ctx.lineWidth = 1.25;
    ctx.stroke();

    if (width < 500 && index % 2 !== 0 && index !== stops.length - 1 && stop.name !== "Alice Springs") continue;
    const left = stop.name === "Barrow Creek" || stop.name === "Erldunda" || stop.name === "Glendambo";
    const labelOffset = stop.name === "Alice Springs" ? 39 : 10;
    const textX = x + (left ? -labelOffset : labelOffset);
    ctx.textAlign = left ? "right" : "left";
    ctx.font = "700 13px Georgia, serif";
    ctx.strokeStyle = dark ? "#344139" : "#f4e4bd";
    ctx.fillStyle = dark ? "#faf0d8" : "#243123";
    ctx.lineWidth = 2.7;
    ctx.strokeText(stop.name, textX, y - 2);
    ctx.fillText(stop.name, textX, y - 2);
    if (width < 650 || !Number.isFinite(stop.km)) continue;
    ctx.font = "11px Georgia, serif";
    const distance = `${Math.round(stop.km).toLocaleString("en-AU")} km`;
    ctx.strokeText(distance, textX, y + 11);
    ctx.fillText(distance, textX, y + 11);
  }
}

export function drawCruiserAtlas(canvas, scene) {
  const { clientWidth: width, clientHeight: height } = canvas;
  if (width < 50 || height < 50) return 0;
  const ratio = Math.min(window.devicePixelRatio || 1, 2);
  if (canvas.width !== Math.round(width * ratio)) canvas.width = Math.round(width * ratio);
  if (canvas.height !== Math.round(height * ratio)) canvas.height = Math.round(height * ratio);
  const ctx = canvas.getContext("2d");
  if (!ctx) return 0;
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);

  const { kilometres, stops, racers, active, follow, sun, terrain, motion, showAllStops = false } = scene;
  const focus = follow && active ? geographicPosition(kilometres, active.state.distance) : null;
  const project = projection(width, height, focus);
  const night = 1 - smooth((sun.elevation + 12) / 17);
  const dark = night > .55;
  mapBackground(ctx, width, height, night);
  drawTerrain(ctx, project, terrain, sun, night);
  drawGrid(ctx, width, height, project, dark, !follow);
  const points = course.points.map(project);
  if (active?.state.phase === "drive" || active?.state.phase === "control") {
    drawWeather(ctx, positionAt(points, kilometres, active.state.distance), active.state.event, dark, motion);
  }
  drawCourse(ctx, points, stops, dark, width);
  for (const { id, index, state, paint = carColors[id] ?? policyStyles[id]?.paint } of racers) {
    const [roadX, roadY, angle] = positionAt(points, kilometres, state.distance);
    const stagger = (index - (racers.length - 1) / 2) * 13;
    const x = roadX - Math.sin(angle) * stagger;
    const y = roadY + Math.cos(angle) * stagger;
    drawCar(ctx, x, y, angle, paint, id === active?.id ? .85 : .66, state.depleted, id === active?.id);
    if ((!showAllStops && id !== active?.id) || (state.phase !== "control" && state.phase !== "overnight")) continue;
    const tag = state.phase === "control" ? "REST" : state.externalChargeWh > 0 ? "TOP-UP" : "PARKED";
    ctx.font = "700 10px Georgia, serif";
    ctx.textAlign = "center";
    const tagWidth = ctx.measureText(tag).width + 10;
    ctx.fillStyle = "#273923";
    const tagX = x + (showAllStops ? (index - (racers.length - 1) / 2) * 10 : 0);
    const tagY = y - 22 - (showAllStops ? index % 2 * 14 : 0);
    ctx.fillRect(tagX - tagWidth / 2, tagY - 7, tagWidth, 14);
    ctx.fillStyle = "#fff5db";
    ctx.fillText(tag, tagX, tagY);
  }
  // East–west scale at 25 degrees south, where longitude spans about 101 km/degree.
  return (project([121, -25])[0] - project([120, -25])[0]) / (111.195 * Math.cos(25 * radians));
}
