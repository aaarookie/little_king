"""Unreal 5.8 importer: owns only /Game/Art/StorybookV1/V083, never edits maps or saves."""
from pathlib import Path
import hashlib
import json
import os
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SRC = ROOT/'ArtSource/StorybookV1/V083'
BASE = '/Game/Art/StorybookV1/V083'
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
manifest = json.loads((SRC/'manifest.json').read_text(encoding='utf-8-sig'))
assert not manifest['partial'] or os.environ.get('LK_V083_PARTIAL') == '1', 'Final imports require a complete manifest'
rows = manifest['animations'] + manifest['cards'] + manifest['audio']
for row in rows:
    assert hashlib.sha256((ROOT/row['file']).read_bytes()).hexdigest() == row['sha256'], row['file']
report = []

def save(obj):
    assert LIB.save_loaded_asset(obj), obj.get_path_name()
    report.append(obj.get_path_name())
    return obj

def create(name, folder, cls, factory):
    path = BASE+'/'+folder+'/'+name
    return LIB.load_asset(path) if LIB.does_asset_exist(path) else TOOLS.create_asset(name, BASE+'/'+folder, cls, factory)

def import_file(path, folder):
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = BASE+'/'+folder
    task.destination_name = path.stem
    task.automated = task.replace_existing = task.save = True
    TOOLS.import_asset_tasks([task])
    objects = task.get_objects()
    if not objects:
        raise RuntimeError('Import failed: '+str(path))
    return objects[0]

def texture(path, folder, size):
    tex = import_file(path, folder)
    for key, value in dict(srgb=True, compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        lod_group=unreal.TextureGroup.TEXTUREGROUP_UI, mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        filter=unreal.TextureFilter.TF_BILINEAR, max_texture_size=size).items():
        tex.set_editor_property(key, value)
    return save(tex)

material = LIB.load_asset('/Paper2D/MaskedUnlitSpriteMaterial')
assert material
for row in manifest['animations']:
    tex = texture(ROOT/row['file'], 'Animations', 2048)
    sprites = []
    for frame in row['frames']:
        sprite = create('SP_'+row['id']+'_'+str(frame['index']).zfill(2), 'Animations', unreal.PaperSprite, unreal.PaperSpriteFactory())
        sprite.set_editor_property('source_texture', tex)
        sprite.set_editor_property('source_uv', unreal.Vector2D(*frame['uv']))
        sprite.set_editor_property('source_dimension', unreal.Vector2D(*frame['size']))
        sprite.set_editor_property('pixels_per_unreal_unit', row['ppu'])
        sprite.set_editor_property('pivot_mode', unreal.SpritePivotMode.CUSTOM)
        # UE 5.8 CustomPivotPoint is expressed in the full texture's pixel coordinates.
        sprite.set_editor_property('custom_pivot_point', unreal.Vector2D(*frame['pivot']))
        sprite.set_editor_property('snap_pivot_to_pixel_grid', False)
        sprite.set_editor_property('sprite_collision_domain', unreal.SpriteCollisionMode.NONE)
        sprite.set_editor_property('default_material', material)
        sprites.append(save(sprite))
    for state, indices in row['states'].items():
        clip = create('FB_'+row['id']+'_'+state, 'Animations', unreal.PaperFlipbook, unreal.PaperFlipbookFactory())
        keys = []
        for index in indices:
            key = unreal.PaperFlipbookKeyFrame()
            key.set_editor_property('sprite', sprites[index])
            key.set_editor_property('frame_run', 1)
            keys.append(key)
        clip.set_editor_property('key_frames', keys)
        clip.set_editor_property('frames_per_second', 4 if state == 'Idle' else 8)
        clip.set_editor_property('default_material', material)
        clip.set_editor_property('collision_source', unreal.FlipbookCollisionMode.NO_COLLISION)
        save(clip)
for row in manifest['cards']:
    texture(ROOT/row['file'], 'Cards', 512)
combat = LIB.load_asset('/Game/Art/StorybookV1/Audio/SC_Combat')
assert combat, 'Reuse the existing total combat sound voice cap'
for row in manifest['audio']:
    sound = import_file(ROOT/row['file'], 'Audio')
    sound.set_editor_property('looping', False)
    sound.set_editor_property('volume', .8)
    sound.set_editor_property('concurrency_set', [combat])
    save(sound)
result = dict(version='0.8.3', partial=manifest['partial'], assets=report,
    animationIdentities=len(manifest['animations']), cardIcons=len(manifest['cards']), sounds=len(manifest['audio']))
(ROOT/'Saved/V083AssetsImport.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
unreal.log('V083_ASSETS_IMPORT_OK assets='+str(len(report))+' partial='+str(manifest['partial']))
