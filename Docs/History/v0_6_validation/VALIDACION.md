# Validación VOXERA 0.6.0

## Resultados comprobados localmente

| Prueba | Resultado / alcance |
|---|---|
| Código del procesador y del editor | Compilado y enlazado con GCC, JUCE 9.0.1 y Rubber Band 4.0.0, ejecutado en Linux |
| Afinación: control | PASS: error de octava aislado, salto sostenido, reinicio entre frases, suavizado independiente de frecuencia de control |
| Utilidades de tiempo real | PASS: rampas estéreo, retardo exacto, aislamiento de canales y reset |
| YIN | 15 combinaciones: 80/110/220/440/880 Hz a 44.1/48/96 kHz. Error máximo observado 2.029 cents, con tonos sinusoidales |
| Saturación | PASS: mono/estéreo, Mix=0 con retardo esperado, valores finitos y bloques mayores que el tamaño anunciado |
| Ganancia automática | PASS: mismo resultado en ambos canales con entradas idénticas |
| Perfil | PASS: silencio no válido, captura con señal válida, copia a otra instancia, intercambio concurrente coherente |
| Pitch a unidad | Pico del impulso en muestra 2623; coincide con retardo declarado a 48 kHz |
| Cadena completa | PASS a 44.1/48/96 kHz, mono/estéreo; bloques 0, 1, 64, 257, 2048, 17 y 4096; salida finita |
| Bypass / Mix=0 | PASS: impulso de salida idéntico a entrada con retardo nominal de 2629 samples a 44.1/48 kHz y 4677 a 96 kHz |
| Estado del plugin | PASS: parámetros y perfil se recuperan en otra instancia; preset de fábrica conserva tonalidad |
| Interfaz | PASS: creación, vínculo del mando Tune con el parámetro, pestañas, captura de imagen y redimensionamiento; revisión visual de capturas |

Los tests pequeños de control y utilidades se ejecutaron con AddressSanitizer y UndefinedBehaviorSanitizer. LeakSanitizer se desactivó por incompatibilidad con el entorno. El test completo de DSP/integración no se ejecutó con sanitizers: no se afirma ausencia general de fugas o condiciones de carrera.

Las capturas en Previews son renderizados del editor JUCE real con señal sintética de prueba. No se ha validado el comportamiento de un DAW mediante interacción con una ventana real. El diálogo nativo de selección de archivos no se ha probado manualmente.

## Qué no demuestran estas pruebas
- No comparan calidad vocal con Auto-Tune, Melodyne ni otros productos comerciales.
- La precisión YIN es una prueba de regresión del detector existente, no una mejora demostrada frente a v0.5.1 ni una garantía con voces ruidosas.
- No hay medición de LUFS/true-peak, benchmark de CPU en el PC del usuario ni evaluación perceptual con sus grabaciones.
- El retardo del shifter a ratios extremos y la duración real de las colas requieren validación adicional.
- Las capturas no reproducen exactamente la fotografía de referencia.

## Reproducir
Windows: BUILD_WINDOWS.ps1 compila con VOXERA_BUILD_TESTS=ON y ejecuta CTest en Release. Las comprobaciones usan CHECK, activo incluso con NDEBUG.

Dependencias principales: JUCE 9.0.1 (e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8), Rubber Band v4.0.0 (1d95888bec3ae0a17c0c4af791810d5a63f6bc35). Los logs de esta entrega se conservan en Tests/Results.

## Compilación VST3 y pluginval
CMake 4.4.3: configuración y generación completadas. Build Release del VST3 Linux completado, incluido moduleinfo.json. CTest: pitch_control y realtime_utilities pasan; las pruebas DSP e Integration se compilaron/ejecutaron con el programa local de integración descrito arriba, no mediante esos dos targets CMake.

pluginval v1.0.4, strictness-level 5, random-seed 12345: SUCCESS. Se comprobaron descubrimiento/carga del VST3, procesamiento, estado, automatización y buses a las frecuencias/tamaños de bloque predeterminados. Se omitieron las pruebas GUI mediante --skip-gui-tests; no se ejecutó el validador adicional de Steinberg. La GUI se revisó mediante nuestras capturas y pruebas de componentes separadas.

El binario compilado es Linux; este ZIP entrega fuentes y no un VST3/instalador para Windows. Ni GitHub Actions ni el VST3 Windows se han ejecutado en esta entrega. No se afirma compatibilidad comprobada con FL Studio/Ableton Windows.
