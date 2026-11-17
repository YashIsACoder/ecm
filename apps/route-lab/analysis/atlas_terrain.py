# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "pillow"]
# ///
"""Build the atlas relief sprite from Mapzen Terrarium elevation tiles.

Run from any directory with uv run apps/route-lab/analysis/atlas_terrain.py.
The web app reads the resulting image locally, without a tile service or token.
"""

from concurrent.futures import ThreadPoolExecutor
from io import BytesIO
import json
from pathlib import Path
from urllib.request import urlopen

import numpy as np
from PIL import Image


# Match terrainBounds in atlas.mjs. Raster rows run north to south.
WEST, SOUTH, EAST, NORTH = 111, -44, 155, -9
WIDTH, HEIGHT = 1120, 896
ZOOM = 5
WORLD_PIXELS = 256 * 2**ZOOM
OUTPUT = Path(__file__).resolve().parents[1] / "web" / "atlas-relief.webp"
COAST_SOURCE = ("https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/"
                "geojson/ne_50m_admin_0_countries.geojson")


def download_tile(tile: tuple[int, int]) -> tuple[int, int, np.ndarray]:
    x, y = tile
    url = f"https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{ZOOM}/{x}/{y}.png"
    with urlopen(url, timeout=30) as response:
        image = np.asarray(Image.open(BytesIO(response.read())).convert("RGB"), dtype=np.float32)
    return x, y, image[:, :, 0] * 256 + image[:, :, 1] + image[:, :, 2] / 256 - 32768


def elevation_grid() -> np.ndarray:
    longitudes = WEST + (np.arange(WIDTH) + .5) / WIDTH * (EAST - WEST)
    latitudes = NORTH - (np.arange(HEIGHT) + .5) / HEIGHT * (NORTH - SOUTH)
    x = (longitudes + 180) / 360 * WORLD_PIXELS - .5
    y = (1 - np.arcsinh(np.tan(np.deg2rad(latitudes))) / np.pi) / 2 * WORLD_PIXELS - .5
    left, right = int(x[0]) // 256, (int(x[-1]) + 1) // 256
    top, bottom = int(y[0]) // 256, (int(y[-1]) + 1) // 256
    raw = np.empty(((bottom - top + 1) * 256, (right - left + 1) * 256), dtype=np.float32)
    with ThreadPoolExecutor(max_workers=6) as executor:
        for tile_x, tile_y, tile in executor.map(
            download_tile, ((i, j) for i in range(left, right + 1) for j in range(top, bottom + 1))
        ):
            col, row = (tile_x - left) * 256, (tile_y - top) * 256
            raw[row:row + 256, col:col + 256] = tile
    x -= left * 256
    y -= top * 256
    cols, rows = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = (x - cols)[None, :], (y - rows)[:, None]
    upper = raw[rows[:, None], cols] * (1 - fx) + raw[rows[:, None], cols + 1] * fx
    lower = raw[rows[:, None] + 1, cols] * (1 - fx) + raw[rows[:, None] + 1, cols + 1] * fx
    return upper * (1 - fy) + lower * fy


def main() -> None:
    # The vector coastline clips the relief. Avoid artificial slopes at the shore.
    height = np.maximum(elevation_grid(), 0)
    metres_per_row = (NORTH - SOUTH) / HEIGHT * 111_195
    latitudes = np.linspace(NORTH, SOUTH, HEIGHT)
    metres_per_column = (EAST - WEST) / WIDTH * 111_195 * np.cos(np.deg2rad(latitudes))[:, None]
    south, east = np.gradient(height)
    dx, dy = east / metres_per_column, -south / metres_per_row

    altitude = np.clip(height / 1700, 0, 1)
    grain = np.random.default_rng(2025).normal(0, 1.8, height.shape)
    base = np.array([244, 220, 163]) - altitude[:, :, None] * np.array([19, 32, 49])
    base = base + grain[:, :, None]

    sprite = Image.new("RGB", (WIDTH, HEIGHT * 5))
    sprite.paste(Image.fromarray(np.clip(base, 0, 255).astype("uint8")), (0, 0))
    # White is neutral under multiply. The four lobes shade slopes facing away
    # from east, west, north and south light respectively. Scale is illustrative.
    for index, slope in enumerate([dx, -dx, dy, -dy], start=1):
        shadow = np.clip(1 - np.maximum(0, slope) * 18, .28, 1)
        layer = Image.fromarray(np.round(shadow * 255).astype("uint8")).convert("RGB")
        sprite.paste(layer, (0, HEIGHT * index))
    sprite.save(OUTPUT, format="WEBP", quality=82, method=6)
    print(f"{OUTPUT.name}: {OUTPUT.stat().st_size:,} bytes")
    write_coastline()


def write_coastline() -> None:
    with urlopen(COAST_SOURCE, timeout=30) as response:
        countries = json.load(response)["features"]
    geometry = next(feature["geometry"] for feature in countries if feature["properties"]["ADMIN"] == "Australia")
    coast = []
    for outer, *_ in geometry["coordinates"]:
        if any(WEST <= lon <= EAST and SOUTH <= lat <= NORTH for lon, lat in outer):
            coast.append([[round(lon, 3), round(lat, 3)] for lon, lat in outer])
    path = OUTPUT.with_name("map-data.mjs")
    waypoints = path.read_text().split("// Waypoints", maxsplit=1)[1]
    lines = ["// Australia coastline from Natural Earth 1:50m v5.1.2, public domain.",
             "export const australia = ["]
    for polygon in coast:
        lines.append("  [")
        for offset in range(0, len(polygon), 6):
            points = (json.dumps(point, separators=(",", ":")) for point in polygon[offset:offset + 6])
            lines.append("    " + ", ".join(points) + ",")
        lines.append("  ],")
    lines.extend(["];", "", "// Waypoints" + waypoints])
    path.write_text("\n".join(lines))


if __name__ == "__main__":
    main()
