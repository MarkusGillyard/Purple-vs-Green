"""Build the Meridian battlemage's original fitted armor and shared skeleton."""
import math
import sys

import bpy
from mathutils import Vector

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = .01

def material(name, color):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.)
    return mat

iron = material("Obsidian", (.07, .105, .13))
gold = material("Bronze", (.57, .31, .082))
cloth = material("Livery", (.41, .13, .62))
dark = material("Gaps", (.018, .024, .04))
materials = [iron, gold, cloth, dark]

rig_data = bpy.data.armatures.new("MeridianRig")
rig = bpy.data.objects.new("MeridianRig", rig_data)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")

def bone(name, start, end, parent=None):
    joint = rig_data.edit_bones.new(name)
    joint.head, joint.tail = start, end
    if parent:
        joint.parent = rig_data.edit_bones[parent]

bone("Root", (0, 0, 0), (0, 0, 12))
bone("Pelvis", (0, 0, 87), (0, 0, 107), "Root")
bone("Chest", (0, 0, 107), (0, 0, 144), "Pelvis")
bone("Head", (0, 0, 146), (0, 0, 172), "Chest")
for tag, side in (("L", -1), ("R", 1)):
    bone(f"UpperArm_{tag}", (21*side, 0, 135), (29*side, 0, 108), "Chest")
    bone(f"Forearm_{tag}", (29*side, 0, 108), (31*side, -2, 87), f"UpperArm_{tag}")
    bone(f"Hand_{tag}", (31*side, -2, 87), (32*side, -2, 77), f"Forearm_{tag}")
    bone(f"Thigh_{tag}", (11*side, 0, 87), (12*side, -2, 48), "Pelvis")
    bone(f"Shin_{tag}", (12*side, -2, 48), (13*side, 0, 13), f"Thigh_{tag}")
    bone(f"Foot_{tag}", (13*side, 0, 13), (13*side, -11, 4), f"Shin_{tag}")
bpy.ops.object.mode_set(mode="OBJECT")

vertices, faces, colors, weights, smoothing = [], [], [], [], []

def shell(owner, color, rings, sides=16, rotation=math.pi/16):
    """Loft elliptical cross sections in local centimeters, front toward negative Y."""
    first = len(vertices)
    for x, y, z, rx, ry in rings:
        for k in range(sides):
            a = rotation + math.tau*k/sides
            vertices.append((x + math.cos(a)*rx, y + math.sin(a)*ry, z))
            weights.append(owner)
    def face(indices, smooth=False):
        faces.append(tuple(indices))
        colors.append(color)
        smoothing.append(smooth)
    face(reversed(range(first, first+sides)))
    for j in range(len(rings)-1):
        bottom = first + j*sides
        top = bottom+sides
        for k in range(sides):
            nxt = (k+1) % sides
            face([bottom+k, bottom+nxt, top+nxt, top+k], smooth=True)
    face(range(first+(len(rings)-1)*sides, first+len(rings)*sides))

def plate(owner, color, contour, depth):
    """An angular relief with its exposed contour facing the arena."""
    first = len(vertices)
    count = len(contour)
    vertices.extend(contour)
    vertices.extend((x, y+depth, z) for x,y,z in contour)
    weights.extend([owner]*count*2)
    faces.append(tuple(first+i for i in range(count)))
    faces.append(tuple(first+count+i for i in reversed(range(count))))
    colors.extend([color]*2)
    smoothing.extend([False]*2)
    for i in range(count):
        j = (i+1) % count
        faces.append((first+i, first+j, first+count+j, first+count+i))
        colors.append(color)
        smoothing.append(False)

shell("Pelvis", 3, [(0,0,79,15,9),(0,0,87,18,11),(0,0,95,16,10),(0,0,103,15,9)])
shell("Pelvis", 3, [(0,0,92,17.1,10.8),(0,0,93,17.7,11.6),(0,0,97,17.5,11.3),(0,0,98,17,10.4)])
plate("Pelvis", 1, [(-3.6,-11.9,97.5),(-4,-12.2,93),(-2.5,-12.7,92),(3,-12.7,92),
                     (4,-12.2,93),(3.6,-11.9,97.5)], .9)
shell("Chest", 3, [(0,1,96,14,9),(0,1,105,15,10),(0,0,119,20,12),(0,0,133,21,11),
                    (0,0,141,16,8),(0,0,145,7.5,6.6)])
# Rounded cuirass over a fitted gambeson.
shell("Chest", 0, [(0,1,102,14.6,10),(0,0,109,16.2,11.8),(0,-1,121,20.2,13.9),
                    (0,0,130,21.5,13.1),(0,0,137,19.5,10.9),(0,0,141,14.2,8.9)])
plate("Chest", 2, [(-9,-11.8,138),(-7,-14.1,130),(-4,-14.8,121),(-2,-14.5,111),
                    (2,-14.5,111),(4,-14.8,121),(7,-14.1,130),(9,-11.8,138),(0,-12.6,135)], 1.4)
plate("Chest", 1, [(-3.3,-14.5,130),(0,-16.2,124),(3.3,-14.5,130),(0,-15.3,134)], 1.2)
shell("Chest", 3, [(0,0,139,12,8.7),(0,0,141,12.2,8.9),(0,0,145,8.2,7.5)])
shell("Head", 3, [(0,0,143,6.6,6.6),(0,-.8,150,7.6,7.5),(0,-.4,156,8.2,8.9)])
shell("Head", 0, [(0,-1.4,150,7.8,8.5),(0,-1.9,153,9.9,10.4),(0,-1.2,158,11.3,11.9),
                   (0,.15,164,12,12.8),(0,.9,169.2,11.3,12.1),(0,1.5,173.5,9,10),
                   (0,1.1,176,5,6.5),(0,1.1,177,2.8,3.5)], sides=24)
# A recessed eye band between brow and cheek guards.
plate("Head", 3, [(-10.6,-8.4,164),(-8.2,-11.5,161.7),(-3.7,-13,161.4),
                   (0,-14.4,162.7),(3.7,-13,161.4),(8.2,-11.5,161.7),
                   (10.6,-8.4,164),(6.9,-12.4,166.4),(0,-14,167),(-6.9,-12.4,166.4)], 1.3)
plate("Head", 1, [(-10.7,-8.4,164.8),(-7.6,-12.4,166.8),(0,-14.5,168),
                   (7.6,-12.4,166.8),(10.7,-8.4,164.8),(5.8,-12.5,165.8),
                   (0,-14.8,166.1),(-5.8,-12.5,165.8)], 1.)
plate("Head", 0, [(-9.1,-10,161.1),(-7.6,-12.2,153.7),(-3.8,-13.8,150.7),
                   (3.8,-13.8,150.7),(7.6,-12.2,153.7),(9.1,-10,161.1),
                   (4,-13.6,160.3),(0,-15.6,164),(-4,-13.6,160.3)], 1.7)
for x in (-4.7, -2.5, 2.5, 4.7):
    y = -14.8 + abs(x) * .28
    plate("Head", 3, [(x-.45,y,153.3),(x+.36,y,153.4),(x+.6,y-.22,157.6),(x-.16,y-.22,157.5)], .4)
plate("Pelvis", 2, [(-12,-10.7,94),(-9,-11.1,82),(-8,-12,69),(-5,-12.4,63),
                    (0,-13.2,66),(6,-12.5,63),(9,-11.5,73),(12,-10.6,94),
                    (3,-12.3,91),(-4,-11.8,92)], 1.)

for tag, side in (("L", -1), ("R", 1)):
    upper = f"UpperArm_{tag}"
    fore = f"Forearm_{tag}"
    hand = f"Hand_{tag}"
    thigh = f"Thigh_{tag}"
    shin = f"Shin_{tag}"
    foot = f"Foot_{tag}"
    shell(upper, 3, [(27*side,0,104,5,5.3),(28*side,0,110,6.1,6.7),(26*side,0,120,7.7,7.6),
                     (23*side,0,133,8.2,8.5),(22*side,0,141,7.2,7.4)])
    shell(upper, 2, [(26*side,0,121,7.9,8),(25*side,0,128,9.4,9.4),(23*side,0,136,9.6,9.3)])
    shell(upper, 0, [(27*side,0,128,8.1,9.2),(26*side,0,130,10.4,10.7),(24*side,0,138,10.3,10.6),
                     (23*side,0,143,8.9,8.7),(22*side,0,146,4.6,5.5)])
    shell(upper, 0, [(27.5*side,0,124,7.1,8.1),(27*side,0,126,8.8,9.1),(26*side,0,132,9.2,9.7)])
    shell(fore, 3, [(31*side,-2,85,4.1,4.4),(30*side,-1,94,5.5,5.8),(29*side,0,103,6.6,6.4),
                    (29*side,0,112,5.6,5.8)])
    for z, width in ((88, 4.9), (95, 5.8), (101, 6.5)):
        x = (31 - (z - 88) * .095) * side
        y = -1.7 + (z - 88) * .09
        shell(fore, 0, [(x,y,z,width,width),(x,y,z+1,width+.4,width+.45),
                        (x-.25*side,y+.3,z+6,width+.75,width+.8)])
    shell(hand, 3, [(32*side,-2,78,3.4,3.2),(32*side,-2,80,4.2,4.6),
                    (31.4*side,-1.8,83.3,4,4.55),(31*side,-2,87.5,3.5,3.7)])
    for offset in (-2.9, -.95, 1.02, 2.85):
        y = -2 + offset
        x = 32*side
        bottom = 76 + .32 * abs(offset)
        shell(hand, 3, [(x-.65*side,y,bottom,.8,.7),(x+.4*side,y,bottom+1.2,1.3,.93),
                        (x+1.55*side,y,80.2,1.55,.96)], sides=10)
    shell(hand, 3, [(31.8*side,-6.6,80.6,1.23,1.09),(31.1*side,-6.6,83,1.45,1.33),
                    (31.2*side,-4.3,86,1.6,1.37)], sides=10)
    shell(thigh, 3, [(12*side,-1,46,5.9,6.6),(11.5*side,0,61,7.5,7.7),(11*side,0,75,8.9,8.4),
                     (10*side,0,90,9.1,8.6)])
    shell(thigh, 0, [(12*side,-1.4,53,6.2,7),(11.7*side,-1,57,7.6,8.6),(11*side,-.8,72,9.1,9.1),
                     (10.5*side,0,80,8.5,8.2)])
    shell(shin, 0, [(13*side,0,12,4.9,5.8),(13*side,0,19,5.9,6.6),(12.7*side,0,29,7.2,8.0),
                    (12*side,-1.7,41,7.7,9.1),(12*side,-2,48,6.6,7.8)])
    x = 12*side
    plate(shin, 0, [(x-5,-9.5,45),(x-3.7,-11.4,39),(x,-13.2,36),(x+3.7,-11.4,39),
                     (x+5,-9.5,45),(x,-12.1,49)], 1.6)
    shell(foot, 3, [(13*side,-5.9,2,5.8,11.4),(13*side,-6.0,4,6.2,12),
                    (13*side,-4.7,7.5,6.05,10.7),(13*side,-2.7,11,5.6,8.4),(13*side,0,16.6,4.9,6.6)])
    shell(foot, 0, [(13*side,-6.5,4,6.3,11.9),(13*side,-6.0,6.3,6.1,12),
                    (13*side,-7.0,8.3,5.4,9.7),(13*side,-8.2,9.6,4.3,6.8)])

mesh = bpy.data.meshes.new("BattlemageArmor")
mesh.from_pydata(vertices, [], faces)
mesh.update()
body = bpy.data.objects.new("Battlemage", mesh)
bpy.context.collection.objects.link(body)
for mat in materials:
    mesh.materials.append(mat)
for polygon, color, smooth in zip(mesh.polygons, colors, smoothing, strict=True):
    polygon.material_index = color
    polygon.use_smooth = smooth
for name in rig_data.bones.keys():
    group = body.vertex_groups.new(name=name)
    ids = [i for i, owner in enumerate(weights) if owner == name]
    if ids:
        group.add(ids, 1., "REPLACE")
body.parent = rig
deform = body.modifiers.new("MeridianRig", "ARMATURE")
deform.object = rig

bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
body.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(
    filepath=sys.argv[sys.argv.index("--")+1], use_selection=True,
    object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    axis_forward="-Y", axis_up="Z", apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE",
    armature_nodetype="NULL",
)
