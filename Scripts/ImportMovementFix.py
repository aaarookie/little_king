"""UE 5.8: import walk replacements; retain old atlases and non-movement clips."""
from pathlib import Path
import hashlib
import json
import os
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
manifest = json.loads((ROOT/'ArtSource/StorybookV1/MovementFix/manifest.json').read_text(encoding='utf-8-sig'))
assert not manifest['partial'] or os.environ.get('LK_WALK_PARTIAL') == '1'
if not manifest['partial']:
    assert manifest['count'] == manifest['expectedCount'] == 24
BASE = '/Game/Art/StorybookV1/MovementFix'
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
material = LIB.load_asset('/Paper2D/MaskedUnlitSpriteMaterial')
assets = []

def save(obj):
    assert LIB.save_loaded_asset(obj)
    assets.append(obj.get_path_name())
    return obj

for row in manifest['items']:
    path = ROOT/row['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest() == row['sha256']
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = BASE
    task.destination_name = path.stem
    task.automated = task.replace_existing = task.save = True
    TOOLS.import_asset_tasks([task])
    texture = task.get_objects()[0]
    for key, value in dict(srgb=True, compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        lod_group=unreal.TextureGroup.TEXTUREGROUP_UI, mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        filter=unreal.TextureFilter.TF_BILINEAR, max_texture_size=2048).items():
        texture.set_editor_property(key, value)
    save(texture)
    keys = []
    for f in row['frames']:
        name = 'SP_Walk_'+row['id']+'_'+str(f['index']).zfill(2)
        sprite = LIB.load_asset(BASE+'/'+name) if LIB.does_asset_exist(BASE+'/'+name) else TOOLS.create_asset(name,BASE,unreal.PaperSprite,unreal.PaperSpriteFactory())
        sprite.set_editor_property('source_texture', texture)
        sprite.set_editor_property('source_uv', unreal.Vector2D(*f['uv']))
        sprite.set_editor_property('source_dimension', unreal.Vector2D(*f['size']))
        sprite.set_editor_property('pixels_per_unreal_unit', row['ppu'])
        sprite.set_editor_property('pivot_mode', unreal.SpritePivotMode.CUSTOM)
        sprite.set_editor_property('custom_pivot_point', unreal.Vector2D(*f['pivot']))
        sprite.set_editor_property('snap_pivot_to_pixel_grid', False)
        sprite.set_editor_property('sprite_collision_domain', unreal.SpriteCollisionMode.NONE)
        sprite.set_editor_property('default_material', material)
        save(sprite)
        key = unreal.PaperFlipbookKeyFrame()
        key.set_editor_property('sprite', sprite)
        key.set_editor_property('frame_run', 1)
        keys.append(key)
    # Dedicated override: reimporting the historical D batch cannot undo this fix.
    name = 'FB_Walk_'+row['id']
    clip = LIB.load_asset(BASE+'/'+name) if LIB.does_asset_exist(BASE+'/'+name) else TOOLS.create_asset(name,BASE,unreal.PaperFlipbook,unreal.PaperFlipbookFactory())
    clip.set_editor_property('key_frames', keys)
    clip.set_editor_property('frames_per_second', 8)
    clip.set_editor_property('default_material', material)
    clip.set_editor_property('collision_source', unreal.FlipbookCollisionMode.NO_COLLISION)
    save(clip)
(ROOT/'Saved/MovementFixImport.json').write_text(json.dumps({'partial':manifest['partial'],'assets':assets},indent=2)+'\n',encoding='utf-8')
unreal.log('MOVEMENT_FIX_IMPORT_OK units='+str(manifest['count'])+' assets='+str(len(assets)))
