"""Import the shared rig as a skeletal mesh using Unreal's legacy FBX importer."""
import os

import unreal

source = os.environ["PVG_BATTLEMAGE_FBX"]
task = unreal.AssetImportTask()
task.filename = source
task.destination_path = "/Game/Battlemage"
task.destination_name = "SK_Battlemage"
task.automated = True
task.replace_existing = True
task.save = True
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.import_as_skeletal = True
options.import_mesh = True
options.import_animations = False
options.import_materials = False
options.import_textures = False
options.create_physics_asset = False
options.skeletal_mesh_import_data.set_editor_property("force_front_x_axis", True)
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assets = task.get_objects()
mesh = next((obj for obj in assets if isinstance(obj, unreal.SkeletalMesh)), None)
if mesh is None:
    raise RuntimeError("Skeletal mesh import failed")
skeleton = mesh.get_editor_property("skeleton")
if skeleton is None or not unreal.EditorAssetLibrary.save_loaded_asset(skeleton, only_if_is_dirty=False):
    raise RuntimeError("Could not save the shared skeleton")
unreal.log(f"PvG battlemage imported {mesh.get_path_name()} with bounds {mesh.get_bounds()}")
unreal.log(f"PvG material slots: {[str(m.material_slot_name) for m in mesh.materials]}")

tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary
path = "/Game/Materials"

def new_material(name):
    asset = f"{path}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset):
        return unreal.EditorAssetLibrary.load_asset(asset)
    return tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())

def tint(mat, color):
    node = edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -460, -100)
    node.set_editor_property("parameter_name", "Color")
    node.set_editor_property("default_value", unreal.LinearColor(*color, 1))
    return node

def scalar(mat, name, value, y):
    node = edit.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -460, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node

armor = new_material("M_Armor")
edit.delete_all_material_expressions(armor)
armor.set_editor_property("used_with_skeletal_mesh", True)
edit.connect_material_property(tint(armor, (.085, .11, .143)), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
edit.connect_material_property(scalar(armor, "Roughness", .56, 80), "", unreal.MaterialProperty.MP_ROUGHNESS)
edit.connect_material_property(scalar(armor, "Metallic", .19, 240), "", unreal.MaterialProperty.MP_METALLIC)
edit.recompile_material(armor)
unreal.EditorAssetLibrary.save_loaded_asset(armor)

aether = new_material("M_Aether")
edit.delete_all_material_expressions(aether)
aether.set_editor_property("used_with_instanced_static_meshes", True)
aether.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
emission = edit.create_material_expression(aether, unreal.MaterialExpressionMultiply, -120, -20)
edit.connect_material_expressions(tint(aether, (.4, .77, 1)), "RGB", emission, "A")
edit.connect_material_expressions(scalar(aether, "Intensity", 2.1, 100), "", emission, "B")
edit.connect_material_property(emission, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(aether)
unreal.EditorAssetLibrary.save_loaded_asset(aether)
