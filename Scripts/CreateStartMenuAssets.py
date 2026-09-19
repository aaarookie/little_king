"""UE 5.8: generate the start map, preserving unrelated maps and assets."""
import unreal

path = "/Game/Maps/L_StartMenu"
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
mode = unreal.load_class(None, "/Script/little_king.LKStartMenuGameMode")
if not mode:
    raise RuntimeError("Build little_kingEditor before creating the start menu")
if unreal.EditorAssetLibrary.does_asset_exist(path):
    editor.load_level(path)
elif not editor.new_level(path):
    raise RuntimeError("Could not create L_StartMenu")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", mode)
if not editor.save_current_level():
    raise RuntimeError("Could not save L_StartMenu")
unreal.log("Start menu map ready: " + path)
