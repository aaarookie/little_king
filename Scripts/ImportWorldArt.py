"""UE 5.8 C batch. Reimport only /Game/Art/StorybookV1/World; no map/table/save edits."""
from pathlib import Path
import json
import struct
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SRC = ROOT / 'ArtSource/StorybookV1/World'
BASE = '/Game/Art/StorybookV1/World'
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ITEMS = json.loads((SRC / 'catalog.json').read_text(encoding='utf-8-sig'))
AUDIO = json.loads((SRC / 'Audio/manifest.json').read_text(encoding='utf-8-sig'))
REPORT = []
for row in ITEMS:
    assert (SRC / 'Images' / ('T_' + row['id'] + '.png')).is_file(), row['id']
for row in AUDIO:
    assert (ROOT / row['file']).is_file(), row['key']

def import_asset(path, folder):
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = BASE + '/' + folder
    task.destination_name = path.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    TOOLS.import_asset_tasks([task])
    objects = task.get_objects()
    if not objects: raise RuntimeError('Import failed: ' + str(path))
    REPORT.append(objects[0].get_path_name())
    return objects[0]

for row in ITEMS:
    path = SRC / 'Images' / ('T_' + row['id'] + '.png')
    tex = import_asset(path, 'Textures')
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('max_texture_size', 1024 if row['kind'] == 'ground' else 512)
    LIB.save_loaded_asset(tex)
    if row['kind'] in ('node', 'fx'): continue
    name = 'SP_' + row['id']
    asset = BASE + '/Sprites/' + name
    sprite = LIB.load_asset(asset) if LIB.does_asset_exist(asset) else TOOLS.create_asset(name, BASE + '/Sprites', unreal.PaperSprite, unreal.PaperSpriteFactory())
    width, height = struct.unpack('>II', path.read_bytes()[16:24])
    sprite.set_editor_property('source_texture', tex)
    sprite.set_editor_property('source_dimension', unreal.Vector2D(width, height))
    sprite.set_editor_property('source_uv', unreal.Vector2D(0, 0))
    sprite.set_editor_property('pixels_per_unreal_unit', height / row.get('worldHeight', 100))
    sprite.set_editor_property('default_material', LIB.load_asset('/Paper2D/MaskedUnlitSpriteMaterial'))
    LIB.save_loaded_asset(sprite)
    REPORT.append(sprite.get_path_name())

# C cues share the existing combat/UI caps, including A sounds, instead of each having its own limit.
combat = LIB.load_asset('/Game/Art/StorybookV1/Audio/SC_Combat')
ui = LIB.load_asset('/Game/Art/StorybookV1/Audio/SC_UI')
assert combat and ui
for row in AUDIO:
    sound = import_asset(ROOT / row['file'], 'Audio')
    sound.set_editor_property('looping', False)
    sound.set_editor_property('volume', .8)
    sound.set_editor_property('concurrency_set', [ui if row['key'] in ('NodeEnter', 'Rest', 'Market', 'Upgrade') else combat])
    LIB.save_loaded_asset(sound)
(ROOT / 'Saved/WorldArtImport.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')
unreal.log('WORLD_ART_IMPORT_OK assets=' + str(len(REPORT)))
