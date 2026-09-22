"""UE 5.8 D importer. Only owns /Game/Art/StorybookV1/Polish; no maps, data tables or saves.
Run PreparePolishFrames.py and PreparePolishAudio.py first. Font import is a separate Slate-enabled editor step.
Set LK_POLISH_PARTIAL=1 only for incremental authoring; final validation requires all 28 atlases and 7 buildings.
"""
from pathlib import Path
import os,json,hashlib,struct,unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();SRC=ROOT/'ArtSource/StorybookV1/Polish'
BASE='/Game/Art/StorybookV1/Polish';LIB=unreal.EditorAssetLibrary;TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
catalog=json.loads((SRC/'catalog.json').read_text(encoding='utf-8-sig'))
frames=json.loads((SRC/'frames.json').read_text(encoding='utf-8'))
music=json.loads((SRC/'Music/manifest.json').read_text(encoding='utf-8'))
partial=os.environ.get('LK_POLISH_PARTIAL')=='1';report=[]
if not partial:
    assert len(frames)==28
    assert len(catalog['buildings'])==7 and all((ROOT/r['file']).exists() for r in catalog['buildings'])
for row in frames+music:
    assert hashlib.sha256((ROOT/row['file']).read_bytes()).hexdigest()==row['sha256'],row['id']

def save(obj):
    assert LIB.save_loaded_asset(obj);report.append(obj.get_path_name());return obj
def create(name,folder,cls,factory):
    path=BASE+'/'+folder+'/'+name
    return LIB.load_asset(path) if LIB.does_asset_exist(path) else TOOLS.create_asset(name,BASE+'/'+folder,cls,factory)
def import_file(path,folder):
    task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=BASE+'/'+folder;task.destination_name=path.stem
    task.automated=True;task.replace_existing=True;task.save=True;TOOLS.import_asset_tasks([task])
    objects=task.get_objects()
    if not objects:raise RuntimeError('Import failed: '+str(path))
    return objects[0]
def texture(path,folder,maxsize):
    tex=import_file(path,folder)
    for key,value in dict(srgb=True,compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        filter=unreal.TextureFilter.TF_BILINEAR,max_texture_size=maxsize).items():tex.set_editor_property(key,value)
    return save(tex)
material=LIB.load_asset('/Paper2D/MaskedUnlitSpriteMaterial')
for row in frames:
    tex=texture(ROOT/row['file'],'Animations',2048);sprites=[]
    for f in row['frames']:
        sprite=create('SP_'+row['id']+'_'+str(f['index']).zfill(2),'Animations',unreal.PaperSprite,unreal.PaperSpriteFactory())
        sprite.set_editor_property('source_texture',tex)
        sprite.set_editor_property('source_uv',unreal.Vector2D(*f['uv']))
        sprite.set_editor_property('source_dimension',unreal.Vector2D(*f['size']))
        sprite.set_editor_property('pixels_per_unreal_unit',row['ppu'])
        sprite.set_editor_property('pivot_mode',unreal.SpritePivotMode.CUSTOM)
        sprite.set_editor_property('custom_pivot_point',unreal.Vector2D(*f['pivot']))
        sprite.set_editor_property('snap_pivot_to_pixel_grid',False)
        sprite.set_editor_property('sprite_collision_domain',unreal.SpriteCollisionMode.NONE)
        sprite.set_editor_property('default_material',material)
        sprites.append(save(sprite))
    for state,indices in row['states'].items():
        clip=create('FB_'+row['id']+'_'+state,'Animations',unreal.PaperFlipbook,unreal.PaperFlipbookFactory())
        keys=[]
        for i in indices:
            key=unreal.PaperFlipbookKeyFrame();key.set_editor_property('sprite',sprites[i]);key.set_editor_property('frame_run',1);keys.append(key)
        clip.set_editor_property('key_frames',keys)
        clip.set_editor_property('frames_per_second',4 if state=='Idle' else 8)
        clip.set_editor_property('default_material',material)
        clip.set_editor_property('collision_source',unreal.FlipbookCollisionMode.NO_COLLISION)
        save(clip)
for row in catalog['buildings']:
    path=ROOT/row['file']
    if partial and not path.exists():continue
    tex=texture(path,'Buildings',1024)
    width,height=struct.unpack('>II',path.read_bytes()[16:24])
    sprite=create('SP_'+row['id'],'Buildings',unreal.PaperSprite,unreal.PaperSpriteFactory())
    sprite.set_editor_property('source_texture',tex);sprite.set_editor_property('source_uv',unreal.Vector2D(0,0))
    sprite.set_editor_property('source_dimension',unreal.Vector2D(width,height))
    sprite.set_editor_property('pixels_per_unreal_unit',width/row['worldWidth'])
    sprite.set_editor_property('sprite_collision_domain',unreal.SpriteCollisionMode.NONE)
    sprite.set_editor_property('default_material',material);save(sprite)
group=create('SC_Music','Music',unreal.SoundConcurrency,unreal.SoundConcurrencyFactory())
settings=group.get_editor_property('concurrency');settings.set_editor_property('max_count',2)
settings.set_editor_property('limit_to_owner',False);settings.set_editor_property('resolution_rule',unreal.MaxConcurrentResolutionRule.PREVENT_NEW)
group.set_editor_property('concurrency',settings);save(group)
for row in music:
    sound=import_file(ROOT/row['file'],'Music');sound.set_editor_property('looping',True)
    sound.set_editor_property('volume',1.0);sound.set_editor_property('concurrency_set',[group]);save(sound)
(ROOT/'Saved/PolishAssetsImport.json').write_text(json.dumps(dict(partial=partial,assets=report),indent=2),encoding='utf-8')
unreal.log('POLISH_ASSETS_OK count='+str(len(report))+' partial='+str(partial))
