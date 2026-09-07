# VOXERA v0.5 architecture

```text
                       DRY VOCAL
                           │
             ┌─────────────┴──────────────┐
             │                            │
             ▼                            ▼
       VOICE PROFILE                    AUTO GAIN
             │                            │
  RMS / crest / bands                     ▼
  pitch range/confidence             YIN / CMNDF
             │                            │
             │                       OCTAVE GUARD
             │                            │
             │                   TRANSITION/VIBRATO
             │                            │
             │                      KEY / SCALE
             │                            │
             │                     RETUNE CURVE
             │                            │
             │                      MIX SHIFTER
             │                            │
             └──────► AUTO VOICE ─────────┤
                                          ▼
                                  SPECTRAL ENGINE
                                          │
                                      DYNAMICS
                                          │
                                      SATURATION
                                          │
                                    SPATIAL ENGINE
                                          │
                                        OUTPUT
```

Auto Voice is intentionally a bias layer, not a destructive preset system.
