"""Read-only image inspection and provenance manifest; does not alter source pixels. Requires Pillow."""
from pathlib import Path
from PIL import Image
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'ArtSource/StorybookV1/Battle'
catalog = json.loads((BASE / 'catalog.json').read_text(encoding='utf-8-sig'))['items']
cards = json.loads((BASE / 'Prompts/cards.json').read_text(encoding='utf-8-sig'))
if isinstance(cards, dict):
    cards = cards['items']
card_map = {item['id']: item for item in cards}
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
result = []
for index, item in enumerate(catalog):
    for kind in (['sprite', 'card'] if item['card'] else ['sprite']):
        path = BASE / ('Images/T_' if kind == 'sprite' else 'Cards/T_Card_')
        path = path.with_name(path.name + item['id'] + '.png')
        with Image.open(path) as image:
            alpha = image.getchannel('A') if 'A' in image.getbands() else None
            alpha_range = list(alpha.getextrema()) if alpha else None
            assert image.width == image.height, str(path)
            if kind == 'sprite':
                assert image.mode == 'RGBA' and alpha_range == [0, 255], str(path)
                # Some unmodified outputs contain 1/255 alpha specks, invisible under the masked material.
                assert all(alpha.getpixel(p) <= 1 for p in [(0, 0), (0, image.height-1), (image.width-1, 0), (image.width-1, image.height-1)])
            else:
                assert alpha is None or alpha_range == [255, 255], str(path)
            record = dict(id=item['id'], name=item['name'], kind=kind,
                file=path.relative_to(ROOT).as_posix(), sha256=sha(path),
                size=list(image.size), mode=image.mode, alpha=alpha_range,
                alphaBounds=list(alpha.getbbox()) if alpha else None,
                maskedBounds=list(alpha.point(lambda a: 255 if a >= 85 else 0).getbbox()) if alpha else None,
                generator='Built-in image_gen', model=None, seed=None,
                date='2026-09-19' if kind == 'sprite' and index < 15 else '2026-09-20',
                prompt=item['spritePrompt'] if kind == 'sprite' else card_map[item['id']]['prompt'],
                postProcessing='None; original tool output copied without pixel changes')
            if kind == 'card':
                reference = ROOT / card_map[item['id']]['reference']
                record['reference'] = reference.relative_to(ROOT).as_posix()
                record['referenceSha256'] = sha(reference)
            else:
                record['canvasWorldHeightCm'] = item['worldHeight']
            result.append(record)
assert len(result) == 37
(BASE / 'manifest.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(dict(images=len(result), sprites=20, cards=17, dimensions=sorted(set(tuple(i['size']) for i in result))), ensure_ascii=False))
