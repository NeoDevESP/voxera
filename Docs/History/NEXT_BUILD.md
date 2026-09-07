# VOXERA v0.6

Two priorities only.

## 1. TRACK ENGINE

Replace the high-latency MIX shifter when recording.

Target architecture:

```text
causal F0
  ↓
period estimator
  ↓
pitch marks
  ↓
short PSOLA grains
  ↓
formant envelope compensation
  ↓
output
```

Targets:
- < 15 ms additional latency if achievable
- no heap allocation
- stable 44.1 / 48 / 96 kHz
- NATURAL / MODERN / HARD
- mono vocal first

## 2. FINAL GUI

Implement the already approved VOXERA visual direction:
- brushed-metal hardware frame
- dark magenta display
- large VOXERA logo
- waveform / pitch trace
- TUNE / TONE / AIR / SPACE / MIX macros
- mascot
- page tabs: VOCALS / FX / PRESETS
- pitch display: detected note → target note
- analyze voice control

Do not add more DSP modules until both are working.
