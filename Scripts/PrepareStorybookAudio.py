"""Reproducible CC0 audio preparation and original plucked-string stingers.
Run with Python + numpy + soundfile; source archives are documented in downloads.json.
No image modification is performed by this script.
"""
from pathlib import Path
import hashlib
import json
import sys
import wave
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Saved/ArtAudioPython'))
import soundfile as sf

OUT = ROOT / 'ArtSource/StorybookV1/Audio'
OUT.mkdir(parents=True, exist_ok=True)
RATE = 48000
SOURCES = {
    'MeleeHit': ('impact-sounds', 'Audio/impactPunch_medium_000.ogg', -17),
    'RangedShoot': ('rpg-audio', 'Audio/knifeSlice.ogg', -19),
    'HitTaken': ('impact-sounds', 'Audio/impactSoft_medium_000.ogg', -23),
    'UnitDied': ('rpg-audio', 'Audio/dropLeather.ogg', -19),
    'CardPlay': ('rpg-audio', 'Audio/bookPlace1.ogg', -16),
    'UIClick': ('interface-sounds', 'Audio/click_003.ogg', -20),
}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def write(key, data, peak_db):
    # DC removal, gentle de-click ramps, peak ceiling; no clipping or arbitrary time truncation.
    data = np.asarray(data, dtype=np.float64)
    data -= data.mean()
    ramp = min(int(RATE * .006), len(data) // 3)
    data[:ramp] *= np.linspace(0, 1, ramp)
    data[-ramp:] *= np.linspace(1, 0, ramp)
    peak = float(np.abs(data).max())
    if peak > 0:
        data *= 10 ** (peak_db / 20) / peak
    path = OUT / ('S_' + key + '.wav')
    sf.write(path, data, RATE, subtype='PCM_16')
    return {'file': str(path.relative_to(ROOT)).replace('\\', '/'), 'sha256': digest(path),
            'sampleRate': RATE, 'channels': 1, 'peakDbFS': peak_db,
            'durationSeconds': round(len(data) / RATE, 4)}

rows = []
for key, (pack, name, ceiling) in SOURCES.items():
    src = OUT / 'Originals' / pack / Path(name).name
    if not src.is_file():
        src = ROOT / 'Saved/ArtAudioDownloads' / pack / name
    data, rate = sf.read(src, always_2d=True)
    data = data.mean(axis=1)
    # Upsampling from 44.1 kHz only, with linear interpolation for these short, soft foley samples.
    if rate != RATE:
        assert rate <= RATE
        data = np.interp(np.arange(round(len(data) * RATE / rate)) * rate / RATE,
                         np.arange(len(data)), data)
    row = write(key, data, ceiling)
    row.update(key=key, author='Kenney', license='CC0-1.0',
               page='https://kenney.nl/assets/' + pack, sourceFile=name, sourceSha256=digest(src),
               processing='mono mix, resample to 48 kHz if needed, DC removal, 6 ms edge fades, peak normalization')
    rows.append(row)

# Original short phrases in a consistent soft plucked-string timbre.
# Parameters constitute the creation brief; no AI audio model or sampled instrument is used.
phrases = {
    'BattleStart': [(0, 60, .6), (.16, 67, .6), (.32, 72, .9)],
    'Victory': [(0, 60, .7), (.18, 64, .7), (.36, 67, .8), (.60, 72, 1.3), (.60, 64, 1.1)],
    'Defeat': [(0, 69, .8), (.24, 65, .8), (.48, 64, 1.2), (.48, 57, 1.2)],
}
for key, notes in phrases.items():
    duration = max(start + length for start, _, length in notes) + .15
    data = np.zeros(round(duration * RATE))
    for start, midi, length in notes:
        t = np.arange(round(length * RATE)) / RATE
        hz = 440 * 2 ** ((midi - 69) / 12)
        tone = sum((1 / h ** 2) * np.sin(2 * np.pi * hz * h * t) * np.exp(-t * (3 + h))
                   for h in range(1, 6))
        tone *= np.minimum(1, t / .004)
        tone[-480:] *= np.linspace(1, 0, 480)
        offset = round(start * RATE)
        data[offset:offset + len(tone)] += tone
    row = write(key, data, -14)
    row.update(key=key, author='Little King project / Codex procedural composition',
               license='project-original; no third-party samples', sourceFile='Scripts/PrepareStorybookAudio.py',
               creationBrief='Soft warm five-harmonic plucked strings; brief royal signals without harsh fanfare.',
               notes=notes, processing='original synthesis at 48 kHz; DC removal, 6 ms edge fades, -14 dBFS peak')
    rows.append(row)

(OUT / 'manifest.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'audioFiles': len(rows), 'durations': {r['key']: r['durationSeconds'] for r in rows}}, indent=2))
