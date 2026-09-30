"""UE 5.8 editor Python: targeted Balance V1 data migration (idempotent).

Run only after compiling C++. Preserves all existing art/animation fields.
Exports a one-time backup to Saved/BalanceV1AssetBackup before each asset edit.
"""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
BACKUP = ROOT / "Saved/BalanceV1AssetBackup"
BACKUP.mkdir(parents=True, exist_ok=True)


def export(table):
    value = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
    if isinstance(value, tuple):
        assert value[0], "DataTable export failed"
        value = value[1]
    return json.loads(value)


def backup(name, data):
    path = BACKUP / name
    if not path.exists():
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def save_table(table, rows):
    payload = json.dumps(rows, ensure_ascii=False, indent=2)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, payload)
    assert unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)


units = unreal.load_asset("/Game/Data/DT_Units")
rows = export(units)
backup("DT_Units.before.json", rows)
VALUES = {
    "Hero_Knight": (6000, 25, 1), "Hero_Mage": (4200, 20, 1.2),
    "Hero_Ranger": (4800, 22, 1), "Hero_Necromancer": (18000, 26, 1.8),
    "Hero_SkeletonGiant": (10000, 34, 1.9), "Boss_SkeletonKing": (8000, 44, 1.7),
    "Unit_Skeleton": (240, 10, 1.3), "Unit_SkeletonArcher": (160, 12, 1.6),
}
seen = set()
for row in rows:
    # Repair the old mistyped row key; UnitId and presentation are already correct.
    if row["Name"] == "NewHero_NecromancerRow":
        assert not any(r["Name"] == "Hero_Necromancer" for r in rows)
        row["Name"] = "Hero_Necromancer"
    key = row["Name"]
    if key not in VALUES:
        continue
    hp, damage, interval = VALUES[key]
    row["BaseHealth"] = hp
    # Player damage and cadence are deliberately untouched.
    if key not in ("Hero_Knight", "Hero_Mage", "Hero_Ranger"):
        row["AttackDamage"] = damage
        row["AttackInterval"] = interval
    seen.add(key)
assert seen == set(VALUES), f"Missing unit rows: {set(VALUES)-seen}"
save_table(units, rows)

encounters = unreal.load_asset("/Game/Data/DT_Encounters")
rows = export(encounters)
backup("DT_Encounters.before.json", rows)
for row in rows:
    if row["Name"] not in ("Encounter_UndeadPatrol", "Encounter_UndeadElite", "Encounter_SkeletonKing"):
        continue
    rank = row["Rank"]
    interval, last, silver, cooldown = {"Normal": (8,176,.35,20), "Elite": (7,175,.40,17), "Boss": (6,180,.45,14)}[rank]
    waves = [{"Time": 0, "UnitId": "Unit_Skeleton", "Count": 4}, {"Time": 0, "UnitId": "Unit_SkeletonArcher", "Count": 2}]
    for time in range(interval, last+1, interval):
        waves.extend([{"Time": time, "UnitId": "Unit_Skeleton", "Count": 2}, {"Time": time, "UnitId": "Unit_SkeletonArcher", "Count": 1}])
    row.update(Waves=waves, EnemySilverPerSecond=silver, EnemySilverCap=10, EnemyStartingSilver=0,
               BalanceVersion=1, Depth=0, EnemyHealthScale=1, EnemyDamageScale=1)
    row["EnemyHeroHealthScale"] = [.5 if rank == "Elite" else .3 if rank == "Boss" and not hero.startswith("Boss_") else 1 for hero in row["EnemyHeroIds"]]
    row["EnemySpell"] = dict(bEnabled=True, SpellId="Spell_SkeletonCircle", FirstCastTime=12, CheckInterval=.5,
                             Cooldown=cooldown, MaxActive=1, PreferredTargets=2, StopTime=180)
save_table(encounters, rows)
(ROOT / "Content/DataSources/DT_Encounters.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")

data = unreal.load_asset("/Game/Data/DA_GameData")
backup("DA_GameData.before.json", {key: data.get_editor_property(key) for key in ("battle_time_limit", "overtime_weakness_growth")})
data.set_editor_property("battle_time_limit", 300.0)
# Preserve the project's authored overtime strength; this pass changes its start time.
assert unreal.EditorAssetLibrary.save_loaded_asset(data, only_if_is_dirty=False)
unreal.log("[BalanceV1] Targeted data migration completed; presentation and unrelated settings preserved.")
