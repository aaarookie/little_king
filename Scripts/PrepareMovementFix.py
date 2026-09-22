"""Audit original 2x2 walk images and write sprite metadata; never edit pixels."""
from pathlib import Path
import argparse
import hashlib
import json
import numpy as np
from PIL import Image
from PreparePolishFrames import components

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'ArtSource/StorybookV1/MovementFix'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--partial', action='store_true', help='Explicit incremental authoring only')
    args = parser.parse_args()
    old = {r['id']: r for r in json.loads((BASE.parent/'Polish/frames.json').read_text(encoding='utf-8-sig'))}
    expected = {i for i in old if not i.startswith('Building_') and i != 'Unit_TwoHeadedDragon'}
    rows = []
    doc = ['# 战斗显示修复：步行素材实际提示词', '',
           '图片使用既有角色作身份参考，由内置 image_gen 生成。原 PNG 字节保留；只用 UE SourceUV/尺寸/枢轴裁取四个姿势。原始回执与哈希见 [manifest](../ArtSource/StorybookV1/MovementFix/manifest.json)。', '']
    for receipt in sorted(BASE.glob('*.receipt.json')):
        row = json.loads(receipt.read_text(encoding='utf-8-sig'))
        path = ROOT / row['file']
        original = Path(row['originalToolFile'])
        if original.exists():
            assert sha(path) == sha(original), row['id']
        with Image.open(path) as im:
            assert im.mode == 'RGBA' and im.getchannel('A').getextrema() == (0, 255)
            a = np.asarray(im.getchannel('A'))
        h, w = a.shape
        groups = [[] for _ in range(4)]
        for box in components(a > 96):
            if box[4] < w*h*.0001:
                continue
            column = min(1, int((box[0]+box[2])/w))
            line = min(1, int((box[1]+box[3])/h))
            groups[line*2+column].append(box)
        assert all(groups), (row['id'], 'missing pose')
        bounds = []
        for group in groups:
            b = list(group[0][:4])
            for part in group[1:]:
                gap = max(b[0]-part[2], part[0]-b[2], b[1]-part[3], part[1]-b[3], 0)
                if gap <= 6 and part[4] > group[0][4]*.002:
                    b = [min(b[0],part[0]),min(b[1],part[1]),max(b[2],part[2]),max(b[3],part[3])]
            bounds.append([max(0,b[0]-2),max(0,b[1]-2),min(w,b[2]+2),min(h,b[3]+2)])
        previous = old[row['id']]
        idle = previous['frames'][0]
        local_height = idle['size'][1]/previous['ppu']
        ppu = float(np.median([b[3]-b[1] for b in bounds]))/local_height
        foot = (idle['uv'][1]+idle['size'][1]-idle['pivot'][1])/previous['ppu']
        frames = []
        for i, b in enumerate(bounds):
            # Follow head/upper torso instead of the swinging foot or broad cloak.
            upper_end = b[1]+max(1, int((b[3]-b[1])*.45))
            yy, xx = np.nonzero(a[b[1]:upper_end,b[0]:b[2]] > 96)
            pivot = [b[0]+float(np.median(xx)), b[3]-foot*ppu]
            frames.append(dict(index=i, uv=b[:2], size=[b[2]-b[0],b[3]-b[1]], pivot=pivot))
        rows.append({**row, 'sha256': sha(path), 'referenceSha256': sha(ROOT/row['reference']),
                     'width': w, 'height': h, 'ppu': ppu, 'frames': frames,
                     'originalToolFileHashVerified': original.exists(),
                     'replacesClip': '/Game/Art/StorybookV1/Polish/Animations/FB_'+row['id']+'_Move'})
        doc += ['## '+row['id'], '', '参考：`'+row['reference']+'`；实际尺寸：'+str(w)+'×'+str(h)+'。', '', row['prompt'], '']
    assert {r['id'] for r in rows} <= expected
    if not args.partial:
        assert {r['id'] for r in rows} == expected, sorted(expected-{r['id'] for r in rows})
    result = {'partial': args.partial, 'count': len(rows), 'expectedCount': len(expected), 'items': rows}
    (BASE/'manifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (ROOT/'docs/46-MovementFixPrompts.md').write_text('\n'.join(doc).rstrip()+'\n',encoding='utf-8')
    print('WALK_SOURCE_OK',len(rows),'/',len(expected),'partial='+str(args.partial))

if __name__ == '__main__':
    main()
