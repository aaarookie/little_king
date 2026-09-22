"""D-batch Music3 runner. Uses the user's existing installation without changing it.
Resume-safe: a completed WAV plus matching receipt is not regenerated.
"""
from pathlib import Path
import hashlib, json, os, sys, time
ROOT=Path(__file__).resolve().parents[1]
INSTALL=Path(os.environ.get('LK_MUSIC3_INSTALL','E:/minimax_music3/repo'))
OUT=ROOT/'ArtSource/StorybookV1/Polish/Music/Originals'
sys.path.insert(0,str(INSTALL/'app'))
os.environ['HF_HUB_OFFLINE']='1'
os.environ['TRANSFORMERS_OFFLINE']='1'
import inference

COMMON=('Instrumental acoustic fantasy strategy game soundtrack for a warm illustrated small kingdom. '
        'No vocals, no choir, no spoken words. Consistent small ensemble: wooden flute, fingerpicked lute, '
        'harp, warm chamber strings and soft hand percussion. Natural intimate room, rounded transients, '
        'clear midrange, restrained dynamics, no electronic synth, no electric guitar, no cinematic braams. '
        'A repeating musical cycle with balanced phrases, steady pulse, no dramatic intro or final cadence; suitable for a seamless game loop. ')
TRACKS=[
    dict(id='Home',name='晨光小王国',seed=210901,bpm=80,
         detail='80 BPM, C major, gentle pastoral 4/4. Welcoming and safe. Lute arpeggios and harp cradle a short playful wooden flute motif. Very light brushed tambourine, spacious simple strings, no large crescendo.'),
    dict(id='Expedition',name='五境之路',seed=210902,bpm=96,
         detail='96 BPM, D dorian, light walking 4/4. Curious woodland travel, hopeful adventure, quiet sense of discovery. Repeating plucked lute ostinato, answered by short flute phrases, soft pizzicato cello and a small frame drum.'),
    dict(id='Battle',name='举盾前行',seed=210903,bpm=112,
         detail='112 BPM, D minor resolving toward F major, steady 4/4. Determined tactical skirmish, agile and heroic rather than frightening. Lute and pizzicato strings trade compact motifs, low strings pulse, restrained frame drums accent the beat. Keep space for game sound effects.'),
    dict(id='Boss',name='王庭的挑战',seed=210904,bpm=108,
         detail='108 BPM, A minor, resolute 4/4. A formidable storybook king approaches. Deeper cello ostinato, low hand drums, bowed chamber strings and firm wooden flute phrases. Noble tension, controlled weight, no horror drones or loud risers. Keep space for game sound effects.')]

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    specs=[]
    for track in TRACKS:
        spec={**track,'caption':COMMON+track['detail'],'lyrics':'[Instrumental]\n[Instrumental]\n[Instrumental]',
              'audio_duration':48.0,'num_inference_steps':30,'vram_mode':'low',
              'model':'MiniMaxAI/MiniMax-Music3','installation':str(INSTALL),
              'sourceLicense':'MiniMax-Music3 COMMUNITY LICENSE; generated output, not CC0'}
        specs.append(spec)
    (OUT.parent/'prompts.json').write_text(json.dumps(specs,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    for spec in specs:
        wav=OUT/(spec['id']+'.wav'); receipt=OUT/(spec['id']+'.json')
        if wav.exists() and receipt.exists():
            old=json.loads(receipt.read_text(encoding='utf-8'))
            generation_keys=('caption','lyrics','seed','audio_duration','num_inference_steps','vram_mode','model')
            if old.get('sha256')==hashlib.sha256(wav.read_bytes()).hexdigest() and all(old.get(key)==spec[key] for key in generation_keys):
                print('SKIP',spec['id'],flush=True); continue
        start=time.time(); print('START',spec['id'],time.strftime('%Y-%m-%d %H:%M:%S'),flush=True)
        def progress(frac,msg): print(spec['id'],round(time.time()-start,1),frac,msg,flush=True)
        try:
            sr,audio,path,seed=inference.generate(caption=spec['caption'],lyrics=spec['lyrics'],audio_duration=spec['audio_duration'],
                seed=spec['seed'],num_inference_steps=spec['num_inference_steps'],vram_mode=spec['vram_mode'],output_dir=str(OUT),filename=wav.name,progress_cb=progress)
            receipt.write_text(json.dumps({**spec,'sampleRate':sr,'frames':len(audio),'actualSeconds':len(audio)/sr,
                'elapsedSeconds':time.time()-start,'sha256':hashlib.sha256(wav.read_bytes()).hexdigest()},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            print('COMPLETE',spec['id'],flush=True)
        except Exception as exc:
            (OUT/(spec['id']+'.error.txt')).write_text(repr(exc),encoding='utf-8')
            raise
    inference.unload()
    print('POLISH_MUSIC_GENERATION_COMPLETE',flush=True)
if __name__=='__main__': main()
