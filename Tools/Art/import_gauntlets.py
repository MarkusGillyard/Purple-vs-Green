"""Import the casting meshes and their parameterized surface material."""
import os
from pathlib import Path

import unreal

source = Path(os.environ["PVG_GAUNTLET_ART"])
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
directory = "/Game/Materials"
name = "M_CastingGear"
path = f"{directory}/{name}"
material = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
    name, directory, unreal.Material, unreal.MaterialFactoryNew())
edit.delete_all_material_expressions(material)


def expression(kind, x, y, **properties):
    node = edit.create_material_expression(material, kind, x, y)
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node


def scalar(name, value, y):
    return expression(unreal.MaterialExpressionScalarParameter, -750, y,
                      parameter_name=name, default_value=value)


def custom(code, inputs, output, x, y):
    node = expression(unreal.MaterialExpressionCustom, x, y, code=code, output_type=output)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    for name, (upstream, pin) in inputs.items():
        if not edit.connect_material_expressions(upstream, pin, node, name):
            raise RuntimeError(f"Could not connect {name}")
    return node


tint = expression(unreal.MaterialExpressionVectorParameter, -750, -260, parameter_name="Color",
                  default_value=unreal.LinearColor(.135, .085, .049, 1.))
vertex = expression(unreal.MaterialExpressionVertexColor, -750, -110)
uv = expression(unreal.MaterialExpressionTextureCoordinate, -1040, 0)
roughness = scalar("Roughness", .72, 170)
metallic = scalar("Metallic", 0., 320)
number = unreal.CustomMaterialOutputType.CMOT_FLOAT1
vector = unreal.CustomMaterialOutputType.CMOT_FLOAT3
grain = custom("""
float2 p = UV * float2(74, 46);
float2 cell = floor(p);
float2 blend = smoothstep(0., 1., frac(p));
float4 dots = float4(dot(cell, float2(127.1, 311.7)),
                    dot(cell + float2(1, 0), float2(127.1, 311.7)),
                    dot(cell + float2(0, 1), float2(127.1, 311.7)),
                    dot(cell + 1, float2(127.1, 311.7)));
float4 hash = frac(sin(dots) * 43758.5453);
float noise = lerp(lerp(hash.x, hash.y, blend.x), lerp(hash.z, hash.w, blend.x), blend.y);
return lerp(noise, .5, saturate(length(fwidth(p)) - .35));
""", {"UV": (uv, "")}, number, -750, 20)
color = custom("return Tint * (0.66 + .42 * Wear) * (.82 + .36 * Grain);",
               {"Tint": (tint, "RGB"), "Wear": (vertex, "R"), "Grain": (grain, "")},
               vector, -370, -180)
finish = custom("return saturate(Finish + (Grain - .5) * .20 + (.90 - Wear) * .25);",
                {"Finish": (roughness, ""), "Grain": (grain, ""), "Wear": (vertex, "R")},
                number, -370, 120)
edit.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
edit.connect_material_property(finish, "", unreal.MaterialProperty.MP_ROUGHNESS)
edit.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
edit.recompile_material(material)
if not assets.save_loaded_asset(material):
    raise RuntimeError("Could not save the casting gear material")

for name in ("SM_CastingArm_R", "SM_CastingArm_L", "SM_CastingFinger", "SM_CastingFingertip"):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials = False
    options.import_textures = False
    data = options.static_mesh_import_data
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("force_front_x_axis", True)
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)

    task = unreal.AssetImportTask()
    task.filename = str(source / f"{name}.fbx")
    task.destination_path = "/Game/Battlemage"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    tools.import_asset_tasks([task])
    mesh = next((obj for obj in task.get_objects() if isinstance(obj, unreal.StaticMesh)), None)
    if mesh is None:
        raise RuntimeError(f"Failed to import {name}")
    slots = [str(slot.material_slot_name) for slot in mesh.get_editor_property("static_materials")]
    expected = {"Leather", "Cloth", "Steel", "Bronze", "Livery", "Thread"} if "Arm" in name else {"Leather"}
    if not expected.issubset(slots):
        raise RuntimeError(f"{name} has unexpected material slots {slots}")
    bounds = mesh.get_bounding_box()
    unreal.log(f"PvG {name} bounds {bounds}, slots {slots}")
    if bounds.max.x < 3. or ("Arm" in name and bounds.min.x > -40.):
        raise RuntimeError(f"{name} did not import forward along +X")
    if "Arm" in name:
        side = 1. if name.endswith("_R") else -1.
        if side * (bounds.max.y + bounds.min.y) < 1.:
            raise RuntimeError(f"{name} imported with mirrored lateral geometry: {bounds}")
