"""Local-only WAV measurements and excerpts. Never changes source recordings."""
import argparse, json, struct, wave
from pathlib import Path
import numpy as np

def read_wav(path):
    data = Path(path).read_bytes()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE': raise ValueError('Not RIFF/WAVE')
    pos = 12; fmt = raw = None
    while pos + 8 <= len(data):
        kind = data[pos:pos+4]; size = struct.unpack_from('<I', data, pos+4)[0]
        chunk = data[pos+8:pos+8+size]
        if kind == b'fmt ': fmt = chunk
        if kind == b'data': raw = chunk
        pos += 8 + size + (size % 2)
    encoding, channels, rate, _, _, bits = struct.unpack_from('<HHIIHH', fmt)
    if encoding == 65534: encoding = struct.unpack_from('<H', fmt, 24)[0]
    if encoding == 3: x = np.frombuffer(raw, dtype='<f4' if bits == 32 else '<f8').astype(float)
    elif bits == 16: x = np.frombuffer(raw, dtype='<i2').astype(float) / 32768
    elif bits == 24:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1,3).astype(np.int32)
        q = b[:,0] | b[:,1]<<8 | b[:,2]<<16; q = (q ^ 0x800000)-0x800000; x = q / 8388608
    elif bits == 32: x = np.frombuffer(raw, dtype='<i4').astype(float) / 2147483648
    else: raise ValueError(f'Unsupported PCM {bits}')
    return rate, x.reshape(-1, channels)

def measure(x, sr):
    power = np.mean(x*x); peak = np.max(np.abs(x))
    db = lambda v: round(float(20*np.log10(max(v, 1e-12))), 2)
    return dict(seconds=round(len(x)/sr,2), rate=sr, channels=x.shape[1], peak_db=db(peak),
        rms_db=db(np.sqrt(power)), crest_db=round(db(peak)-db(np.sqrt(power)),2),
        dc=float(np.mean(x)), finite=bool(np.isfinite(x).all()), clipped=int(np.sum(np.abs(x)>=1)))

def excerpt(source, destination, seconds=18):
    sr,x=read_wav(source); length=min(len(x),int(sr*seconds)); step=sr
    starts=range(0,max(1,len(x)-length+1),step)
    start=max(starts,key=lambda i:float(np.sum(x[i:i+length]**2)))
    y=x[start:start+length]; Path(destination).parent.mkdir(parents=True,exist_ok=True)
    # PCM 24-bit preserves headroom details in ordinary microphone recordings.
    q=np.clip(np.rint(y*8388608),-8388608,8388607).astype(np.int32).reshape(-1)
    packed=np.column_stack((q&255,(q>>8)&255,(q>>16)&255)).astype(np.uint8).tobytes()
    with wave.open(str(destination),'wb') as f:
        f.setnchannels(y.shape[1]);f.setsampwidth(3);f.setframerate(sr);f.writeframes(packed)
    return dict(source=str(source), start_seconds=start/sr, **measure(y,sr))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('path');p.add_argument('--out');args=p.parse_args()
    src=Path(args.path)
    if args.out: print(json.dumps(excerpt(src,args.out),ensure_ascii=False))
    elif src.is_dir():
        for f in sorted(src.glob('*.wav'),key=lambda v:v.stat().st_mtime,reverse=True)[:12]:
            try: sr,x=read_wav(f);print(json.dumps(dict(file=f.name,**measure(x,sr)),ensure_ascii=False))
            except Exception as e: print(f.name, str(e))
    else: sr,x=read_wav(src); print(json.dumps(measure(x,sr)))
