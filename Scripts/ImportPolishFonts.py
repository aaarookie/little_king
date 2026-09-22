"""UE 5.8: import OFL font faces and create self-contained runtime composite fonts."""
from pathlib import Path
import json,unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
BASE='/Game/Art/StorybookV1/Polish/Fonts'
LIB=unreal.EditorAssetLibrary;TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
report=[]
for kind,filename in [('Body','NotoSansCJKsc-Regular.otf'),('Title','NotoSerifCJKsc-SemiBold.otf')]:
    task=unreal.AssetImportTask();task.filename=str(ROOT/'ArtSource/StorybookV1/Polish/Fonts'/filename)
    task.destination_path=BASE;task.destination_name='FF_'+kind
    task.automated=True;task.replace_existing=True;task.save=True
    factory=unreal.FontFileImportFactory();factory.set_editor_property('batch_create_font_asset',unreal.BatchCreateFontAsset.YES)
    task.factory=factory;TOOLS.import_asset_tasks([task])
    face=task.get_objects()[0];face.set_editor_property('loading_policy',unreal.FontLoadingPolicy.INLINE)
    LIB.save_loaded_asset(face);report.append(face.get_path_name())
    name='F_'+kind;path=BASE+'/'+name
    candidates=[LIB.load_asset(a) for a in LIB.list_assets(BASE) if 'FF_'+kind in a]
    unreal.log('FONT_CANDIDATES '+str([(a.get_name(),a.get_class().get_name()) for a in candidates]))
    font=next((a for a in candidates if isinstance(a,unreal.Font)),None)
    if not font: raise RuntimeError('Font factory did not create companion font for '+kind)
    if LIB.does_asset_exist(path):
        target=LIB.load_asset(path)
        target.set_editor_property('composite_font',font.get_editor_property('composite_font'))
        target.set_editor_property('legacy_font_name',font.get_editor_property('legacy_font_name'))
        LIB.delete_asset(font.get_path_name())
        font=target
    else:
        assert LIB.rename_asset(font.get_path_name(),path)
        font=LIB.load_asset(path)
    LIB.save_loaded_asset(font);report.append(font.get_path_name())
(ROOT/'Saved/PolishFontsImport.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('POLISH_FONTS_OK')
