"""C-batch original sound design: deterministic synthesis, no third-party samples.
No image manipulation. Run with bundled Python + numpy; WAV uses stdlib wave.
"""
from pathlib import Path
import hashlib
import json
import numpy as np
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'ArtSource/StorybookV1/World/Audio'
OUT.mkdir(parents=True, exist_ok=True)
RATE = 48000
# key: family, seconds, base Hz, end Hz, peak dBFS, minimum global interval.
# Independent random seeds keep adding sounds from changing previously generated files.
SPECS = {
 'BowShot': ('pluck', .25, 190, 100, -21, .08),
 'SpearShot': ('air', .32, 180, 90, -21, .10),
 'MagicShot': ('bell', .32, 500, 300, -23, .12),
 'FireballCast': ('air', .35, 200, 480, -19, .10),
 'FireballImpact': ('rumble', .55, 120, 45, -18, .10),
 'Heal': ('chime', .65, 520, 780, -24, .30),
 'Summon': ('air', .55, 170, 370, -21, .20),
 'Sacrifice': ('bell', .28, 210, 110, -25, .20),
 'Revive': ('chime', 1.05, 220, 440, -18, .30),
 'Backstab': ('air', .22, 600, 160, -20, .15),
 'Coin': ('bell', .28, 1200, 980, -23, .18),
 'Mimic': ('chime', .40, 390, 620, -22, .20),
 'Dash': ('air', .35, 150, 550, -21, .15),
 'Empower': ('chime', .60, 250, 375, -21, .25),
 'HeavyHit': ('rumble', .40, 95, 38, -20, .15),
 'Stun': ('bell', .35, 850, 650, -24, .20),
 'Freeze': ('bell', .50, 1500, 1000, -25, .20),
 'Burn': ('air', .40, 400, 200, -26, .30),
 'IceBreath': ('air', .48, 600, 350, -23, .12),
 'FireBreath': ('rumble', .45, 170, 75, -23, .12),
 'SiegeShot': ('pluck', .40, 110, 50, -20, .20),
 'SiegeImpact': ('rumble', .65, 85, 30, -19, .20),
 'Footstep': ('rumble', .18, 70, 40, -28, .55),
 'CampCommand': ('pluck', .24, 650, 500, -22, .12),
 'NodeEnter': ('chime', .55, 330, 495, -21, .20),
 'Rest': ('chime', 1.00, 440, 660, -20, .40),
 'Market': ('bell', .65, 750, 1100, -23, .30),
 'Upgrade': ('chime', 1.15, 330, 880, -19, .40),
}
rows = []
for key, (family, duration, start, end, ceiling, gap) in SPECS.items():
    seed = int.from_bytes(hashlib.sha256(key.encode()).digest()[:4], 'little')
    rng = np.random.default_rng(seed)
    t = np.arange(round(duration * RATE)) / RATE
    p = t / duration
    phase = 2 * np.pi * (start * t + (end - start) * t * t / (2 * duration))
    noise = rng.standard_normal(len(t))
    soft_noise = np.convolve(noise, np.ones(24) / 24, mode='same')
    if family == 'pluck':
        data = sum(np.sin(phase * h) / h**2 for h in range(1, 6)) * np.exp(-p * 8)
        data += soft_noise * .6 * np.exp(-p * 15)
    elif family == 'air':
        data = (soft_noise * 2 + .12 * np.sin(phase)) * np.sin(np.pi * p)**1.6
    elif family == 'rumble':
        data = (.7 * np.sin(phase) + soft_noise * 2) * np.exp(-p * 5)
    elif family == 'bell':
        data = (np.sin(phase) + .25 * np.sin(phase * 2.76)) * np.exp(-p * 6)
    else:
        data = np.zeros(len(t))
        for index, ratio in enumerate((1, 1.25, 1.5)):
            local = np.maximum(0, t - index * .08)
            data += np.sin(2 * np.pi * start * ratio * local) * np.minimum(1, local / .008) * np.exp(-local * 7)
    data -= data.mean()
    fade = min(480, len(t) // 4)
    data[:fade] *= np.linspace(0, 1, fade)
    data[-fade:] *= np.linspace(1, 0, fade)
    data *= 10 ** (ceiling / 20) / max(1.e-12, np.abs(data).max())
    path = OUT / ('S_' + key + '.wav')
    with wave.open(str(path), 'wb') as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(RATE)
        stream.writeframes(np.rint(data * 32767).astype('<i2').tobytes())
    rows.append(dict(key=key, file=str(path.relative_to(ROOT)).replace('\\', '/'),
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), family=family,
        seconds=duration, startHz=start, endHz=end, peakDbFS=ceiling,
        minimumInterval=gap, seed=seed, sampleRate=RATE, channels=1,
        author='Little King project / Codex procedural sound design',
        license='project-original; no third-party samples',
        creationBrief='Short soft storybook foley and magical cues; deterministic filtered noise, plucked harmonics and chimes. Parameters and synthesis preserved in Scripts/PrepareWorldAudio.py.'))
(OUT / 'manifest.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'WORLD_AUDIO_OK {len(rows)} mono PCM16 files at {RATE} Hz')
