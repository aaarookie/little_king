"""Read transparent PNGs to record atlas rectangles and provenance. No image pixels are written."""
from pathlib import Path
import argparse
import hashlib
import json
import numpy as np
from PIL import Image
from PreparePolishFrames import components

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT/'ArtSource/StorybookV1/V083'
ROLES = {
    'AngelWarrior': (240, ['Unit_AngelWarrior_Uncommon', 'Unit_AngelWarrior_Rare', 'Unit_AngelWarrior_Epic']),
    'AngelArcher': (245, ['Unit_AngelArcher_Uncommon', 'Unit_AngelArcher_Rare', 'Unit_AngelArcher_Epic']),
    'AngelPriest': (250, ['Unit_AngelPriest_Uncommon', 'Unit_AngelPriest_Rare', 'Unit_AngelPriest_Epic']),
    'DivineJudge': (285, ['Unit_DivineJudge']),
    'ChosenHighPriest': (280, ['Unit_ChosenHighPriest']),
}
SPELLS = ['Freeze', 'MariaNovice', 'MariaIntermediate', 'MariaAdvanced', 'MariaDivine',
          'BlackCloud', 'Lightning', 'Hurricane', 'DivineBlessing']
STATES = dict(Idle=[0, 1, 2, 3], Move=[4, 5, 6, 7], Attack=[8, 9, 10, 11], Hit=[12, 13], Death=[14, 15])

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--partial', action='store_true', help='Permit missing generated PNGs while authoring')
    args = parser.parse_args()
    SRC.mkdir(parents=True, exist_ok=True)
    generation_path = SRC/'generation.json'
    generations = json.loads(generation_path.read_text(encoding='utf-8-sig')) if generation_path.exists() else []
    if isinstance(generations, dict):
        generations = generations.get('items', [])
    by_file = {}
    for row in generations:
        path = ROOT/row['file']
        if not path.is_file():
            path = SRC/row['file']
        by_file[path.relative_to(ROOT).as_posix()] = row
    assets, animations, cards = [], [], []
    for role, (world_height, ids) in ROLES.items():
        path = SRC/'Animations'/('T_Anim_'+role+'.png')
        if not path.is_file():
            if args.partial: continue
            raise FileNotFoundError(path)
        im = Image.open(path)
        assert im.mode == 'RGBA', (role, 'A true RGBA sprite atlas is required')
        alpha = np.asarray(im.getchannel('A'))
        height, width = alpha.shape
        assert width == height, (role, 'Use a square 4x4 atlas')
        assert np.mean(alpha == 0) > .2, (role, 'Outside silhouette must be transparent')
        cell_w, cell_h = width/4, height/4
        bounds, frames = [], []
        cells = [[] for _ in range(16)]
        for box in components(alpha > 96):
            if box[4] < cell_w*cell_h*.0005:
                continue
            col = min(3, int((box[0]+box[2])/(2*cell_w)))
            row = min(3, int((box[1]+box[3])/(2*cell_h)))
            cells[row*4+col].append(box)
        assert all(cells), (role, 'Expected sixteen separated poses')
        for group in cells:
            main = group[0]
            b = list(main[:4])
            for box in group[1:]:
                gap = max(b[0]-box[2], box[0]-b[2], b[1]-box[3], box[1]-b[3], 0)
                if gap <= 5 and box[4] > main[4]*.01:
                    b = [min(b[0], box[0]), min(b[1], box[1]), max(b[2], box[2]), max(b[3], box[3])]
            bounds.append([max(0, b[0]-2), max(0, b[1]-2), min(width, b[2]+2), min(height, b[3]+2)])
        idle_height = float(np.median([b[3]-b[1] for b in bounds[:4]]))
        ppu = idle_height/world_height
        # Actor centres remain collision centres. Preserve a consistent ground anchor
        # across all poses, while the animation component performs visual depth sorting.
        foot_offset = world_height*.30
        for index, b in enumerate(bounds):
            yy, xx = np.nonzero(alpha[b[1]:b[3], b[0]:b[2]] > 96)
            frames.append(dict(index=index, uv=b[:2], size=[b[2]-b[0], b[3]-b[1]],
                pivot=[b[0]+float(np.median(xx)), b[3]-foot_offset*ppu]))
        file = path.relative_to(ROOT).as_posix()
        animations.append(dict(id=role, unitIds=ids, file=file, sha256=sha(path),
            width=width, height=height, ppu=ppu, visibleWorldHeight=world_height,
            transparentFraction=float(np.mean(alpha == 0)), states=STATES, frames=frames))
        assets.append(dict(kind='animation-atlas', id=role, file=file, sha256=sha(path),
            width=width, height=height, generation=by_file.get(file)))
    for spell in SPELLS:
        card_id = 'Spell_'+spell
        path = SRC/'Cards'/('T_Card_'+card_id+'.png')
        if not path.is_file():
            if args.partial: continue
            raise FileNotFoundError(path)
        width, height = Image.open(path).size
        file = path.relative_to(ROOT).as_posix()
        cards.append(dict(id=card_id, file=file, sha256=sha(path), width=width, height=height))
        assets.append(dict(kind='card-icon', id=card_id, file=file, sha256=sha(path),
            width=width, height=height, generation=by_file.get(file)))
    audio_path = SRC/'Audio/manifest.json'
    audio = json.loads(audio_path.read_text(encoding='utf-8')) if audio_path.exists() else []
    if not args.partial:
        assert len(animations) == 5 and len(cards) == 9 and len(audio) == 6
        assert all(row['generation'] and row['generation'].get('prompt') for row in assets), 'Record all imagegen prompts in generation.json'
    (SRC/'frames.json').write_text(json.dumps(animations, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    manifest = dict(version='0.8.3', date='2026-10-02', partial=args.partial,
        animations=animations, cards=cards, audio=audio, sources=assets,
        imagePolicy='Original generated PNG bytes retained; UV, pivot and texture settings only; no raster edits')
    (SRC/'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('V083_FRAME_RECTS', len(animations), '/5; cards', len(cards), '/9; audio', len(audio), '/6')

if __name__ == '__main__':
    main()
