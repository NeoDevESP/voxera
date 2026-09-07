# Validación de VOXERA 0.6.1

Procesador/editor compilados y enlazados con GCC en Linux usando JUCE 9.0.1 y Rubber Band 4.0.0 reales. Pruebas ejecutadas sobre los fuentes de esta entrega.

| Comprobación | Resultado |
|---|---|
| Bypass espectral y espacial | PASS: primer paso acotado por la rampa de 20 ms, salida seca exacta al terminar, reactivación finita |
| Partición del procesamiento espectral | PASS: diferencia máxima 0 entre bloque de 24.000 muestras y particiones 1/17/64/257/1024, a 48 kHz, con controles constantes |
| Sincronización de delay | PASS: primer impulso a las 144.000 muestras para blanca a 40 BPM, 48 kHz, después de estabilizar el tiempo |
| Detector, saturación y perfil | PASS: regresión DSP heredada, incluyendo mono/estéreo, retardos, señal finita, silencio y transferencia del perfil |
| Cadena completa | PASS: 44.1/48/96 kHz, mono/estéreo y bloques vacíos, pequeños y mayores que el anunciado |
| Bypass global y Mix=0 | PASS: impulso seco con el retardo declarado |
| Cola reportada | PASS: estimación superior a 85 s al máximo feedback y al menos 7.8 s con feedback cero; es un límite conservador, no una medición auditiva |
| Estado e interfaz | PASS: parámetros y perfil recuperados, attachment de Tune, pestañas, renderizados y resize |

Logs actuales en Tests/Results. Los resultados de 0.6.0, incluido pluginval, se conservan separados en Docs/History/v0_6_validation. No se han vuelto a ejecutar pluginval ni el wrapper VST3 en 0.6.1; tampoco se ha compilado en Windows. El flujo GitHub Actions incluido permite ejecutar compilación, CTest y pluginval Windows al subir el proyecto a un repositorio.

Estos tests no demuestran superioridad sonora frente a otras versiones o plugins comerciales, ni ausencia de clicks en todo material posible. No hay comparación con grabaciones vocales del usuario, prueba de ventanas en un DAW ni benchmark de CPU de su equipo. Sigue siendo un motor MIX de latencia alta; no es TRACK.
