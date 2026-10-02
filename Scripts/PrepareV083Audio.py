"""Deterministic, sample-free storybook spell cues. Generates PCM16 WAVs, never image pixels."""
from pathlib import Path
import hashlib
import json
import wave
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'ArtSource/StorybookV1/V083/Audio'
RATE = 48000
SPECS = {
    'FreezeSpell': (.72, -25, .30, 'soft ice crystals followed by a pale airy shimmer'),
    'HolySummon': (1.24, -22, .40, 'three warm ascending chimes and a gentle opening air swell'),
    'BlackCloud': (.86, -26, .30, 'muted velvety low air, restrained obscuring mist'),
    'Lightning': (.66, -23, .25, 'brief dry electric crackle with soft short thunder, no explosive boom'),
    'Hurricane': (1.35, -25, .40, 'rounded wind rising and falling, subtle leafy rustle'),
    'DivineBlessing': (1.62, -22, .60, 'warm major harmony, five gentle rising healing chimes'),
}

def chimes(t, notes, spacing, release):
    data = np.zeros_like(t)
    for index, hz in enumerate(notes):
        local = t - spacing * index
        active = local >= 0
        local = np.maximum(0, local)
        envelope = np.minimum(1, local / .008) * np.exp(-local * release) * active
        data += (np.sin(2*np.pi*hz*local) + .16*np.sin(2*np.pi*hz*2.01*local)) * envelope
    return data

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    for key, (seconds, ceiling, gap, brief) in SPECS.items():
        seed = int.from_bytes(hashlib.sha256(('V083:'+key).encode()).digest()[:4], 'little')
        rng = np.random.default_rng(seed)
        t = np.arange(round(seconds*RATE))/RATE
        progress = t/seconds
        noise = rng.standard_normal(len(t))
        air = np.convolve(noise, np.ones(80)/80, mode='same')
        low = np.convolve(noise, np.ones(240)/240, mode='same')
        swell = np.sin(np.pi*progress)**1.6
        if key == 'FreezeSpell':
            data = .35*chimes(t, (1046.5, 1396.9, 1568), .055, 8) + air*1.3*swell
        elif key == 'HolySummon':
            data = chimes(t, (261.63, 329.63, 392), .14, 4.1) + air*.8*swell
        elif key == 'BlackCloud':
            data = low*2.4*swell + .08*np.sin(2*np.pi*(90*t+14*t*t))*swell
        elif key == 'Lightning':
            crackle = (noise-np.convolve(noise, np.ones(8)/8, mode='same'))*.13*np.exp(-t*22)
            data = crackle + low*2*np.exp(-t*7) + .12*np.sin(2*np.pi*(100*t-35*t*t))*np.exp(-t*9)
        elif key == 'Hurricane':
            data = air*(2+.25*np.sin(2*np.pi*5*t))*swell + low*.7*swell
        else:
            data = chimes(t, (261.63, 329.63, 392, 523.25, 659.25), .105, 3.6) + air*.45*swell
        data -= data.mean()
        fade = min(960, len(data)//4)
        data[:fade] *= np.linspace(0, 1, fade)
        data[-fade:] *= np.linspace(1, 0, fade)
        data *= 10**(ceiling/20)/max(1.e-12, np.abs(data).max())
        samples = np.rint(data*32767).astype('<i2')
        path = OUT/('S_'+key+'.wav')
        with wave.open(str(path), 'wb') as stream:
            stream.setnchannels(1)
            stream.setsampwidth(2)
            stream.setframerate(RATE)
            stream.writeframes(samples.tobytes())
        rows.append(dict(key=key, file=path.relative_to(ROOT).as_posix(),
            sha256=hashlib.sha256(path.read_bytes()).hexdigest(), seconds=seconds,
            sampleRate=RATE, channels=1, bits=16, peakDbFS=ceiling,
            rmsDbFS=float(20*np.log10(max(1.e-12, np.sqrt(np.mean((samples.astype(float)/32767)**2))))),
            minimumInterval=gap, seed=seed, creationBrief=brief,
            generator='Scripts/PrepareV083Audio.py; NumPy synthesis + Python wave',
            author='Little King project / Codex procedural sound design',
            license='project-original; no third-party samples', date='2026-10-01'))
    (OUT/'manifest.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('V083_AUDIO_OK', len(rows), 'mono PCM16 cues at', RATE, 'Hz')

if __name__ == '__main__':
    main()
