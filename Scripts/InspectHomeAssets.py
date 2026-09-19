import unreal
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level("/Game/Maps/L_Home"):
    raise RuntimeError("L_Home missing")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mode = world.get_world_settings().get_editor_property("default_game_mode")
unreal.log("HOME_CHECK GameMode=" + mode.get_path_name())
defaults = unreal.get_default_object(mode)
unreal.log("HOME_CHECK GameData=" + str(defaults.get_editor_property("game_data")))
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor, unreal.CameraActor):
        comp = actor.get_component_by_class(unreal.CameraComponent)
        unreal.log("HOME_CHECK Camera=" + str(actor.get_actor_location()) + " / " + str(actor.get_actor_rotation())
                   + " width=" + str(comp.get_editor_property("ortho_width"))
                   + " mode=" + str(comp.get_editor_property("projection_mode")))
unreal.log("HOME_CHECK complete; no assets saved")
