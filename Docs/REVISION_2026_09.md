# Revisión VOXERA — septiembre de 2026

## Qué cambia

### Estabilidad y sesiones

Voice Match mantiene su estado de trabajo en el hilo de audio. La interfaz y el guardado reciben copias mediante un intercambio acotado; las órdenes de restaurar y olvidar se consumen en audio. La duración se mide por muestras de nuevos espectros, no por llamadas del DAW. La adaptación y el movimiento de filtros dependen del tiempo transcurrido.

Al cargar un estado sin modelo o sin referencia se elimina el anterior. Guardar después de olvidar una referencia o quitar un modelo no resucita propiedades antiguas. Si falta un archivo neuronal, el informe indica que hay que volver a seleccionarlo. Los menús asíncronos del editor comprueban que la ventana sigue existiendo antes de usarla.

La finalización de Auto Mix está separada de las notificaciones de latencia: cambiar Low Latency durante el análisis ya no aplica un perfil incompleto. La publicación del perfil debe terminar antes de aplicar ajustes. La ruta seca cambia de retardo antes de construir el bloque en el que cambia el motor.

### Control del resultado

- A/B del último Auto Mix y deshacer conjunto de sus parámetros, sin depender del historial del DAW.
- Intensidad de 0–100 % en MORE; accesos 25/50/75/100 % en MIX OPTIONS.
- Bloqueos independientes de afinación, EQ, dinámica y color.
- Match Level compara RMS con la entrada retardada. No es LUFS ni garantiza igualdad perceptual. Puede atenuar hasta 36 dB o aumentar hasta 9 dB; el limitador sigue después.
- El análisis ignora señal bajo su umbral de actividad, informa de segundos útiles y rechaza capturas insuficientes o con demasiadas muestras recortadas.
- Los presets definen su tratamiento completo y conservan el contexto descrito en README. El historial A/B no se guarda en sesiones.

### Ingeniería de sonido

Nuevo Opto y Comp Sauce accesibles desde PRESETS. FET, VCA y Vari-Mu conservan sus identidades anteriores con detector estéreo corregido, memoria temporal de recuperación y eliminación de continua en el color asimétrico. La etapa Optical independiente incorpora memoria de reducción. Se han ajustado explícitamente los compresores de los diez presets.

[Diseño y fuentes primarias](SOUND_ENGINEERING.md).

## Prueba con grabaciones de FL Studio

Se han procesado copias locales de tres archivos de Audio/Recorded y del proyecto VOCALMIXING. Los nombres de carpeta no demuestran por sí solos que las tomas estén libres de procesado previo; no se ha afirmado eso ni se ha modificado el material original. Se desactivó la afinación para aislar dinámica y color.

| Toma y tratamiento | Pico entrada → salida, dBFS | RMS entrada → salida, dBFS | Factor de cresta entrada → salida, dB |
|---|---:|---:|---:|
| 1, Warm, 13,70 s | -10,24 → -0,80 | -33,52 → -20,95 | 23,28 → 20,15 |
| 2, Modern, 12,02 s | -13,44 → -2,80 | -32,91 → -19,87 | 19,47 → 17,07 |
| 3, Auto Mix, 11,86 s | -11,36 → -0,30 | -28,08 → -14,43 | 16,72 → 14,13 |

No hubo muestras no finitas ni muestras a 0 dBFS en los WAV resultantes. En la tercera toma, Auto Mix encontró 7,7 segundos de señal útil. La reducción máxima fue 5,59 dB en el compresor principal, 1,75 dB en Optical y 0,44 dB en el limitador. Son mediciones de estos archivos, no una predicción para cualquier cantante.

Se generaron parejas adicionales con RMS igualado y alineadas por la latencia declarada: -24,28 dBFS para la toma 1 y -23 dBFS para las tomas 2 y 3. La diferencia de RMS entre cada pareja es menor de 0,01 dB. Están en `build/audio-review/`; el JSON recoge las medidas. Los audios del usuario no se incluyen en el paquete redistribuible.

## Uso de las comparaciones

Alterna los archivos `dry-matched` y su pareja `warm-matched`, `modern-matched` o `automix-matched`. Escucha especialmente consonantes, finales de palabra, ruido entre frases y cuánto conserva la voz de su movimiento original. Las medidas anteriores verifican comportamiento técnico; no sustituyen una escucha artística dentro de la mezcla.

## Compatibilidad y límites

Se conserva la identidad de VOXERA 1.5 y los identificadores de los parámetros anteriores; el estado interno pasa a versión 6. Las correcciones de dinámica pueden cambiar el sonido de sesiones existentes: conserva el binario previo si necesitas reproducir una mezcla exactamente.

La compilación local cubre Windows x64. La receta de macOS permanece en el repositorio, pero no se ha ejecutado aquí. No se ha probado instalación/desinstalación ni una sesión completa de FL Studio con el nuevo VST3. El instalador requiere Inno Setup; se prepara un ZIP para instalación manual cuando esa herramienta no está disponible.

Las comprobaciones reproducibles se ejecutan con CMake/CTest y pluginval; sus resultados actuales están en `VALIDACION.md`. Para repetir la prueba con otra toma: `VOXERA_Render entrada.wav --auto-mix --set lowLatency=1 --set autoMixLockPitch=1 --out salida.wav`. Las herramientas Python de medida requieren NumPy y operan localmente.
