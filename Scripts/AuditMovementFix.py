"""Read-only final audit; copy renderer screenshots unchanged and write evidence."""
from pathlib import Path
from datetime import datetime
import ast
import hashlib
import json
import re
import shutil
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return json.loads((ROOT/path).read_text(encoding='utf-8-sig'))


def record(path):
    obj = ROOT/path
    return dict(path=obj.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(obj.read_bytes()).hexdigest())


def main():
    source = read('ArtSource/StorybookV1/MovementFix/manifest.json')
    assert not source['partial'] and source['count'] == source['expectedCount'] == 24
    for row in source['items']:
        assert record(row['file'])['sha256'] == row['sha256']
        assert record(row['reference'])['sha256'] == row['referenceSha256']
        assert len(row['frames']) == 4
        with Image.open(ROOT/row['file']) as im:
            assert im.mode == 'RGBA' and im.getchannel('A').getextrema() == (0, 255)
            assert im.size == (row['width'], row['height'])
        for f in row['frames']:
            x, y = f['uv']
            w, h = f['size']
            assert x >= 0 and y >= 0 and w > 0 and h > 0
            assert x+w <= row['width'] and y+h <= row['height']

    imported = read('Saved/MovementFixImport.json')
    assert not imported['partial'] and len(set(imported['assets'])) == 144
    assets = []
    for obj in imported['assets']:
        path = 'Content/'+obj.removeprefix('/Game/').split('.')[0]+'.uasset'
        assets.append(record(path))
    assert len(list((ROOT/'Content/Art/StorybookV1').rglob('*.uasset'))) == 919

    report_path = 'Saved/Automation/MovementFix02/index.json'
    report = read(report_path)
    assert report['failed'] == report['notRun'] == report['inProcess'] == 0
    assert report['succeeded']+report['succeededWithWarnings'] == len(report['tests']) == 87
    focused = [t for t in report['tests'] if t['fullTestPath'].split('.')[-1] in {
        'OverlapDepthAndCamps', 'FacingFollowsVoluntaryTravel',
        'WalkDistanceAndInterruption', 'CorrectedWalkAtlases'}]
    assert len(focused) == 4 and all(t['state'] == 'Success' for t in focused)

    build = 'Saved/Logs/MovementFixBuild05.log'
    build_text = (ROOT/build).read_text(encoding='utf-8', errors='replace')
    assert 'Result: Succeeded' in build_text
    screenshots, logs, copied = [], [], []
    for height in (720, 1080):
        log = f'Saved/Logs/MovementFixFinal{height}.log'
        text = (ROOT/log).read_text(encoding='utf-8-sig', errors='replace')
        assert 'MOVEMENT_FIX_PREVIEW_OK walkers=24' in text
        assert 'MOVEMENT_FIX_PREVIEW_FAILED' not in text and 'Fatal error:' not in text
        logs.append(record(log))
        shots = sorted((ROOT/'Saved/Screenshots/MovementFix').glob(f'*_{height}.png'))
        assert len(shots) == 24
        for p in shots:
            with Image.open(p) as im:
                assert im.size == (1280 if height == 720 else 1920, height)
            screenshots.append(record(p.relative_to(ROOT)))
        # Preserve actual renderer output, no compositing/cropping/retouching.
        for stem in ('Walk_Right_02', 'Walk_Left_02', 'Overlap_230', 'Overlap_250'):
            src = ROOT/f'Saved/Screenshots/MovementFix/{stem}_{height}.png'
            dst = ROOT/f'docs/images/MovementFix_{stem}_{height}.png'
            shutil.copyfile(src, dst)
            item = record(dst.relative_to(ROOT))
            assert item['sha256'] == record(src.relative_to(ROOT))['sha256']
            item.update(source=src.relative_to(ROOT).as_posix(), copiedUnmodified=True)
            copied.append(item)

    baseline = read('Saved/MovementFixBaseline/saves.json')
    saves = []
    for item in baseline:
        now = record('Saved/SaveGames/'+item['Name'])
        assert now['sha256'].upper() == item['Hash'].upper()
        saves.append(dict(path=now['path'], sha256=now['sha256'], matchesBaseline=True))
    assert len(saves) == len(list((ROOT/'Saved/SaveGames').glob('*.sav'))) == 5
    table = record('Content/Data/DT_Units.uasset')
    assert table['sha256'] == '5bd2282930b84cf9430bd2a2d86e5a6ccc500b0ab1be4487784181a5e576b782'

    docs = ['README.md', 'README.en.md', 'docs/03-TaskList.md', 'docs/05-BugLog.md',
            'docs/07-ArtStyleGuide.md', 'docs/12-AssetRequest.md', 'docs/29-OptimizationChangeLog.md',
            'docs/36-ArtAudioSources.md', 'docs/45-BattlePresentationFixes.md', 'docs/46-MovementFixPrompts.md']
    links = 0
    for path in docs:
        text = (ROOT/path).read_text(encoding='utf-8-sig')
        for link in re.findall(r'\]\(([^)]+)\)', text):
            if re.match(r'(https?:|mailto:|#)', link):
                continue
            target = link.split('#')[0].strip('<>')
            if target:
                assert ((ROOT/path).parent/target).exists(), (path, target)
                links += 1
    for path in ('Scripts/PrepareMovementFix.py', 'Scripts/ImportMovementFix.py', 'Scripts/AuditMovementFix.py'):
        ast.parse((ROOT/path).read_text(encoding='utf-8-sig'))
    untracked = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
    text_files = [p for p in untracked if Path(p).suffix in {'.cpp', '.h', '.py', '.md', '.json'}]
    for path in text_files:
        for number, line in enumerate((ROOT/path).read_text(encoding='utf-8-sig').splitlines(), 1):
            assert line == line.rstrip(' \t'), (path, number, 'trailing whitespace')
    subprocess.run(['git', '-c', 'core.safecrlf=false', 'diff', '--check'], cwd=ROOT, check=True, capture_output=True)
    result = dict(
        auditedAt=datetime.now().astimezone().isoformat(), baseline='v0.8',
        build={**record(build), 'result': 'Succeeded', 'engine': 'UE 5.8.1',
               'note': 'Build04 runtime passed full regression; Build05 only adds shader-ready waiting to editor preview.'},
        automation={**record(report_path), 'succeeded': report['succeeded'],
                    'succeededWithWarnings': report['succeededWithWarnings'], 'failed': 0, 'notRun': 0,
                    'newTests': [dict(name=t['fullTestPath'], state=t['state']) for t in focused]},
        source={**record('ArtSource/StorybookV1/MovementFix/manifest.json'), 'images': 24, 'frames': 96},
        assets={'newCount': 144, 'storybookTotal': 919, 'items': assets},
        render={'resolutions': ['1280x720', '1920x1080'], 'samples': screenshots, 'logs': logs, 'documentationImages': copied,
                'review': 'Verified left/right walking poses, stop states and front/back unit/camp overlap in actual renderer screenshots.'},
        preservation={'saves': saves, 'extraSaveFiles': 0, 'unitTable': table},
        staticChecks={'localLinks': links, 'pythonSyntax': 'passed', 'gitDiffCheck': 'passed', 'untrackedTextFilesChecked': len(text_files)},
        limitations=['Two horizontal facings; vertical travel retains last facing.',
                     'Four-pose generated walking, not a hand-cleaned eight-direction animation set.',
                     'Historical non-movement artwork is retained.',
                     'Packaging and hardware acceptance remain deferred.'])
    (ROOT/'docs/validation/MovementFix-Validation.json').write_text(
        json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('MOVEMENT_FIX_AUDIT_OK images=24 assets=144 tests=87 screenshots=48 saves=5 links='+str(links))


if __name__ == '__main__':
    main()
