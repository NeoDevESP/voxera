# VOXERA voice profile

## Captured features

### RMS
Average energy over the capture window.

### Crest factor
Peak dB - RMS dB.

Large crest factor means more dynamic/transient variation and can justify
a little more compression.

### Low-mid density
Approximate low-band energy relative to total vocal energy.

Used conservatively to bias BODY and CLEAN.

### Sibilance
Upper-band energy ratio.

Used to bias DE-ESS.

### Brightness
Upper spectral-energy proxy.

Used to avoid adding AIR to an already bright vocal.

### Pitch confidence
Mean confidence of usable voiced frames.

### Pitch range
Max/min voiced F0 converted to semitones.

## Auto Voice philosophy

Auto Voice does NOT write permanent parameter values.

It computes an adaptive layer in real time:

manual control
+
voice-profile bias
=
effective DSP value

That means you can always back Auto Voice down to zero and return to the
exact manual sound.
