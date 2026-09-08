import os
import unreal


ASSET_PATH = "/Game/Data/DT_Encounters"
SOURCE_PATH = os.path.join(unreal.Paths.project_content_dir(), "DataSources", "DT_Encounters.json")


def load_or_create_table():
    existing = unreal.EditorAssetLibrary.load_asset(ASSET_PATH) if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH) else None
    if existing:
        if not isinstance(existing, unreal.DataTable):
            raise RuntimeError(f"{ASSET_PATH} exists but is not a DataTable")
        return existing

    row_struct = unreal.load_object(None, "/Script/little_king.LKEncounterRow")
    if not row_struct:
        raise RuntimeError("FLKEncounterRow is unavailable; build little_kingEditor first")
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", row_struct)
    table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DT_Encounters", "/Game/Data", unreal.DataTable, factory
    )
    if not table:
        raise RuntimeError("Failed to create /Game/Data/DT_Encounters")
    return table


table = load_or_create_table()
if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(table, SOURCE_PATH):
    raise RuntimeError(f"Failed to import {SOURCE_PATH}")
unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)
unreal.log(f"D2 encounter table ready: {ASSET_PATH}")
