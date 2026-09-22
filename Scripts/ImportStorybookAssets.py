"""UE 5.8 editor commandlet: import only owned StorybookV1 assets, no gameplay/table/map resaves."""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.EditorAssetLibrary
BASE = '/Game/Art/StorybookV1'
SOURCE = ROOT / 'ArtSource/StorybookV1'
REPORT = []

def import_file(path, folder):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(path))
    task.set_editor_property('destination_path', BASE + '/' + folder)
    task.set_editor_property('destination_name', path.stem)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    TOOLS.import_asset_tasks([task])
    result = task.get_objects()
    if not result:
        raise RuntimeError('Import failed: ' + str(path))
    return result[0]

for path in sorted((SOURCE / 'Images').glob('*.png')):
    texture = import_file(path, 'Textures')
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    LIB.save_loaded_asset(texture)
    REPORT.append(texture.get_path_name())
    if path.stem.startswith('T_Home_'):
        name = path.stem.replace('T_', 'SP_', 1)
        sprite_path = BASE + '/Sprites/' + name
        sprite = LIB.load_asset(sprite_path) if LIB.does_asset_exist(sprite_path) else None
        if not sprite:
            factory = unreal.PaperSpriteFactory()
            sprite = TOOLS.create_asset(name, BASE + '/Sprites', unreal.PaperSprite, factory)
        sprite.set_editor_property('source_texture', texture)
        sprite.set_editor_property('source_dimension', unreal.Vector2D(texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
        sprite.set_editor_property('source_uv', unreal.Vector2D(0, 0))
        sprite.set_editor_property('pixels_per_unreal_unit', texture.blueprint_get_size_x() / 500.0)
        sprite.set_editor_property('default_material', LIB.load_asset('/Paper2D/MaskedUnlitSpriteMaterial'))
        LIB.save_loaded_asset(sprite)
        REPORT.append(sprite.get_path_name())

def concurrency(name, count):
    path = BASE + '/Audio/' + name
    obj = LIB.load_asset(path) if LIB.does_asset_exist(path) else TOOLS.create_asset(
        name, BASE + '/Audio', unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    settings = obj.get_editor_property('concurrency')
    settings.set_editor_property('max_count', count)
    settings.set_editor_property('limit_to_owner', False)
    settings.set_editor_property('resolution_rule', unreal.MaxConcurrentResolutionRule.PREVENT_NEW)
    obj.set_editor_property('concurrency', settings)
    LIB.save_loaded_asset(obj)
    REPORT.append(obj.get_path_name())
    return obj

combat = concurrency('SC_Combat', 12)
ui = concurrency('SC_UI', 3)
signal = concurrency('SC_Signals', 2)
for path in sorted((SOURCE / 'Audio').glob('*.wav')):
    sound = import_file(path, 'Audio')
    sound.set_editor_property('looping', False)
    sound.set_editor_property('volume', .65 if path.stem in ('S_Victory', 'S_Defeat', 'S_BattleStart') else .8)
    group = ui if path.stem == 'S_UIClick' else signal if path.stem in ('S_Victory', 'S_Defeat', 'S_BattleStart') else combat
    sound.set_editor_property('concurrency_set', {group})
    LIB.save_loaded_asset(sound)
    REPORT.append(sound.get_path_name())

(ROOT / 'Saved/ArtAudioImport.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')
unreal.log('STORYBOOK_IMPORT_OK assets=' + str(len(REPORT)))
