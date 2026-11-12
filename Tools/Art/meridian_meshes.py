"""Author the surrounding ridge and the armillary crystal in Unreal centimeters."""

import math
import random
import sys
from pathlib import Path

import bpy

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = .01
out = Path(sys.argv[sys.argv.index("--") + 1])


def export(name, vertices, polygons, palette=None, smooth=False, uvs=None):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], polygons)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    if palette:
        color = mesh.color_attributes.new(name="Surface", type="BYTE_COLOR", domain="POINT")
        for entry, value in zip(color.data, palette):
            entry.color_srgb = (*value, 1)
    if uvs:
        uv_layer = mesh.uv_layers.new(name="Surface")
        for corner, loop in zip(uv_layer.data, mesh.loops):
            corner.uv = uvs[loop.vertex_index]
    for face in mesh.polygons:
        face.use_smooth = smooth
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(out / f"{name}.fbx"), use_selection=True, object_types={"MESH"},
        axis_forward="-Y", axis_up="Z", apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE",
    )


vertices, faces, colors = [], [], []
segments = 192
radii = (2400, 3850, 4900, 6100, 7800, 9600, 12300, 15300,
         19500, 24000, 30000, 37500, 46800, 57500, 72000, 90000)
for ring, radius in enumerate(radii):
    for segment in range(segments):
        angle = math.tau * segment / segments
        x = radius * math.cos(angle)
        y = radius * math.sin(angle)
        band = max(0, min(1, (radius - 3800) / 5100))
        near = math.exp(-((radius - 9500 - 1600 * math.sin(angle * 3)) / 5600) ** 2)
        far = math.exp(-((radius - 29000 - 4200 * math.sin(angle * 5)) / 13500) ** 2)
        peaks = (.34 + .37 * abs(math.sin(angle * 3 - .8)) ** 2
                 + .30 * abs(math.cos(angle * 7 + .8)) ** 5)
        strata = (math.sin(x * .0014 + y * .0027) + math.sin(x * .0031 - y * .0020)) * 112
        ridge = abs(math.sin(angle * 15 + radius * .00052)) * 420
        z = -430 + band * (near * (1950 * peaks + ridge) + far * (6980 * peaks + ridge) + strata)
        vertices.append((x, y, z))
        light = .86 + .1 * math.sin(angle * 21 + radius * .0013) + .08 * math.cos(angle * 37)
        high = max(0, min(1, (z - 2400 + 280 * math.cos(angle * 16)) / 2200))
        rock = (.28, .31, .32) if ring < 7 else (.32, .36, .39)
        snow = (.68, .72, .73)
        colors.append(tuple((base * (1 - high) + bright * high) * light for base, bright in zip(rock, snow)))
for ring in range(len(radii) - 1):
    for j in range(segments):
        a = ring * segments + j
        b = ring * segments + (j + 1) % segments
        faces.extend(((a, a + segments, b + segments), (a, b + segments, b)))
export("SM_MeridianRidge", vertices, faces, colors, True)

vertices = [(0, 0, -91)]
faces = []
for height, radius, turn in [(-21, 37, 0), (17, 53, .16), (57, 28, .32)]:
    vertices.extend((radius * math.cos(j * math.tau / 7 + turn),
                     radius * math.sin(j * math.tau / 7 + turn), height)
                    for j in range(7))
vertices.append((0, 0, 102))
for j in range(7):
    following = (j + 1) % 7
    faces.extend(((0, following + 1, j + 1), (15 + j, 15 + following, 22)))
    for ring in (1, 8):
        a, b = ring + j, ring + following
        faces.extend(((a, b, a + 7), (b, b + 7, a + 7)))
export("SM_MeridianCrystal", vertices, faces)

# A tapered plate above the leather sleeve, with a turned lip around its edge.
vertices, faces = [], []
rows = [(-50, .68), (-43, .84), (-24, .94), (11, 1.0), (37, .94), (50, .62)]
segments = 8
for inset in (0, -5):
    for z, width in rows:
        for j in range(segments + 1):
            angle = math.radians(-51 + j * 102 / segments)
            r = 43 + inset + (3 if j == segments // 2 else 0)
            vertices.append((math.cos(angle) * r, math.sin(angle) * (49 + inset) * width, z))
cols = segments + 1
half = len(rows) * cols
for y in range(len(rows) - 1):
    for j in range(segments):
        a = y * cols + j
        quad = (a, a + 1, a + cols + 1, a + cols)
        faces.append(quad)
        faces.append(tuple(v + half for v in reversed(quad)))
for j in range(segments):
    a = j
    b = half - cols + j
    faces.extend(((a, a + half, a + half + 1, a + 1), (b, b + 1, b + half + 1, b + half)))
for y in range(len(rows) - 1):
    a = y * cols
    b = a + segments
    faces.extend(((a, a + cols, a + cols + half, a + half), (b, b + half, b + cols + half, b + cols)))
uvs = [((i % cols) / segments, (point[2] + 50) / 100) for i, point in enumerate(vertices)]
export("SM_MeridianVambrace", vertices, faces, uvs=uvs)

rng = random.Random(4072)
vertices, faces, colors = [], [], []
for _ in range(27):
    cx, cy = (rng.uniform(-24, 24) for _ in range(2))
    yaw, height, width = rng.uniform(0, math.tau), rng.uniform(29, 82), rng.uniform(1.8, 4.9)
    dx, dy = math.cos(yaw), math.sin(yaw)
    blade = len(vertices)
    shade = rng.uniform(.74, 1.12)
    for row, span in zip((0, .42, .73, 1), (1., .80, .49, 0.)):
        bend = row ** 1.9 * height * .49
        dark, light = (.25, .30, .19), (.57, .54, .32)
        for side in (-1, 1):
            vertices.append((cx + dx * bend - dy * width * span * side,
                             cy + dy * bend + dx * width * span * side, height * row))
            colors.append(tuple((lo * (1 - row) + hi * row) * shade for lo, hi in zip(dark, light)))
    for row in range(3):
        a = blade + row * 2
        quad = (a, a + 1, a + 3, a + 2)
        faces.extend((quad, quad[::-1]))
export("SM_MeridianReeds", vertices, faces, colors)

# Chamfered ashlar is rendered over the existing simple cover collision.
bpy.ops.mesh.primitive_cube_add(size=100.)
block = bpy.context.object
bevel = block.modifiers.new("Worn edges", "BEVEL")
bevel.width = 4.2
bevel.segments = 2
bpy.ops.object.modifier_apply(modifier=bevel.name)
export("SM_MeridianAshlar", [tuple(v.co) for v in block.data.vertices],
       [tuple(face.vertices) for face in block.data.polygons])
