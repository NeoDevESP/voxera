import wave, math, os, sys
def air(path, lo=4000.0, seconds=8):
    w=wave.open(path,'rb'); sr=w.getframerate(); ch=w.getnchannels(); sw=w.getsampwidth()
    n=min(w.getnframes(), sr*seconds); raw=w.readframes(n); w.close()
    step=sw*ch; k=math.exp(-2*math.pi*lo/sr); st=[0.0]*4; hi=0.0; tot=0.0
    for i in range(n):
        v=int.from_bytes(raw[i*step:i*step+sw],'little',signed=True)/float(1<<(8*sw-1))
        tot+=v*v
        for j in range(4):
            st[j]=(1-k)*v+k*st[j]; v-=st[j]
        hi+=v*v
    return 10*math.log10(max(hi,1e-20))-10*math.log10(max(tot,1e-20))
print(f"{air(sys.argv[1]):.2f}")
