"""Balance V1 (docs/48) read-only inventory dump.

Writes Saved/BalanceV1Delegation/inventory.json with the authored rows that matter
for the balance pass: DT_Units, DT_Encounters and the DA_GameData fields we touch.
Never mutates assets. Run:
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script=Scripts/AuditBalanceV1.py -unattended -nop4 -nosplash -NullRHI
"""
import json
import os

import unreal

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "BalanceV1Delegation")
OUT_PATH = os.path.join(OUT_DIR, "inventory.json")

UNIT_IDS = [
    "Hero_Knight", "Hero_Mage", "Hero_Ranger",
    "Hero_Necromancer", "Hero_SkeletonGiant", "Boss_SkeletonKing",
    "Unit_Skeleton", "Unit_SkeletonArcher",
]

UNIT_FIELDS = [
    "UnitId", "BaseHealth", "AttackDamage", "AttackRange", "AttackInterval", "MoveSpeed",
    "UnitClass", "AttackType", "Quality", "Race", "bSkeleton", "PassiveAbility",
    "HeroTraits", "SkillCooldown", "SpawnUnitId", "SpawnInterval",
]


def jsonable(value):
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    if isinstance(value, (list, tuple)):
        return [jsonable(v) for v in value]
    if isinstance(value, dict):
        return {str(k): jsonable(v) for k, v in value.items()}
    if hasattr(value, "get_name"):
        try:
            return value.get_name()
        except Exception:
            pass
    return str(value)


def row_to_dict(row):
    data = {}
    for field in UNIT_FIELDS:
        try:
            data[field] = jsonable(row.get_editor_property(field))
        except Exception as exc:  # field may not exist on this struct
            data[field] = f"<unavailable: {exc}>"
    return data


def dump_table(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return {"exists": False}
    table = unreal.EditorAssetLibrary.load_asset(path)
    if not table:
        return {"exists": True, "loaded": False}
    result = {"exists": True, "class": table.get_class().get_name(), "row_struct": None, "row_names": [], "rows": {}}
    try:
        result["row_struct"] = table.get_editor_property("row_struct").get_name()
    except Exception as exc:
        result["row_struct"] = f"<unavailable: {exc}>"
    try:
        names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
        result["row_names"] = [str(n) for n in names]
    except Exception as exc:
        result["row_names_error"] = str(exc)
    # Prefer the engine JSON export; fall back to per-row reflection.
    try:
        exported = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
        if isinstance(exported, (list, tuple)) and len(exported) == 2:
            result["json_export"] = json.loads(exported[1]) if exported[0] else None
        else:
            result["json_export"] = json.loads(exported)
    except Exception as exc:
        result["json_export_error"] = str(exc)
        for name in result.get("row_names", []):
            try:
                row = table.get_editor_property("row_map")  # not exposed; guarded below
            except Exception:
                row = None
            result["rows"][str(name)] = jsonable(row)
    return result


def dump_units():
    out = {"table": dump_table("/Game/Data/DT_Units"), "selected_rows": {}}
    json_export = out["table"].get("json_export") or []
    for entry in json_export:
        name = entry.get("Name")
        if name in UNIT_IDS:
            out["selected_rows"][name] = entry
    return out


def dump_game_data():
    if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Data/DA_GameData"):
        return {"exists": False}
    data = unreal.EditorAssetLibrary.load_asset("/Game/Data/DA_GameData")
    if not data:
        return {"exists": True, "loaded": False}
    fields = [
        "BattleSeed", "BattleTimeLimit", "OvertimeWeaknessBasePct", "OvertimeWeaknessTick",
        "OvertimeWeaknessGrowth", "SilverPerSecond", "SilverCap", "HandSize", "MaxUnitsPerTeam",
        "MaxHeroesPerTeam", "BuildingTypeLimit", "FieldHalfWidth", "FieldHalfHeight",
        "UnitBodyRadius", "HeroCampBodyRadius", "UnitAcquireRadius", "EnemyEncounterId",
        "DefaultPlayerDeck", "DefaultEnemyDeck", "HeroPostBattleRecovery", "HomeMapName",
        "BattleMapName", "bDrawDebugShapes", "bDrawFieldBounds", "CameraShakeScale",
    ]
    out = {"exists": True, "fields": {}}
    for field in fields:
        try:
            out["fields"][field] = jsonable(data.get_editor_property(field))
        except Exception as exc:
            out["fields"][field] = f"<unavailable: {exc}>"
    try:
        out["card_library"] = [c.get_name() for c in data.get_editor_property("card_library") if c]
    except Exception as exc:
        out["card_library"] = f"<unavailable: {exc}>"
    try:
        unit_table = data.get_editor_property("unit_table")
        out["unit_table"] = str(unit_table)
    except Exception as exc:
        out["unit_table"] = f"<unavailable: {exc}>"
    try:
        out["encounter_table"] = str(data.get_editor_property("encounter_table"))
    except Exception as exc:
        out["encounter_table"] = f"<unavailable: {exc}>"
    return out


def dump_card(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return {"exists": False}
    card = unreal.EditorAssetLibrary.load_asset(path)
    out = {"exists": True, "asset": path}
    for field in ["CardId", "CardName", "Cost", "CardType", "SpellEffect", "SpellValue",
                  "SpellRadius", "SpawnUnitId", "BuildingUnitId", "SpellGrade",
                  "bExpeditionOnly", "BuildingTypeLimitOverride"]:
        try:
            out[field] = jsonable(card.get_editor_property(field))
        except Exception as exc:
            out[field] = f"<unavailable: {exc}>"
    return out


payload = {
    "engine": unreal.SystemLibrary.get_engine_version(),
    "project": unreal.Paths.project_dir(),
    "units": dump_units(),
    "encounters": dump_table("/Game/Data/DT_Encounters"),
    "traits": dump_table("/Game/Data/DT_Traits"),
    "waves": dump_table("/Game/Data/DT_Waves"),
    "game_data": dump_game_data(),
    "cards": {name: dump_card(f"/Game/Data/{name}") for name in [
        "C_Swordsman", "C_Archer", "C_Shieldbearer", "C_Fireball", "C_HealWave",
        "C_ArrowTower", "C_Barracks",
    ]},
}

os.makedirs(OUT_DIR, exist_ok=True)
with open(OUT_PATH, "w", encoding="utf-8") as handle:
    json.dump(payload, handle, ensure_ascii=False, indent=2)
unreal.log(f"BalanceV1 inventory written: {OUT_PATH}")
