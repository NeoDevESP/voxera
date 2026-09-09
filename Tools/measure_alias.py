import wave, math, sys, cmath
# Energia por debajo de 12 kHz en una senal que solo contenia 15 kHz.
# Todo lo que aparezca ahi es aliasing: armonicos reflejados por Nyquist.
def analyse(path):
    w = wave.open(path,'rb'); sr=w.getframerate(); ch=w.getnchannels(); sw=w.getsampwidth()
    n = min(w.getnframes(), 32768); raw = w.readframes(n); w.close()
    step = sw*ch
    x = [int.from_bytes(raw[i*step:i*step+sw],'little',signed=True)/float(1<<(8*sw-1)) for i in range(n)]
    # Goertzel en una rejilla gruesa, que basta para separar 15 kHz de lo reflejado
    def energy(f0):
        k = 2*math.cos(2*math.pi*f0/sr); s1=s2=0.0
        for v in x:
            s = v + k*s1 - s2; s2 = s1; s1 = s
        return s1*s1 + s2*s2 - k*s1*s2
    low = sum(energy(f) for f in range(500, 12000, 500))
    tone = energy(15000)
    return 10*math.log10(max(low,1e-20)/max(tone,1e-20))
print(f"{analyse(sys.argv[1]):.1f}")
