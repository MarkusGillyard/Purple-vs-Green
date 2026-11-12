"""Build the Meridian battlemage's original faceted armor and shared skeleton."""
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

vertices, faces, colors, weights = [], [], [], []

def shell(owner, color, rings, sides=8, rotation=math.pi/8):
    """Loft elliptical cross sections in local centimeters, front toward negative Y."""
    first = len(vertices)
    for x, y, z, rx, ry in rings:
        for k in range(sides):
            a = rotation + math.tau*k/sides
            vertices.append((x + math.cos(a)*rx, y + math.sin(a)*ry, z))
            weights.append(owner)
    def face(indices):
        faces.append(tuple(indices))
        colors.append(color)
    face(reversed(range(first, first+sides)))
    for j in range(len(rings)-1):
        bottom = first + j*sides
        top = bottom+sides
        for k in range(sides):
            nxt = (k+1) % sides
            face([bottom+k, bottom+nxt, top+nxt, top+k])
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
    for i in range(count):
        j = (i+1) % count
        faces.append((first+i, first+j, first+count+j, first+count+i))
        colors.append(color)

shell("Pelvis", 3, [(0,0,79,16,10),(0,0,87,20,12),(0,0,103,16,10)])
shell("Pelvis", 1, [(0,0,94,18,11),(0,0,97,18,11)])
shell("Chest", 0, [(0,1,96,15,10),(0,1,109,16,12),(0,-1,125,23,14),
                    (0,0,137,22,12),(0,1,142,15,9)])
shell("Chest", 2, [(0,0,137,15,9),(0,0,142,15,9),(0,0,146,9,8)])
shell("Head", 3, [(0,0,141,7,7),(0,0,154,8,8)])
shell("Head", 0, [(0,1,149,10,10),(0,0,154,15,13),(0,0,166,16,15),
                   (0,1,175,10,11),(0,1,177,5,7)])
plate("Head", 3, [(-11,-13.4,162),(-9,-11,155),(9,-11,155),(11,-13.4,162),(0,-16.6,163)], 2)
plate("Head", 2, [(-11,-14.4,163),(-9,-14.1,160),(0,-17.4,160),(9,-14.1,160),
                   (11,-14.4,163),(0,-17.1,164)], 1.6)
plate("Head", 1, [(-3,-14.3,169),(0,-16.7,165),(3,-14.3,169),(0,-13.2,175)], 1.2)
plate("Chest", 2, [(-12,-13.4,136),(-11,-16,122),(0,-17.8,113),(11,-16,122),
                    (12,-13.4,136),(0,-16,132)], 2.8)
plate("Chest", 1, [(-5,-16.3,127),(0,-18.6,119),(5,-16.3,127),(0,-17.8,133)], 1.3)
plate("Pelvis", 2, [(-13,-10.8,97),(-10,-12,65),(0,-12,59),(10,-12,65),(13,-10.8,97)], 2.2)

for tag, side in (("L", -1), ("R", 1)):
    upper = f"UpperArm_{tag}"
    fore = f"Forearm_{tag}"
    hand = f"Hand_{tag}"
    thigh = f"Thigh_{tag}"
    shin = f"Shin_{tag}"
    foot = f"Foot_{tag}"
    shell(upper, 3, [(27*side,0,106,6,6),(26*side,0,117,8,7),(23*side,0,135,9,9)])
    shell(upper, 0, [(29*side,0,122,7,8),(26*side,0,127,12,11),(24*side,0,139,12,12),
                     (22*side,0,145,7,8)])
    shell(upper, 2, [(28*side,0,128,12,12),(25*side,0,135,13,13),(23*side,0,138,10,11)])
    shell(upper, 1, [(28*side,0,126,10,10),(27*side,0,129,12,12)])
    shell(fore, 0, [(31*side,-2,85,5.4,5.8),(30*side,-1,92,6.9,7.6),(29*side,0,103,8.3,7.8),
                    (29*side,0,110,6,6)])
    shell(fore, 1, [(31*side,-2,86,5.9,6),(31*side,-2,89,6.5,6.6)])
    shell(hand, 3, [(32*side,-2,76,3.9,4.2),(31*side,-2,80,5.3,5.6),(31*side,-2,88,4.7,5)])
    shell(thigh, 3, [(12*side,-1,47,7,7),(11*side,0,71,9,9),(10*side,0,91,11,10)])
    shell(thigh, 0, [(12*side,-1,51,7,8),(12*side,0,57,10,10),(11*side,0,77,10,10),(10*side,0,82,8,8)])
    shell(shin, 0, [(13*side,0,12,6,6),(13*side,0,20,7.5,7.5),(12*side,-2,42,8.8,9.8),
                    (12*side,-2,50,7.3,8.7)])
    shell(shin, 1, [(12*side,-2,39,8.8,9.8),(12*side,-2,42,9.3,10.1)])
    shell(foot, 0, [(13*side,-4,2,8,12),(13*side,-5,5,8.4,12),(13*side,-4,10,7.5,11),
                    (13*side,0,17,6,7)])
    shell(foot, 1, [(13*side,-4,4,8.4,12.5),(13*side,-4,6,8.2,12.2)])

mesh = bpy.data.meshes.new("BattlemageArmor")
mesh.from_pydata(vertices, [], faces)
mesh.update()
body = bpy.data.objects.new("Battlemage", mesh)
bpy.context.collection.objects.link(body)
for mat in materials:
    mesh.materials.append(mat)
for polygon, color in zip(mesh.polygons, colors):
    polygon.material_index = color
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
