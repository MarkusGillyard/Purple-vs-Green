"""Create the seamless mineral, roughness, and height data for the Meridian masonry."""

import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 512
randomizer = random.Random(651103)


def noise(cells):
    image = Image.new("L", (cells * 3, cells * 3))
    patch = Image.frombytes("L", (cells, cells), randomizer.randbytes(cells * cells))
    for y in range(3):
        for x in range(3):
            image.paste(patch, (x * cells, y * cells))
    image = image.resize((SIZE * 3, SIZE * 3), Image.Resampling.BICUBIC)
    return list(image.crop((SIZE, SIZE, SIZE * 2, SIZE * 2)).getdata())


coarse, medium, stone, grain, pores = [noise(cells) for cells in (5, 14, 43, 118, 266)]
fissures = Image.new("L", (SIZE, SIZE))
pen = ImageDraw.Draw(fissures)
for _ in range(8):
    x, y = randomizer.randrange(SIZE), randomizer.randrange(SIZE)
    angle = randomizer.uniform(0, math.tau)
    points = [(x, y)]
    for _ in range(randomizer.randrange(8, 24)):
        angle += randomizer.uniform(-.67, .67)
        step = randomizer.uniform(3, 13)
        x += math.cos(angle) * step
        y += math.sin(angle) * step
        points.append((x, y))
    for oy in (-SIZE, 0, SIZE):
        for ox in (-SIZE, 0, SIZE):
            pen.line([(x + ox, y + oy) for x, y in points], fill=randomizer.randrange(110, 230), width=2)
seams = list(fissures.filter(ImageFilter.GaussianBlur(.55)).getdata())

pixels = []
for a, b, c, d, e, crack in zip(coarse, medium, stone, grain, pores, seams):
    mineral = (a - 128) * .36 + (b - 128) * .32 + (c - 128) * .25
    grain_variation = (d - 128) * .20 + (e - 128) * .16
    height = 133 + mineral * .53 + (c - 128) * .12 + grain_variation - crack * .31
    color = 157 + mineral + grain_variation - crack * .29 - max(0, 78 - e) * .24
    roughness = 164 + (b - 128) * .18 + (d - 128) * .11 + crack * .11
    pixels.append(tuple(max(0, min(255, round(channel))) for channel in (color, roughness, height)))

image = Image.new("RGB", (SIZE, SIZE))
image.putdata(pixels)
image.save(Path(sys.argv[1]) / "T_MeridianMasonry.png")
