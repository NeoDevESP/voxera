# VOXERA v0.5.1 — revisión de estabilidad

Esta entrega contiene **código fuente**, no un VST3 compilado ni un instalador.
Consulta `CAMBIOS_V0_5_1.md` para cambios, pruebas y límites.

Windows: instala Visual Studio 2022 con desarrollo de escritorio C++, CMake >= 3.22 y Git. Ejecuta `BUILD_WINDOWS.ps1` desde PowerShell. La primera compilación necesita conexión para descargar JUCE y Rubber Band. El script detiene el proceso si falla un comando.

Se conservan los identificadores de parámetros y del plugin. El sonido de la saturación puede cambiar por la corrección del alineamiento temporal. Conserva renders de proyectos antiguos antes de sustituir la versión instalada.

---

## Documentación heredada de v0.5

# VOXERA v0.5 — Intelligent Voice Engine

V0.5 is the first version where VOXERA starts reacting to the singer,
not only to fixed parameter values.

## New in v0.5

### Pitch intelligence
- rolling F0 history
- octave-error guard
- transition detector
- vibrato activity estimator
- confidence-aware correction protection
- performance-preservation factor
- NATURAL protects transitions/vibrato most
- HARD protects them least

### Voice Profile
Use **Analyze Voice (8s)** while singing a representative section.

VOXERA measures:
- average RMS
- crest factor
- low-mid density
- presence density
- sibilance density
- brightness
- pitch confidence
- pitch range

### Auto Voice
After a completed analysis, turn **Auto Voice** up from 0–100.

It automatically biases:
- CLEAN
- DE-ESS
- BODY
- PRESENCE
- AIR
- compressor threshold

The normal controls still remain active; Auto Voice works as an adaptive
layer on top rather than replacing your settings.

## Recommended workflow

1. Set Key and Scale correctly.
2. Sing 8 seconds containing:
   - a quiet phrase
   - a loud phrase
   - S / SH consonants
   - a sustained note
3. Enable `Analyze Voice (8s)`.
4. Sing the section.
5. Set `Auto Voice` to 30–50%.
6. Fine-tune manually.

Do not start at 100%. The profile is meant to assist, not erase the singer.

## Full chain

```text
INPUT
  ↓
VOICE PROFILE CAPTURE ────────────────┐
  ↓                                   │
AUTO GAIN                             │
  ↓                                   │
YIN F0                                │
  ↓                                   │
OCTAVE GUARD                          │
  ↓                                   │
TRANSITION / VIBRATO ANALYSIS         │
  ↓                                   │
KEY + SCALE                           │
  ↓                                   │
ADAPTIVE RETUNE                       │
  ↓                                   │
MIX PITCH SHIFTER                     │
  ↓                                   │
SPECTRAL ENGINE ◄──── AUTO VOICE ◄────┘
  ↓
COMPRESSOR
  ↓
SATURATION
  ↓
SPATIAL ENGINE
  ↓
OUTPUT
```

## Important

The current pitch-resynthesis path is still the MIX/high-latency engine
based on Rubber Band LiveShifter.

V0.6 should focus on the custom low-latency TRACK engine and the final
custom GUI.
