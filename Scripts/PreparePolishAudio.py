"""Reproducible PCM mastering of Music3 originals. Never modifies source audio."""
from pathlib import Path
import hashlib,json,wave
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'ArtSource/StorybookV1/Polish/Music'
report=[]
for name in ('Home','Expedition','Battle','Boss'):
    path=SRC/'Originals'/(name+'.wav')
    receipt=json.loads(path.with_suffix('.json').read_text(encoding='utf-8'))
    assert hashlib.sha256(path.read_bytes()).hexdigest()==receipt['sha256']
    with wave.open(str(path),'rb') as f:
        rate=f.getframerate();channels=f.getnchannels()
        assert f.getsampwidth()==2 and channels==2
        data=np.frombuffer(f.readframes(f.getnframes()),dtype='<i2').reshape(-1,channels).astype(np.float64)/32768
    # Remove only near-silent edges; do not infer that the requested duration was delivered.
    window=int(rate*.05);energy=np.array([np.sqrt(np.mean(data[i:i+window]**2)) for i in range(0,len(data),window)])
    audible=np.flatnonzero(energy>10**(-48/20))
    assert len(audible)>0,name
    first=max(0,int(audible[0]*window)-window);last=min(len(data),int((audible[-1]+2)*window))
    data=data[first:last];data-=data.mean(axis=0)
    # Rotate the head to the end and overlap tail/head for a continuous, non-silent seam.
    n=min(int(rate*1.5),len(data)//6)
    t=np.linspace(0,1,n)[:,None];blend=.5-.5*np.cos(np.pi*t)
    loop=np.concatenate((data[n:-n],data[-n:]*(1-blend)+data[:n]*blend))
    rms=float(np.sqrt(np.mean(loop**2)));peak=float(np.max(np.abs(loop)))
    gain=min(10**(-22/20)/max(rms,1e-9),10**(-3/20)/max(peak,1e-9))
    loop*=gain;pcm=np.clip(np.rint(loop*32767),-32768,32767).astype('<i2')
    output=SRC/('M_'+name+'.wav')
    with wave.open(str(output),'wb') as f:
        f.setnchannels(2);f.setsampwidth(2);f.setframerate(rate);f.writeframes(pcm.tobytes())
    report.append(dict(id=name,file=output.relative_to(ROOT).as_posix(),source=path.relative_to(ROOT).as_posix(),
        sourceSha256=receipt['sha256'],sha256=hashlib.sha256(output.read_bytes()).hexdigest(),sampleRate=rate,
        channels=2,bits=16,originalSeconds=receipt['actualSeconds'],seconds=len(loop)/rate,
        trimStartSeconds=first/rate,trimEndSeconds=(receipt['frames']-last)/rate,crossfadeSeconds=n/rate,
        masteringGainDb=20*np.log10(gain),rmsDb=20*np.log10(np.sqrt(np.mean(loop**2))),
        peakDb=20*np.log10(np.max(np.abs(loop))),seamMaxDelta=float(np.max(np.abs(loop[-1]-loop[0]))),
        processing='DC removal; -48dB edge trim; 1.5s raised-cosine tail/head overlap and head rotation; target -22dB RMS, peak <= -3dBFS; 16-bit PCM stereo'))
(SRC/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
