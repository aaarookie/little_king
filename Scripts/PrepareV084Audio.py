"""Original deterministic building cues: synthesis only, no external recordings."""
from pathlib import Path
import hashlib
import json
import wave
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'ArtSource/StorybookV1/V084/Audio'
RATE = 48000
SPECS = {
    'WallBuild': (.78, -24, .4, 'Three gentle stone-setting taps, low wood and stone body'),
    'WallRepair': (.72, -24, .3, 'Two light masonry taps followed by warm restoration chime'),
    'WallBreak': (1.12, -23, .25, 'Rounded stone crumble with small falling pebbles, no explosion'),
    'WallReflect': (.38, -28, .12, 'Short soft metallic deflection, restrained bright ring'),
    'SiegeRam': (.85, -22, .25, 'Heavy wooden ram impact with low stone knock and rope creak'),
    'TowerShelter': (1.05, -25, .4, 'Warm glass-and-air ascending shelter chimes'),
    'HealingStation': (1.18, -26, .45, 'Three gentle healing notes, soft airy major harmony'),
}

def notes(t, frequencies, step, decay):
    data = np.zeros_like(t)
    for i, hz in enumerate(frequencies):
        local = np.maximum(t-i*step, 0)
        env = (t >= i*step)*np.minimum(local/.008, 1)*np.exp(-local*decay)
        data += (np.sin(2*np.pi*hz*local)+.12*np.sin(2*np.pi*hz*2.01*local))*env
    return data

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    for key, (duration, peak, gap, brief) in SPECS.items():
        seed = int.from_bytes(hashlib.sha256(('V084:'+key).encode()).digest()[:4], 'little')
        rng = np.random.default_rng(seed)
        t = np.arange(round(duration*RATE))/RATE
        noise = rng.standard_normal(len(t))
        low = np.convolve(noise, np.ones(180)/180, mode='same')
        air = np.convolve(noise, np.ones(35)/35, mode='same')
        data = np.zeros_like(t)
        if key in ('WallBuild', 'WallRepair'):
            for i, start in enumerate((.03, .23, .46) if key == 'WallBuild' else (.03, .2)):
                local = np.maximum(t-start, 0)
                active = t >= start
                data += active*np.exp(-local*24)*(low*1.5+.15*np.sin(2*np.pi*(180+i*30)*local))
            if key == 'WallRepair':
                data += .12*notes(t, (523.25, 659.25), .18, 8)
        elif key == 'WallBreak':
            data = (low*2.5+air*.3)*np.exp(-t*5)
            for i, start in enumerate((.14, .3, .44, .61, .78)):
                local = np.maximum(t-start, 0)
                data += (t >= start)*np.exp(-local*35)*.11*np.sin(2*np.pi*(300+i*80)*local)
        elif key == 'WallReflect':
            data = .5*notes(t, (783.99, 1174.66), .025, 15)+air*.15*np.exp(-t*30)
        elif key == 'SiegeRam':
            data = (low*2.4+air*.3)*np.exp(-t*10)+.4*np.sin(2*np.pi*(100*t-28*t*t))*np.exp(-t*11)
            data += .035*np.sin(2*np.pi*(300*t+60*t*t))*np.sin(np.pi*t/duration)**2
        elif key == 'TowerShelter':
            data = .4*notes(t, (392, 523.25, 659.25), .13, 4.8)+air*.2*np.sin(np.pi*t/duration)**2
        else:
            data = .4*notes(t, (329.63, 392, 523.25), .14, 4.1)+air*.18*np.sin(np.pi*t/duration)**2
        data -= data.mean()
        fade = min(600, len(data)//4)
        data[:fade] *= np.linspace(0, 1, fade)
        data[-fade:] *= np.linspace(1, 0, fade)
        data *= 10**(peak/20)/max(1.e-12, np.abs(data).max())
        samples = np.rint(data*32767).astype('<i2')
        path = OUT/('S_'+key+'.wav')
        with wave.open(str(path), 'wb') as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(RATE)
            wav.writeframes(samples.tobytes())
        rows.append(dict(id=key, file=path.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            seconds=duration, sampleRate=RATE, channels=1, bits=16, peakDbFS=peak, minimumInterval=gap, seed=seed,
            creationBrief=brief, generator='Scripts/PrepareV084Audio.py; NumPy + Python wave',
            author='Little King / Codex procedural sound design', license='project-original; no third-party samples', date='2026-10-08'))
    (OUT/'manifest.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('V084_AUDIO_OK', len(rows))

if __name__ == '__main__':
    main()
