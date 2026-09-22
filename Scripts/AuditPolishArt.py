"""Read-only image/audio/source audit. Writes provenance JSON and actual prompt documentation."""
from pathlib import Path
from datetime import datetime
import hashlib,json,re,wave
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'ArtSource/StorybookV1/Polish'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
catalog=read(BASE/'catalog.json');frames={r['id']:r for r in read(BASE/'frames.json')}
images=[]
doc=['# D 批实际生成提示词与来源','',
    '本页由 Scripts/AuditPolishArt.py 从逐项生成回执整理。图片原 PNG 不改像素；切帧矩形和整图坐标枢轴仅用于 UE 显示。生成图不标为 CC0，不编造工具未提供的模型版本或种子。',
    '', '图片/字体哈希与许可见 [manifest](../ArtSource/StorybookV1/Polish/manifest.json)，整合及新手教程见 [42](42-PolishArtIntegration.md)。','']
for category,folder in [('animations','Animations'),('buildings','Buildings')]:
    for row in catalog[category]:
        receipt=read(BASE/folder/(row['id']+'.receipt.json'))
        row['prompt']=receipt['prompt']
        if row.get('name','').startswith('NSLOCTEXT'):
            row['name']=re.findall(r'"([^"]*)"',row['name'])[-1]
        path=ROOT/row['file'];reference=ROOT/row['reference']
        original=Path(receipt['path'])
        if original.exists():assert sha(original)==sha(path),('Original image was modified',row['id'])
        with Image.open(path) as im:
            assert im.mode=='RGBA' and im.getchannel('A').getextrema()==(0,255),row['id']
            entry=dict(id=row['id'],name=row.get('name',row['id']),file=row['file'],sha256=sha(path),
                width=im.width,height=im.height,mode=im.mode,alphaRange=[0,255],
                generatedDate=datetime.fromtimestamp(path.stat().st_mtime).date().isoformat(),
                author='Little King project / built-in image_gen',license='Generated art; applicable tool terms, not CC0',
                prompt=receipt['prompt'],reference=row['reference'],referenceSha256=sha(reference),
                receipt=(BASE/folder/(row['id']+'.receipt.json')).relative_to(ROOT).as_posix(),
                originalToolFileAvailable=original.exists(),originalToolFileHashVerified=original.exists(),
                processing='Original PNG unchanged. UE SourceUV/SourceDimension/custom pivot only; no pixel edit.')
        if category=='animations':
            f=frames[row['id']];assert f['sha256']==entry['sha256'] and len(f['frames'])==16
            for frame in f['frames']:
                x,y=frame['uv'];w,h=frame['size'];assert x>=0 and y>=0 and x+w<=entry['width'] and y+h<=entry['height']
            entry['frameCount']=16;entry['states']=f['states']
        images.append(entry)
        doc += ['## '+entry['id']+' · '+entry['name'],'',
            f"实际 {entry['width']}×{entry['height']} RGBA；日期 {entry['generatedDate']}；参考 `{row['reference']}`。",'',receipt['prompt'],'']

music=read(BASE/'Music/manifest.json');music_checks=[]
doc+=['## 音乐 · MiniMax-Music3 本地生成','',
      '以下是实际 caption、lyrics 和参数。请求时长 48 秒，模型实际输出时长不同；不把提示词中的乐器、BPM 或调式当作已由音频分析确认。原始文件、原始回执和循环处理结果全部保留。','']
for row in music:
    path=ROOT/row['file'];receipt=read(BASE/'Music/Originals'/(row['id']+'.json'))
    assert sha(path)==row['sha256'] and sha(ROOT/row['source'])==row['sourceSha256']==receipt['sha256']
    with wave.open(str(path),'rb') as stream:
        assert (stream.getnchannels(),stream.getframerate(),stream.getsampwidth())==(2,44100,2)
        samples=np.frombuffer(stream.readframes(stream.getnframes()),dtype='<i2').astype(float).reshape(-1,2)/32768
    assert np.isfinite(samples).all() and np.abs(samples).max()<=.709 and len(samples)>20*44100
    music_checks.append(dict(id=row['id'],peakDbFS=float(20*np.log10(np.abs(samples).max())),
        rmsDbFS=float(20*np.log10(np.sqrt(np.mean(samples**2)))),
        seconds=len(samples)/44100,seamMaxDelta=float(np.abs(samples[0]-samples[-1]).max()),hashVerified=True))
    doc+=['### '+row['id']+' · '+receipt['name'],'',
        f"原始 {receipt['actualSeconds']:.3f} 秒 → 循环 {row['seconds']:.3f} 秒；seed={receipt['seed']}；steps={receipt['num_inference_steps']}；vram_mode={receipt['vram_mode']}；请求 audio_duration={receipt['audio_duration']}。",'',
        '**Caption**','',receipt['caption'],'','**Lyrics**','','```text',receipt['lyrics'],'```','']

fonts=[]
for family,file,license_file in [('Sans','NotoSansCJKsc-Regular.otf','NotoSans-OFL.txt'),('Serif','NotoSerifCJKsc-SemiBold.otf','NotoSerif-OFL.txt')]:
    path=BASE/'Fonts'/file;license_path=BASE/'Licenses'/license_file
    assert path.exists() and 'SIL OPEN FONT LICENSE' in license_path.read_text(encoding='utf-8-sig')
    fonts.append(dict(file=path.relative_to(ROOT).as_posix(),sha256=sha(path),author='Noto CJK contributors',
        source='https://raw.githubusercontent.com/notofonts/noto-cjk/main/'+family+'/OTF/SimplifiedChinese/'+file,
        license='SIL Open Font License 1.1',licenseFile=license_path.relative_to(ROOT).as_posix(),licenseSha256=sha(license_path),
        processing='Original unmodified OTF; UE inline FontFace and runtime composite Font. No model prompt.'))

manifest=dict(images=images,music=music,fonts=fonts,
    musicLicense=dict(source='https://huggingface.co/MiniMaxAI/MiniMax-Music3/raw/main/LICENSE',
        file='ArtSource/StorybookV1/Polish/Licenses/MiniMax-Music3-LICENSE.txt',sha256=sha(BASE/'Licenses/MiniMax-Music3-LICENSE.txt'),
        label='MiniMax-Music3 COMMUNITY LICENSE; generated output, not CC0'),
    originalPromptFiles='Animations/*.receipt.json, Buildings/*.receipt.json, Music/Originals/*.json')
(BASE/'catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(BASE/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(ROOT/'docs/43-PolishArtPrompts.md').write_text('\n'.join(doc).rstrip()+'\n',encoding='utf-8')
result=dict(images=len(images),atlases=len(frames),frames=sum(len(r['frames']) for r in frames.values()),
            music=music_checks,fonts=len(fonts),originalImagesUnmodified=True)
(ROOT/'Saved/PolishArtSourceAudit.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
