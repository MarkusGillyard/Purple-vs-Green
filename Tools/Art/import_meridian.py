"""Import the Meridian art and build its reproducible material graphs."""

import os
from pathlib import Path

import unreal

source = Path(os.environ["PVG_MERIDIAN_ART"])
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary
directory = "/Game/Materials"


def import_asset(filename, path, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(source / filename)
    task.destination_path = path
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
    tools.import_asset_tasks([task])
    objects = task.get_objects()
    if not objects:
        raise RuntimeError(f"Import failed for {filename}")
    return objects[0]


data = import_asset("T_MeridianMasonry.png", directory)
data.set_editor_property("srgb", False)
data.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
unreal.EditorAssetLibrary.save_loaded_asset(data)

for name in ("SM_MeridianRidge", "SM_MeridianCrystal", "SM_MeridianReeds", "SM_MeridianAshlar"):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    options.static_mesh_import_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    import_asset(f"{name}.fbx", "/Game/Meridian", options)


def material(name):
    path = f"{directory}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        edit.delete_all_material_expressions(asset)
        return asset
    return tools.create_asset(name, directory, unreal.Material, unreal.MaterialFactoryNew())


def scalar(mat, name, default, position):
    node = edit.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, *position)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def custom(mat, code, inputs, position, output):
    node = edit.create_material_expression(mat, unreal.MaterialExpressionCustom, *position)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    for name, (upstream, pin) in inputs.items():
        if not edit.connect_material_expressions(upstream, pin, node, name):
            raise RuntimeError(f"Could not wire material input {name}")
    return node


def save(mat):
    edit.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat):
        raise RuntimeError(f"Could not save {mat.get_name()}")


stone = material("M_MeridianMasonry")
stone.set_editor_property("used_with_instanced_static_meshes", True)
stone.set_editor_property("tangent_space_normal", False)
position = edit.create_material_expression(stone, unreal.MaterialExpressionWorldPosition, -1000, -280)
tex = edit.create_material_expression(stone, unreal.MaterialExpressionTextureObject, -1000, 80)
tex.set_editor_property("texture", data)
tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
up = edit.create_material_expression(stone, unreal.MaterialExpressionConstant3Vector, -1180, 320)
up.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
normal = edit.create_material_expression(stone, unreal.MaterialExpressionTransform, -1000, 320)
normal.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT)
normal.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
edit.connect_material_expressions(up, "", normal, "")
scale = scalar(stone, "SurfaceScale", 480., (-1000, 480))
vector = unreal.CustomMaterialOutputType.CMOT_FLOAT3
number = unreal.CustomMaterialOutputType.CMOT_FLOAT1
texture = custom(stone, """
float3 w = pow(abs(normalize(SurfaceNormal)), 6);
w /= max(0.001, w.x + w.y + w.z);
float3 p = Position / max(12.0, Period);
return Texture2DSample(Surface, SurfaceSampler, p.yz).rgb * w.x
     + Texture2DSample(Surface, SurfaceSampler, p.xz).rgb * w.y
     + Texture2DSample(Surface, SurfaceSampler, p.xy).rgb * w.z;
""", {"Position": (position, ""), "SurfaceNormal": (normal, ""), "Surface": (tex, ""), "Period": (scale, "")},
    (-690, 0), vector)
tint = edit.create_material_expression(stone, unreal.MaterialExpressionVectorParameter, -660, -320)
tint.set_editor_property("parameter_name", "Color")
tint.set_editor_property("default_value", unreal.LinearColor(.25, .23, .21, 1.))
albedo = custom(stone, "return Tint * (0.34 + Minerals.r * 1.14);",
    {"Tint": (tint, "RGB"), "Minerals": (texture, "")}, (-350, -270), vector)
roughness = scalar(stone, "Roughness", .84, (-660, 280))
rough = custom(stone, "return saturate(Base * (0.54 + Minerals.g * 0.54));",
    {"Base": (roughness, ""), "Minerals": (texture, "")}, (-350, 30), number)
bump = scalar(stone, "Relief", .82, (-660, 460))
surface_normal = custom(stone, """
float3 N = normalize(SurfaceNormal);
float3 dPdx = ddx(Position);
float3 dPdy = ddy(Position);
float3 Rx = cross(dPdy, N);
float3 Ry = cross(N, dPdx);
float det = dot(dPdx, Rx);
float3 gradient = sign(det) * (ddx(Minerals.b) * Rx + ddy(Minerals.b) * Ry);
return normalize(max(abs(det), 0.00001) * N - gradient * Relief);
""", {"SurfaceNormal": (normal, ""), "Position": (position, ""), "Minerals": (texture, ""), "Relief": (bump, "")},
    (-350, 280), vector)
edit.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
edit.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
edit.connect_material_property(surface_normal, "", unreal.MaterialProperty.MP_NORMAL)
edit.connect_material_property(scalar(stone, "Metallic", 0., (-350, 510)), "", unreal.MaterialProperty.MP_METALLIC)
save(stone)

ridge = material("M_MeridianRidge")
ridge.set_editor_property("used_with_instanced_static_meshes", True)
color = edit.create_material_expression(ridge, unreal.MaterialExpressionVertexColor, -350, -100)
edit.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
edit.connect_material_property(scalar(ridge, "Roughness", .94, (-350, 100)), "", unreal.MaterialProperty.MP_ROUGHNESS)
save(ridge)
aether_path = f"{directory}/M_Aether"
if unreal.EditorAssetLibrary.does_asset_exist(aether_path):
    aether = unreal.EditorAssetLibrary.load_asset(aether_path)
    aether.set_editor_property("used_with_instanced_static_meshes", True)
    save(aether)
unreal.log("PvG Meridian art import complete")
