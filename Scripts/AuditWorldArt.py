"""Read-only C source/media audit. Never rewrites image pixels."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import wave
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'ArtSource/StorybookV1/World'
catalog = json.loads((BASE / 'catalog.json').read_text(encoding='utf-8-sig'))
rows = []
for row in catalog:
    path = BASE / 'Images' / ('T_' + row['id'] + '.png')
    with Image.open(path) as image:
        alpha = image.getchannel('A') if image.mode == 'RGBA' else None
        record = dict(row, file=path.relative_to(ROOT).as_posix(),
            author='Little King project / built-in image_gen',
            license='generated artwork; applicable tool terms, not CC0',
            source='built-in image_gen, text-only generation, no external reference',
            generatedDate=datetime.fromtimestamp(path.stat().st_mtime).date().isoformat(),
            sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            width=image.width, height=image.height, mode=image.mode,
            alphaRange=alpha.getextrema() if alpha else None,
            processing='Original tool PNG copied unchanged; UE import scales display resolution only.')
        if row['kind'] != 'ground':
            assert alpha and alpha.getextrema()[0] == 0, row['id']
        else:
            assert not alpha or alpha.getextrema() == (255,255), row['id']
        rows.append(record)
(BASE / 'manifest.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2)+'\n',encoding='utf-8')

audio = json.loads((BASE / 'Audio/manifest.json').read_text(encoding='utf-8-sig'))
checks = []
for row in audio:
    path = ROOT / row['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest() == row['sha256']
    with wave.open(str(path),'rb') as stream:
        assert stream.getnchannels()==1 and stream.getframerate()==48000 and stream.getsampwidth()==2
        data=np.frombuffer(stream.readframes(stream.getnframes()),dtype='<i2').astype(float)/32768
    assert data[0]==data[-1]==0 and np.abs(data).max()<1
    checks.append(dict(key=row['key'],duration=len(data)/48000,
        measuredPeakDbFS=float(20*np.log10(np.abs(data).max())),
        rmsDbFS=float(20*np.log10(np.sqrt(np.mean(data**2)))),edgesZero=True))
(ROOT/'Saved/WorldArtSourceAudit.json').write_text(json.dumps(dict(images=len(rows),audio=checks),indent=2),encoding='utf-8')

doc = ['# C 批图片：实际生成提示词', '', f'{len(rows)} 张图片的原始文件均未进行像素后处理。实际尺寸、alpha 和 SHA-256 见 [manifest](../ArtSource/StorybookV1/World/manifest.json)。以下为工具实际输入的完整提示词；不是事后概括。音效为代码合成，参数与制作说明见 [脚本](../Scripts/PrepareWorldAudio.py)。', '']
for row in rows:
    doc += ['## '+row['id']+' · '+row['name'], '', f"实际尺寸：{row['width']}×{row['height']}；模式：{row['mode']}；日期：{row['generatedDate']}。", '', row['prompt'], '']
(ROOT/'docs/41-WorldArtPrompts.md').write_text('\n'.join(doc).rstrip()+'\n',encoding='utf-8')
print(json.dumps(dict(images=len(rows),sounds=len(checks),dimensions=sorted(set((r['width'],r['height']) for r in rows))),indent=2))
