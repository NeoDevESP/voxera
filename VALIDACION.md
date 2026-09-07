# Validación VOXERA 0.8.0

## Ejecutado y aprobado en Linux

- Compilación CMake Release de los targets VOXERA_VST3 y VOXERA_Standalone con JUCE 9.0.1 y Rubber Band 4.0.0. El standalone se ha compilado; no se ha comprobado con un dispositivo de audio real.
- VST3 0.8.0 cargado por pluginval 1.0.4, strictness 5, semilla 12345: **SUCCESS**. Pruebas de procesamiento, estado, automatización, parámetros y buses incluidas. GUI omitida con `--skip-gui-tests`; validador oficial Steinberg no configurado.
- Pruebas DSP e integración compiladas y ejecutadas mediante GCC, enlazadas con las dependencias reales. Los CHECK permanecen activos en Release. Pruebas nativas CMake AlgorithmTests y RealtimeTests también ejecutadas. No se completó una pasada CTest del conjunto: se detuvo la compilación duplicada de módulos para tests una vez verificados DSP, integración y VST3 mediante estas rutas.
- Smart EQ: identidad inicial a 0%, atenuación, límite agregado, recuperación en silencio, reset, simetría estéreo, contrafase y equivalencia exacta entre bloques 1/17/64/257/1024 y bloque completo a 44.1/48/96 kHz.
- Cadena con Smart EQ activa: mono/estéreo, 0/1/64/257/2048/17/4096 muestras, salidas finitas; ramas dry y bypass alineadas con la latencia.
- Estado: guardado/restauración de Smart EQ y perfil vocal; carga de estado sin parámetros Smart EQ en una instancia ya usada restablece sus valores iniciales.
- Editor nativo: creación, pestañas, vínculos de Tune y Smart EQ con parámetros, render y reducción a 960×672. Las capturas son del editor real renderizado sin pantalla, no pruebas de interacción dentro de un DAW.

## Medición Smart EQ
Tono senoidal de 1 kHz, dos segundos; Amount 100%, Budget 3 dB, Response 100 ms. Atenuación RMS en el segundo final:

| Frecuencia de muestreo | Atenuación |
|---|---:|
| 44.1 kHz | -0.863615 dB |
| 48 kHz | -0.859096 dB |
| 96 kHz | -0.844314 dB |

Estas mediciones verifican funcionamiento y límites, no calidad vocal perceptual. La curva de detección puede pedir menos corrección que el margen máximo permitido.

## Modo Low Latency (tracking)

Medido, no estimado: la cadena completa reporta 2827 muestras a 44,1 kHz, 2845 a 48 kHz y 5109 a 96 kHz. De esas, **2623 son de Rubber Band LiveShifter** — el 92 %. Con `lowLatency` activo el shifter sale de la ruta de señal y la latencia cae a 204 / 222 / 438 muestras, es decir **4,6 ms en las tres frecuencias**, verificado en `integration_tests`.

Las pruebas comprueban además que el host es informado del cambio, que un impulso sale exactamente en la muestra reportada en modo tracking (si el retardo de compensación y la cadena discreparan se oiría filtrado en peine, no retardo), y que volver al modo completo en mitad del flujo restaura la cifra original.

El detector de tono sigue alimentándose en modo tracking, así que el editor continúa mostrando la nota cantada; lo único que se pierde es la corrección. No se ha medido con un cantante monitorizándose: la cifra es la latencia declarada por el plugin, a la que hay que sumar la ida y vuelta de la interfaz de audio.

## Latencia de la cadena observada
2629 muestras a 44.1/48 kHz; 4677 a 96 kHz para la cadena original. Smart EQ, exciter, óptico, soft clip y character no añaden retardo. Se suman dos ventanas de look-ahead, ambas constantes e incluidas en `setLatencySamples`: el limitador (1,5 ms) y el gate (3 ms), es decir 216 muestras más a 48 kHz y 432 a 96 kHz. No se ha reducido la latencia de Rubber Band ni se ha validado monitorización vocal de baja latencia.

## Ejecutado y aprobado en Windows x64

Compilado con MSVC 19.44 (VS Build Tools 2022) y CMake 3.31.7 sobre Windows 11:

- Targets VOXERA_VST3 y VOXERA_Standalone compilados en Release sin errores. Requirió `NOMINMAX`: las macros `min`/`max` de los cabeceras de Windows rompían `std::min`/`std::max` dentro de Rubber Band.
- CTest completo: 5/5 aprobados (`pitch_control`, `realtime_utilities`, `detector_benchmark`, `dsp`, `integration`).
- pluginval 1.0.4, strictness 5, semilla 12345, `--skip-gui-tests`: **SUCCESS**. Incluye automatización a 48/96 kHz con bloques de 64 a 1024, parámetros automatizables, buses y restauración de layout.
- Buses declarados y aceptados por pluginval: entradas Mono/Estéreo, salidas Mono/Estéreo, incluida la ruta mono→estéreo.
- Cargado en FL Studio 2025 como VST3.

### Etapas nuevas verificadas

- **Limitador**: techo nunca superado con entrada a +12 dBFS tras silencio, a 44,1/48/96 kHz; retardo puro exacto con la etapa desactivada; equivalencia exacta entre bloques 1/17/64/257/1024 y búfer completo.
- **Exciter**: identidad exacta con cantidad 0. Con entrada senoidal de 4 kHz el 2º armónico mide 0,0397 y el 3º entre 8,7e-10 y 3,0e-07 según frecuencia de muestreo — **como máximo el 0,0008 % del 2º**, lo que confirma que la etapa es exactamente de segundo orden y no produce aliasing. Medido con Goertzel sobre ventana Hann; un filtro de medida a frecuencia fija no sirve aquí porque su pendiente cambia con la frecuencia de muestreo por warping bilineal.
- **Punch**: identidad exacta con cantidad 0; incrementa nivel en paralelo manteniendo salidas finitas.
- **Auto Mix**: la función de decisión reacciona en el sentido correcto ante turbidez, falta de agudos, sibilancia y rango dinámico, y todos sus valores caen dentro del rango declarado de cada parámetro.
- **Gate**: con una nota seguida de una consonante final a -44 dBFS y después silencio, la consonante se conserva (pico 0,006) y el silencio queda exactamente en 0, a 44,1/48/96 kHz. Retardo puro exacto con la etapa desactivada.
- **Soft clip**: identidad exacta con cantidad 0. A tope reduce el factor de cresta de 1,414 a 1,071 sin superar fondo de escala. El 2º armónico mide 8,2e-06 frente a 0,401 del 3º — **el 0,002 %** — lo que confirma que la curva es impar como se afirma. No se ha medido la reducción de aliasing por ADAA frente a una evaluación puntual; la afirmación se apoya en el método, no en una medida.
- **Optical**: 1,0 dB de reducción con el control a 0 y 16,4 dB a tope, salidas finitas.
- **Character**: Neutral es idéntico muestra a muestra. Bright sube 6 kHz y baja 150 Hz, Dark hace lo contrario, Demon pesa más que Neutral y Robot queda por debajo de Dark en graves y de Neutral en agudos. Todos los desplazamientos de formante quedan dentro de ±12 semitonos.

## Auto Mix

`voxera::decide` en `Source/DSP/AutoMix.h` es una función pura del perfil de voz a un ajuste de cadena. No es una red neuronal: es un mapeo de reglas expresado como desviaciones respecto de un balance de bandas de referencia, porque los valores absolutos dependen del cantante, el micrófono y la ganancia mientras que las proporciones entre bandas no. Está aislada precisamente para poder sustituirse por inferencia de un modelo entrenado sin tocar nada más de la cadena; lo que falta para eso no es código sino datos: un corpus de voces emparejadas con ajustes juzgados correctos por un mezclador.

## Pendiente
Generación/ejecución/desinstalación del Setup Inno Setup y firma del binario. No se ha probado audio de micrófono ni escucha A/B sobre canto: la ruta mono→estéreo y el limitador están verificados por pluginval y por las regresiones, no por escucha. No se garantiza CPU máxima ni ausencia de clicks con automatización extrema. El limitador acota el pico de muestra al techo configurado; no es un limitador true-peak con sobremuestreo, así que no acota picos inter-muestra.

`Tests/Results/` contiene evidencia actual; `Docs/History/` contiene evidencia anterior. Las capturas SMART EQ y small muestran una señal sintética de 1 kHz como demostración de las lecturas, con ajustes distintos de las capturas de otras pestañas.
