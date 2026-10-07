"""Inspect original generated alpha and write metadata only. Never modify raster pixels."""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image
from PreparePolishFrames import components

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT/'ArtSource/StorybookV1/V084'
IDS = ['WallCommon', 'WallRare', 'WallEpic', 'WallImperial', 'SiegeCart', 'MageTower', 'HealingStation']
STATES = dict(Idle=[0, 1, 2, 3], Move=[4, 5, 6, 7], Attack=[8, 9, 10, 11], Hit=[12, 13], Death=[14, 15])

def checksum(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def inspect(path):
    img = Image.open(path)
    assert img.mode == 'RGBA', path
    alpha = np.asarray(img.getchannel('A'))
    assert (alpha == 0).mean() > .15, path
    ys, xs = np.nonzero(alpha > 96)
    return alpha, [int(xs.min()), int(ys.min()), int(xs.max()+1), int(ys.max()+1)]

def main():
    generations = json.loads((SRC/'generation.json').read_text(encoding='utf-8'))
    by_file = {g['file']: g for g in generations}
    animations, cards, sources = [], [], []
    for role in IDS:
        card_path = SRC/'Images'/('T_'+role+'.png')
        alpha, box = inspect(card_path)
        cards.append(dict(id='Card_'+role, file=card_path.relative_to(ROOT).as_posix(),
            importName='T_Card_'+role, sha256=checksum(card_path), width=alpha.shape[1], height=alpha.shape[0]))
        path = SRC/'Animations/T_Anim_SiegeCart.png' if role == 'SiegeCart' else card_path
        alpha, box = inspect(path)
        height, width = alpha.shape
        frames = []
        if role == 'SiegeCart':
            assert width == height
            cell_w, cell_h = width/4, height/4
            cells = [[] for _ in range(16)]
            for b in components(alpha > 96):
                if b[4] < cell_w*cell_h*.002: continue
                col = min(3, int((b[0]+b[2])/(2*cell_w)))
                row = min(3, int((b[1]+b[3])/(2*cell_h)))
                cells[row*4+col].append(b)
            assert all(cells), ('Expected separated sixteen poses', [len(c) for c in cells])
            bounds = []
            for i, group in enumerate(cells):
                b = list(max(group, key=lambda x: x[4])[:4])
                # No recropping/raster edits: source UV rects stay in the original PNG.
                bounds.append([max(0,b[0]-1), max(0,b[1]-1), min(width,b[2]+1), min(height,b[3]+1)])
            ppu = float(np.median([b[3]-b[1] for b in bounds[:4]]))/230
            for i, b in enumerate(bounds):
                ys, xs = np.nonzero(alpha[b[1]:b[3],b[0]:b[2]] > 96)
                # Centre of rigid chassis rather than moving ram avoids recoil sliding.
                cell_left = (i % 4)*cell_w
                frames.append(dict(index=i, uv=b[:2], size=[b[2]-b[0], b[3]-b[1]],
                    pivot=[cell_left+cell_w*.44, b[3]-230*.3*ppu]))
            states = STATES
        else:
            world_size = 300 if role.startswith('Wall') else (360 if role == 'MageTower' else 310)
            span = box[2]-box[0] if role.startswith('Wall') else box[3]-box[1]
            ppu = span/world_size
            pivot = [(box[0]+box[2])/2, (box[1]+box[3])/2] if role.startswith('Wall') else [(box[0]+box[2])/2, box[3]-world_size*.3*ppu]
            frames = [dict(index=0, uv=box[:2], size=[box[2]-box[0],box[3]-box[1]],pivot=pivot)]
            states = {state: [0] for state in STATES}
        animations.append(dict(id=role, unitIds=['Unit_'+role], file=path.relative_to(ROOT).as_posix(),
            importName='T_Anim_'+role, sha256=checksum(path), width=width, height=height, ppu=ppu,
            transparentFraction=float((alpha == 0).mean()), states=states, frames=frames,
            authoredPoseCount=len(frames), worldWidth=300 if role.startswith('Wall') else None))
    for g in generations:
        path = ROOT/g['file']
        alpha, _ = inspect(path)
        sources.append(dict(file=g['file'], sha256=checksum(path), generation=g,
            width=alpha.shape[1], height=alpha.shape[0]))
    audio = json.loads((SRC/'Audio/manifest.json').read_text(encoding='utf-8'))
    assert len(sources) == 8 and len(audio) == 7
    manifest = dict(version='0.8.4', date='2026-10-08', partial=False, animations=animations, cards=cards, audio=audio,
        sources=sources, imagePolicy='Original generated PNG bytes retained. UV, pivot, scale and texture settings only; no pixel edits.')
    (SRC/'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    (SRC/'frames.json').write_text(json.dumps(animations, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('V084_FRAMES_OK identities=7 uniquePoses=22 cards=7 sounds=7')

if __name__ == '__main__':
    main()
