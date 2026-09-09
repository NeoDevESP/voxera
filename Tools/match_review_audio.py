"""Produce local level-matched pairs from the review renders (requires NumPy)."""
from pathlib import Path
import json, wave
import numpy as np
from audio_review import read_wav, measure

root=Path('build/audio-review')
rows=[]
for stem, variant in [('take1','warm'),('take2','modern'),('take3','automix')]:
    rate, dry=read_wav(root/f'{stem}-dry.wav'); other_rate,wet=read_wav(root/f'{stem}-{variant}.wav')
    assert rate==other_rate and dry.shape==wet.shape
    a,b=measure(dry,rate),measure(wet,rate)
    target=min(-23.0,-1.0-a['crest_db'],-1.0-b['crest_db'])
    row=dict(take=stem,variant=variant,input=a,output=b,target_rms=target)
    for label,x in [('dry',dry),(variant,wet)]:
        rms=float(np.sqrt(np.mean(x*x))); gain=10**(target/20)/max(rms,1e-12)
        y=x*gain; q=np.clip(np.rint(y*8388608),-8388608,8388607).astype(np.int32).reshape(-1)
        raw=np.column_stack((q&255,(q>>8)&255,(q>>16)&255)).astype(np.uint8).tobytes()
        with wave.open(str(root/f'{stem}-{label}-matched.wav'),'wb') as f:
            f.setnchannels(y.shape[1]); f.setsampwidth(3); f.setframerate(rate); f.writeframes(raw)
        row[label+'_matched']=measure(y,rate)
    assert abs(row['dry_matched']['rms_db']-row[variant+'_matched']['rms_db'])<=0.01
    rows.append(row)
(root/'measurements.json').write_text(json.dumps(rows,indent=2))
print(json.dumps(rows,indent=2))
