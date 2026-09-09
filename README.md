# VOXERA 1.5

Procesador vocal en C++/JUCE para VST3, Audio Unit y aplicación autónoma. Windows x64 y macOS universal (Apple Silicon + Intel).

La idea que lo separa de una cadena vocal al uso: **se adapta a la voz que tiene delante** en lugar de aplicar ajustes universales. El detector de tono y el perfilador de voz que ya lleva alimentan tanto la corrección de EQ como el ajuste automático de toda la cadena.

## Home studio

Interfaz organizada alrededor de Auto Mix, con un selector de sonidos iniciales, cinco mandos principales y p�ginas de voz, efectos, din�mica, Smart EQ, Chop y ajustes avanzados. Consulta la [gu�a de esta revisi�n](Docs/HOME_STUDIO.md).

## La cadena

El orden importa y cada etapa está donde está por una razón concreta; el código lo explica caso por caso.

| Etapa | Qué hace |
|---|---|
| **Gate** | Puerta con look-ahead y *hold*. Va primera, para que el ruido de sala no se afine ni llegue a la reverb. |
| **Auto Gain** | Nivelado hacia un objetivo. |
| **Pitch** | Corrección con preservación de formantes (Rubber Band LiveShifter). |
| **Vocal Lock** | EQ correctiva que **sigue el registro del cantante**: el paso-alto se sitúa bajo su nota más grave y el corte de turbidez cerca del segundo armónico de su tono mediano. |
| **Spectral** | Correcci�n de resonancias; cuerpo, presencia y aire se aplican despu�s de la din�mica. |
| **Smart EQ** | Cinco bandas correctivas que miden la prominencia sobre el **espectro real** que calcula el motor anterior. |
| **Character** | Timbre por formante y basculación: Neutral, Bright, Dark, Ghost, Robot, Demon. |
| **Compresor → Optical → Density → Punch** | Cuatro etapas dinámicas en distintas escalas de tiempo: picos, sílabas, suelo y paralelo. |
| **Insert VST3** | Efecto externo despu�s de la din�mica, con estado y latencia propios. |
| **Neural** | Ejecuta una captura NAM o un modelo RTNeural donde iría un previo. |
| **Saturación** | `tanh` con sobremuestreo 4×, más un sesgo que genera armónicos **pares**. |
| **Exciter** | Genera aire por elevación al cuadrado de la banda 3,5–8 kHz. |
| **Crush · Modulation · Chop** | Destrucción, flanger/phaser y puerta rítmica anclada al transporte. |
| **Spatial** | Doubler de seis voces, delay sincronizado y reverb con Body/Air, todo con *ducking*. |
| **Glue → Soft Clip → Limiter** | Cohesión, recorte suave con ADAA y limitador brickwall. |

## Auto Mix

Escucha ocho segundos y configura la cadena entera. **Y explica por qué**: lee de vuelta lo que oyó —turbio, hueco, apagado, áspero, dinámico— en las mismas palabras que usaría un mezclador, para que puedas estar en desacuerdo con la decisión en lugar de adivinar qué mando deshacer.

`voxera::decide` en `Source/DSP/AutoMix.h` es una función pura del perfil a los ajustes. No es una red neuronal: es un mapeo de reglas, aislado precisamente para poder sustituirse por inferencia de un modelo entrenado sin tocar nada más. Lo que falta para eso no es código sino datos.

**MIX OPTIONS** ajusta intensidad y bloqueos de afinaci�n, EQ, din�mica y color. **A/B** compara con la configuraci�n anterior y **UNDO MIX** deshace el �ltimo Auto Mix. **MATCH LEVEL** iguala RMS de manera aproximada y limitada; deja unos segundos para que se estabilice.

La captura exige al menos dos segundos de se�al �til dentro de los ocho de escucha y rechaza una proporci�n de muestras recortadas del 1 % o superior. El informe distingue la propuesta de lo aplicado.

## Compresores y Sauce

En DYNAMICS puedes elegir **Clean, FET, VCA, Vari-Mu u Opto** y mover **COMP SAUCE**, umbral, ratio, tiempos, Optical y Density. Son dise�os propios inspirados en familias anal�gicas. La mezcla paralela y el filtro del detector est�n en ADVANCED. Consulta la [ingenier�a de sonido y referencias](Docs/SOUND_ENGINEERING.md).

Auto Mix conserva los par�metros externos hasta que elijas su control y direcci�n desde INSERT PLUGIN. Una vez asignado, respeta la intensidad y el bloqueo de din�mica, y sus cambios entran en A/B y Undo.

Los presets conservan tonalidad, escala, motor de afinaci�n, ganancias de entrada/salida, Low Latency, bypass, referencias y opciones de Auto Mix. El modelo neuronal permanece cargado, pero su mezcla vuelve a cero.

## Modo Low Latency

Saca el afinador de la ruta de señal. A 48 kHz la latencia depende del motor seleccionado: PSOLA es el predeterminado y Rubber Band es alternativo; Low Latency conserva solo los retardos de las demás etapas. El editor muestra la cifra activa. El detector de tono continúa funcionando, pero no se aplica corrección de afinación.

## Etapa neuronal

Carga capturas `.nam` de [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) y modelos JSON de RTNeural. Aunque NAM apunte a amplificadores de guitarra, el mismo proceso de captura sirve para un previo de micrófono o un canal de consola, que es lo que interesa a una voz.

Admite NAM LSTM y WaveNet con hasta 12 canales por capa, además de modelos RTNeural de una entrada y una salida. Las capturas más anchas se rechazan por coste. El consumo depende del modelo y de la sesión; no se garantiza un número fijo de instancias.

La captura se ejecuta **a la frecuencia a la que fue entrenada**, remuestreando alrededor si la sesión va a otra. El estado de una red recurrente avanza por muestra y no por segundo, así que sin eso todas sus constantes de tiempo se estirarían y la captura dejaría de ser una captura.

> **No se distribuye ninguna captura.** La de otra persona lleva sus propios términos y el equipo que modela lleva un nombre que no es nuestro. Capturar el propio hardware evita ambas preguntas.

## Compilar

**Windows** — necesita CMake, Git y las herramientas de C++ de Visual Studio:

```
powershell -ExecutionPolicy Bypass -File BUILD_WINDOWS.ps1
```

**macOS** — necesita Xcode y CMake:

```
chmod +x BUILD_MACOS.sh && ./BUILD_MACOS.sh --install
```

Para desarrollo de interfaz, `-DVOXERA_UI_INSPECTOR=ON` añade el inspector de componentes (tecla **I**). Está desactivado por defecto y no debe entrar en un binario distribuido.

## Documentación

- `VALIDACION.md` — qué está medido y qué no.
- `LICENSE_NOTES.md` — licencias de las dependencias. **Léelo antes de distribuir**: JUCE y Rubber Band son AGPL/GPL o comercial.
- `SMART_EQ.md`, `ALGORITMO.md`, `LIBRERIAS.md` — diseño de cada motor.
- `Docs/History/` — versiones anteriores.
