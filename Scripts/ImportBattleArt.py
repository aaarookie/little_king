"""UE 5.8: import B-batch source images; only owns /Game/Art/StorybookV1/Battle.

Run after every catalogued sprite and card has been generated. Gameplay values stay unchanged.
"""
from pathlib import Path
import json
import struct
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'ArtSource/StorybookV1/Battle'
BASE = '/Game/Art/StorybookV1/Battle'
CATALOG = json.loads((SOURCE / 'catalog.json').read_text(encoding='utf-8-sig'))['items']
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.EditorAssetLibrary
REPORT = []

# Validate the full batch before making any asset changes.
for item in CATALOG:
    assert (SOURCE / 'Images' / ('T_' + item['id'] + '.png')).is_file(), item['id']
    if item['card']:
        assert (SOURCE / 'Cards' / ('T_Card_' + item['id'] + '.png')).is_file(), item['id']

def texture(path, folder):
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = BASE + '/' + folder
    task.destination_name = path.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    TOOLS.import_asset_tasks([task])
    objects = task.get_objects()
    if not objects: raise RuntimeError('Failed to import ' + str(path))
    tex = objects[0]
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('max_texture_size', 512)
    LIB.save_loaded_asset(tex)
    REPORT.append(tex.get_path_name())
    return tex

for item in CATALOG:
    name = 'T_' + item['id']
    tex = texture(SOURCE / 'Images' / (name + '.png'), 'Textures')
    name = 'SP_' + item['id']
    path = BASE + '/Sprites/' + name
    sprite = LIB.load_asset(path) if LIB.does_asset_exist(path) else TOOLS.create_asset(
        name, BASE + '/Sprites', unreal.PaperSprite, unreal.PaperSpriteFactory())
    # Source dimensions, not built max-texture dimensions: UVs must address the full source.
    source_size, source_height = struct.unpack('>II', (SOURCE / 'Images' / ('T_' + item['id'] + '.png')).read_bytes()[16:24])
    sprite.set_editor_property('source_texture', tex)
    sprite.set_editor_property('source_dimension', unreal.Vector2D(source_size, source_height))
    sprite.set_editor_property('source_uv', unreal.Vector2D(0, 0))
    sprite.set_editor_property('pixels_per_unreal_unit', source_height / item['worldHeight'])
    sprite.set_editor_property('default_material', LIB.load_asset('/Paper2D/MaskedUnlitSpriteMaterial'))
    LIB.save_loaded_asset(sprite)
    REPORT.append(sprite.get_path_name())
    if item['card']:
        texture(SOURCE / 'Cards' / ('T_Card_' + item['id'] + '.png'), 'Cards')

# Correct only the shipped barracks' accidental non-uniform display scale. Custom art/scales stay authored.
table = LIB.load_asset('/Game/Data/DT_Units')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
changed = False
for row in rows:
    if (row.get('Name') == 'Building_Barracks'
            and row.get('Sprite') == '/Game/Sprites/Building_Barracks_sprite.Building_Barracks_sprite'
            and row.get('SpriteScale') == {'X': 0.5, 'Y': 1}):
        row['SpriteScale']['Y'] = 0.5
        changed = True
if changed:
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows))
    LIB.save_loaded_asset(table)
    unreal.log('BATTLE_ART_BARRACKS_SCALE_FIX Y=0.5 (display only)')
(ROOT / 'Saved/BattleArtUnitsAfter.json').write_text(
    unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table), encoding='utf-8')
(ROOT / 'Saved/BattleArtImport.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')
unreal.log('BATTLE_ART_IMPORT_OK assets=' + str(len(REPORT)))
