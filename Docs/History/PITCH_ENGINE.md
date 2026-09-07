# VOXERA pitch engine technical notes

## F0

At 48 kHz:
- input analysis is averaged/decimated by 4
- effective rate = 12 kHz
- YIN frame = 1024 samples
- hop = 128 samples
- update interval ~= 10.67 ms
- search range = 60–1000 Hz

The CMNDF threshold is 0.15.
Fallback candidates above 0.34 CMNDF are rejected.

## Pitch conversion

```text
midi = 69 + 12 * log2(f / 440)
```

Correction ratio:

```text
ratio = 2 ^ (shiftSemitones / 12)
```

Rubber Band expects target/source frequency ratio.

## Hysteresis

A newly quantised note replaces the currently held target only if it is
closer to the measured MIDI pitch by a margin. This reduces rapid target
flips close to scale-note midpoints.

## Humanize

Humanize is not random pitch wobble.

It reduces correction around small deviations, especially in NATURAL mode,
so intentional micro-movement is less aggressively flattened.

## Known v0.4 limits

1. YIN is monophonic. Feed it one vocal, not a full mix.
2. Breath/fry/noisy consonants can still cause uncertain F0.
3. Rubber Band LiveShifter is a MIX-quality/high-latency engine.
4. Pitch ratio is currently updated at Rubber Band block boundaries.
5. There is no vibrato-separation model yet.
6. The detector is deterministic, not neural.
7. Formant preservation is delegated to Rubber Band in this version.

## What v0.5 should improve

- vibrato estimator
- pitch confidence fusion
- low-latency TRACK shifter
- phrase-aware retune
- note transition detector
- better unvoiced protection
- custom UI showing detected note / target note / correction
