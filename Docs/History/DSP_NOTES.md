# VOXERA v0.3 DSP notes

## Doubler

The current doubler is intentionally not a true fixed-cent pitch shifter.
Slow modulation of fractional delay creates small instantaneous pitch
deviations and decorrelation.

That is useful now because:
- it is low CPU
- it is stable
- it has no analysis latency
- it will later be replaceable by the v0.4 pitch engine

## Fractional interpolation

4-point cubic Hermite interpolation is used for continuously moving delay
reads. This is significantly smoother than integer reads and reduces zipper
artefacts when delay times move.

## Ducking

The vocal envelope uses approximately:
- 5 ms attack
- 140 ms release

The activity curve maps the useful vocal range to 0..1 and can produce up to
~18 dB attenuation on delay/reverb.

## FDN

Delay lengths at 48 kHz are approximately based on:
- 43.7 ms
- 53.1 ms
- 61.7 ms
- 71.9 ms

Feedback for each line is:

g = 10 ^ (-3 * delaySeconds / RT60)

The matrix is the normalised 4x4 Hadamard transform.

## Known limitations

- no convolution early reflections yet
- no modulation inside FDN delay lengths yet
- doubler is micro-modulated delay, not true constant-cent detuning
- no true-peak protection after wet summing yet
- Generic JUCE parameter editor is still used
