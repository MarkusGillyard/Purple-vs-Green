"""Build leather casting gloves and fitted forearm armor in centimeters."""
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Vector

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = .01
out = Path(sys.argv[sys.argv.index("--") + 1])
out.mkdir(parents=True, exist_ok=True)
rng = random.Random(7054)
materials = [bpy.data.materials.new(name) for name in
             ("Leather", "Cloth", "Steel", "Bronze", "Livery", "Thread")]


class Mesh:
    def __init__(self):
        self.vertices, self.faces, self.slots = [], [], []
        self.shades, self.uvs = [], []

    def add(self, rings, slot, wrap=True, caps=True):
        start, width = len(self.vertices), len(rings[0])
        for row, ring in enumerate(rings):
            for column, (point, shade) in enumerate(ring):
                self.vertices.append(point)
                self.shades.append(shade * rng.uniform(.94, 1.04))
                self.uvs.append((column / width, row / max(1, len(rings) - 1)))

        def face(indices):
            self.faces.append(tuple(start + index for index in indices))
            self.slots.append(slot)

        if caps and wrap:
            face(reversed(range(width)))
        for row in range(len(rings) - 1):
            for j in range(width if wrap else width - 1):
                a, b = row * width + j, row * width + (j + 1) % width
                face((a, b, b + width, a + width))
        if caps and wrap:
            face(range((len(rings) - 1) * width, len(rings) * width))

    def loft(self, sections, slot, sides=20, folds=0.):
        rings = []
        for i, (x, y, z, ry, rz) in enumerate(sections):
            ring = []
            for j in range(sides):
                angle = math.tau * j / sides
                crease = folds * math.sin(5 * angle + 1.6 * i)
                ring.append(((x, y + (ry + crease) * math.cos(angle),
                              z + (rz + crease) * math.sin(angle)),
                             .88 + .10 * math.sin(angle) + crease * .07))
            rings.append(ring)
        self.add(rings, slot)

    def cord(self, path, radius, slot):
        rings = []
        points = [Vector(point) for point in path]
        for i, point in enumerate(points):
            tangent = (points[min(i + 1, len(points) - 1)] - points[max(0, i - 1)]).normalized()
            reference = Vector((0, 0, 1)) if abs(tangent.z) < .95 else Vector((0, 1, 0))
            side = reference.cross(tangent).normalized()
            up = tangent.cross(side)
            rings.append([(point + radius * (math.cos(a) * side + math.sin(a) * up), .95)
                          for a in (math.tau * j / 6 for j in range(6))])
        self.add(rings, slot)

    def export(self, name, side=1):
        # The Unreal importer converts FBX front (-Y) to engine forward (+X).
        vertices = [(side * y, -x, z) for x, y, z in self.vertices]
        faces = self.faces if side == 1 else [face[::-1] for face in self.faces]
        data = bpy.data.meshes.new(name)
        data.from_pydata(vertices, [], faces)
        data.update()
        colors = data.color_attributes.new(name="Wear", type="FLOAT_COLOR", domain="POINT")
        for item, shade in zip(colors.data, self.shades):
            item.color = (shade, shade, shade, 1.)
        uv = data.uv_layers.new(name="Surface")
        for polygon in data.polygons:
            coords = [self.uvs[data.loops[i].vertex_index] for i in polygon.loop_indices]
            cap = len(coords) != 4
            seam = not cap and max(u for u, v in coords) - min(u for u, v in coords) > .5
            for i, (u, v) in zip(polygon.loop_indices, coords):
                if cap:
                    u, v = .5 + .49 * math.cos(math.tau * u), .5 + .49 * math.sin(math.tau * u)
                elif seam and u == 0.:
                    u = 1.
                uv.data[i].uv = (u, v)
        for material in materials:
            data.materials.append(material)
        for polygon, slot in zip(data.polygons, self.slots):
            polygon.material_index = slot
            polygon.use_smooth = len(polygon.vertices) == 4
        obj = bpy.data.objects.new(name, data)
        bpy.context.collection.objects.link(obj)
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.export_scene.fbx(filepath=str(out / f"{name}.fbx"), use_selection=True,
                                 object_types={"MESH"}, axis_forward="-Y", axis_up="Z",
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
                                 mesh_smooth_type="FACE", colors_type="LINEAR")
        bpy.data.objects.remove(obj, do_unlink=True)


arm = Mesh()
sleeve = [(-48, .8, -2.4, 7.9, 7.0), (-40, .9, -2, 8.7, 7.4),
          (-34, .7, -1.7, 8.2, 7), (-28, .6, -1.3, 7.9, 6.4),
          (-25, .5, -1.2, 7.4, 6.4), (-20, .3, -1, 6.7, 5.5),
          (-14, .2, -.8, 5.8, 4.7), (-8, 0, -.6, 4.4, 3.5),
          (-4, 0, -.4, 3.8, 2.8)]
arm.loft(sleeve, 1, folds=.43)
arm.loft([(-8.5, 0, -.6, 4.65, 3.73), (-8.1, 0, -.6, 4.85, 3.98),
          (-4.4, 0, -.5, 4.23, 3.26), (-4, 0, -.5, 3.95, 2.93)], 0)


def sleeve_at(x):
    for start, end in zip(sleeve, sleeve[1:]):
        if start[0] <= x <= end[0]:
            t = (x - start[0]) / (end[0] - start[0])
            return tuple(a + (b - a) * t for a, b in zip(start[1:], end[1:]))
    raise ValueError(f"No sleeve cross section for {x}")


def over_sleeve(x, angle, offset):
    y, z, ry, rz = sleeve_at(x)
    return x, y + (ry + offset) * math.cos(angle), z + (rz + offset) * math.sin(angle)


# Thin solid plates sit above the cloth with rolled edges and room for the
# wrist to flex. The inlay follows the crown of each plate.
for back, front in ((-37.2, -26.), (-24.6, -15.9), (-14.4, -9.0)):
    rings, badge = [], []
    for fraction, sweep, height in ((0., .70, .82), (.09, .92, 1.21), (.30, 1., 1.29),
                                    (.67, .95, 1.24), (.88, .74, 1.11), (1., .58, .76)):
        x = back + (front - back) * fraction
        angles = [math.pi / 2 + sweep * math.radians(-67 + 134 * j / 16) for j in range(17)]
        crown = [(over_sleeve(x, angle, height + .22 * math.sin(angle)),
                  .97 if j in (0, 16) or fraction in (0., 1.) else .82 + .11 * math.sin(angle))
                 for j, angle in enumerate(angles)]
        underside = [(over_sleeve(x, angle, height - .24), .67) for angle in reversed(angles)]
        rings.append(crown + underside)
        width = math.radians(2.2 if fraction in (0., 1.) else 7.)
        badge.append([(over_sleeve(x, math.pi / 2 + step * width, height + .29), .95)
                      for step in (-1., 0., 1.)])
    arm.add(rings, 2)
    arm.add(badge, 4, wrap=False)
    arm.cord([point for point, _ in rings[-2][:17]], .115, 2)
    for j in (3, 13):
        x, y, z = rings[2][j][0]
        arm.loft([(x - .26, y, z + .01, .22, .16), (x, y, z + .06, .32, .20),
                  (x + .28, y, z + .01, .22, .16)], 3, sides=10)

for back, front in ((-39.7, -37.6), (-25.9, -24.7), (-15.8, -14.5)):
    band = []
    for x, offset in ((back, .39), (back + .17, .62), (front - .17, .60), (front, .39)):
        y, z, ry, rz = sleeve_at(x)
        band.append((x, y, z, ry + offset, rz + offset))
    arm.loft(band, 0)

# The metacarpals fan out from a narrow cuff, and the thumb leaves the palm
# obliquely. The offset across the wrist keeps the two hands mirrored.
arm.loft([(-5.6, 0, -.46, 3.6, 2.58), (-3, 0, -.3, 3.45, 2.35),
          (0, -.1, -.14, 3.65, 2.15), (2.7, -.35, -.12, 4.6, 2.37),
          (5.6, -.2, -.17, 4.84, 2.01), (7.7, .04, -.32, 4.7, 1.54),
          (8.75, -.38, -.35, 3.48, 1.03)], 0)
arm.loft([(1.7, -2.7, -.72, 1.65, 1.61), (3.1, -4.1, -1.10, 1.68, 1.53),
          (4.7, -5.55, -1.46, 1.43, 1.31), (6.4, -6.5, -1.65, 1.17, 1.18),
          (8.1, -6.7, -1.93, .93, .94), (9.25, -6.43, -2.52, .63, .64),
          (9.60, -6.19, -2.77, .18, .24)], 0, sides=16)
arm.cord([(-3.9, 2.5, 1.17), (0, 3.2, 1.08), (3.2, 4.25, .8), (6.3, 4.35, .6),
          (7.4, 3.6, .79)], .11, 0)
for i in range(9):
    x = -1.5 + .93 * i
    y = 2.8 + .19 * i - .019 * i * i
    z = 2.0 - .014 * i * i
    arm.cord([(x, y, z), (x + .29, y + .22, z - .04)], .073, 5)
for y in (-2.4, -.7, 1.1):
    arm.cord([(1.7, y * .65, 2.08), (3.75, y * .88, 2.12),
              (5.4, y, 1.88), (6.2, y * 1.05, 1.69)], .10, 0)

arm.export("SM_CastingArm_R")
arm.export("SM_CastingArm_L", -1)

proximal = Mesh()
proximal.loft([(-.7, 0, -.02, 1.10, .94), (0., 0, 0, 1.24, 1.17),
               (.8, 0, .02, 1.24, 1.19), (1.8, 0, 0, 1.06, .97),
               (3.1, 0, -.06, .95, .88), (4., 0, -.10, .96, .87),
               (4.58, 0, -.14, .79, .72)], 0, sides=16)
for x in (2.9, 3.36):
    proximal.cord([(x, -.67, .58), (x + .07, 0, .84), (x, .67, .58)], .062, 0)
proximal.export("SM_CastingFinger")

distal = Mesh()
distal.loft([(-.6, 0, .02, .77, .68), (0., 0, 0, .93, .84),
             (1.05, 0, -.08, .88, .8), (1.61, 0, -.20, .85, .73),
             (2.65, 0, -.47, .74, .65), (3.22, 0, -.59, .59, .56),
             (3.52, 0, -.62, .24, .25)], 0, sides=16)
distal.cord([(1.25, -.60, .45), (1.31, 0, .72), (1.25, .60, .45)], .06, 0)
distal.export("SM_CastingFingertip")
