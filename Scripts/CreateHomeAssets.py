"""UE 5.8: create the home map and its owned placeholder assets, without resaving battle assets."""
import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIBRARY = unreal.EditorAssetLibrary
MAP = "/Game/Maps/L_Home"
MATERIAL = "/Game/Materials/Home/M_HomePlaceholder"
MODE = "/Game/blueprint/Home/BP_HomeGameMode"


def configure_camera(camera):
    component = camera.get_component_by_class(unreal.CameraComponent)
    component.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    component.set_editor_property("ortho_width", 2400.0)
    component.set_editor_property("constrain_aspect_ratio", False)
    settings = component.get_editor_property("post_process_settings")
    settings.set_editor_property("override_auto_exposure_method", True)
    settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
    settings.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", 0.0)
    component.set_editor_property("post_process_settings", settings)
    component.set_editor_property("post_process_blend_weight", 1.0)
    camera.set_editor_property("tags", ["HomePlaceholderLayoutV2"])


def material():
    existing = LIBRARY.load_asset(MATERIAL) if LIBRARY.does_asset_exist(MATERIAL) else None
    if existing:
        return existing
    asset = ASSET_TOOLS.create_asset("M_HomePlaceholder", "/Game/Materials/Home",
                                   unreal.Material, unreal.MaterialFactoryNew())
    asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    parameter = unreal.MaterialEditingLibrary.create_material_expression(
        asset, unreal.MaterialExpressionVectorParameter, -300, 0)
    parameter.set_editor_property("parameter_name", "Color")
    parameter.set_editor_property("default_value", unreal.LinearColor(0.04, 0.065, 0.09, 1))
    unreal.MaterialEditingLibrary.connect_material_property(parameter, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(asset)
    LIBRARY.save_loaded_asset(asset)
    return asset


def home_mode():
    parent = unreal.load_class(None, "/Script/little_king.LKHomeGameMode")
    if not parent:
        raise RuntimeError("Build little_kingEditor before creating home assets")
    asset = LIBRARY.load_asset(MODE) if LIBRARY.does_asset_exist(MODE) else None
    if not asset:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        asset = ASSET_TOOLS.create_asset("BP_HomeGameMode", "/Game/blueprint/Home", unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    cls = unreal.load_class(None, MODE + ".BP_HomeGameMode_C")
    defaults = unreal.get_default_object(cls)
    defaults.set_editor_property("game_data", LIBRARY.load_asset("/Game/Data/DA_GameData"))
    defaults.set_editor_property("spawn_placeholder_ground", False)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    LIBRARY.save_loaded_asset(asset)
    return cls


mat = material()
mode = home_mode()
if LIBRARY.does_asset_exist(MAP):
    unreal.log("Home map already exists; layout preserved. Only owned material/GameMode checked.")
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    editor.load_level(MAP)
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor, unreal.CameraActor) and actor.get_actor_label() == "HomeCamera":
            rotation = actor.get_actor_rotation()
            # Migrate the first generator revision which used positional Rotator arguments.
            if abs(rotation.pitch) < 0.01 and abs(rotation.roll + 90) < 0.01:
                actor.set_actor_rotation(unreal.Rotator(pitch=-90, yaw=0, roll=0), False)
            if "HomePlaceholderLayoutV2" not in [str(tag) for tag in actor.tags]:
                configure_camera(actor)
        if actor.get_actor_label() == "HomeGround" and abs(actor.get_actor_scale3d().x - 36) < 0.01:
            actor.set_actor_scale3d(unreal.Vector(60, 60, 1))
    editor.save_current_level()
else:
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor.new_level(MAP):
        raise RuntimeError("Cannot create L_Home")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", mode)
    camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 3000), unreal.Rotator(pitch=-90, yaw=0, roll=0))
    camera.set_actor_label("HomeCamera")
    configure_camera(camera)
    plane = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -10))
    plane.set_actor_label("HomeGround")
    mesh = plane.get_component_by_class(unreal.StaticMeshComponent)
    mesh.set_static_mesh(LIBRARY.load_asset("/Engine/BasicShapes/Plane"))
    mesh.set_material(0, mat)
    plane.set_actor_scale3d(unreal.Vector(60, 60, 1))
    # Separate owned material instances keep the simple paths visible without lighting assets.
    def make_patch(name, x, y, sx, sy, color):
        asset_path = "/Game/Materials/Home/MI_" + name
        instance = LIBRARY.load_asset(asset_path) if LIBRARY.does_asset_exist(asset_path) else None
        if not instance:
            instance = ASSET_TOOLS.create_asset("MI_" + name, "/Game/Materials/Home",
                unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            unreal.MaterialEditingLibrary.set_material_instance_parent(instance, mat)
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, "Color", unreal.LinearColor(*color, 1))
            LIBRARY.save_loaded_asset(instance)
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, -5))
        actor.set_actor_label(name)
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        comp.set_static_mesh(LIBRARY.load_asset("/Engine/BasicShapes/Plane"))
        comp.set_material(0, instance)
        actor.set_actor_scale3d(unreal.Vector(sx, sy, 1))
    make_patch("HomePlaza", 0, 0, 4, 4, (0.09, 0.12, 0.15))
    make_patch("HomeMainPath", 0, 0, 17, 0.6, (0.07, 0.10, 0.13))
    make_patch("HomeUpperPath", 300, 0, 0.6, 15, (0.07, 0.10, 0.13))
    make_patch("HomeLowerPath", -300, 0, 0.6, 15, (0.07, 0.10, 0.13))
    make_patch("HomeGatePath", -700, 0, 0.6, 8, (0.09, 0.12, 0.15))
    if not editor.save_current_level():
        raise RuntimeError("Failed to save home map")
    unreal.log("Created L_Home with BP_HomeGameMode, authored GameData, orthographic camera, unlit ground and paths")

unreal.log("Home asset setup complete")
