# Validación VOXERA 0.7.0

## Comparación con detector 0.6.1
Se ejecutaron ambos detectores sobre las mismas señales sintéticas deterministas, compilados con GCC -O2. Los resultados se observan después del arranque; no son grabaciones vocales ni ensayos independientes.

| Cambio de nota 220→330 Hz | 0.6.1 | 0.7 |
|---|---:|---:|
| 44.1 kHz | 115.057 ms | 67.166 ms |
| 48 kHz | 100 ms | 68 ms |
| 96 kHz | 60 ms | 68 ms |

Tiempo hasta tres observaciones consecutivas con error <20 cents. Hay una regresión de 8 ms a 96 kHz. No se ha reducido la latencia reportada del plugin.

| Interferencia aguda sobre fundamental 220 Hz | Aciertos 0.6.1 | Aciertos 0.7 |
|---|---:|---:|
| 44.1 kHz | 0/50 | 50/50 |
| 48 kHz | 0/49 | 49/49 |
| 96 kHz | 49/49 | 49/49 |

Interferencia = seno de amplitud 0.8 a (sampleRate/4 - 700) Hz sobre seno de 220 Hz y amplitud 0.2. Este caso reproduce un problema de aliasing; no representa una tasa general de acierto del plugin.

Los demás fixtures (tono puro, armónicos, fundamental ausente, armónicos + ruido a 80/220/880 Hz) pasan todos los puntos de observación de esta ejecución en las tres frecuencias. El ruido blanco solo produce cero detecciones voiced en las observaciones evaluadas. Total: 24 fixtures. En esta ejecución el detector nuevo tardó menos en procesar todos los fixtures; las cifras de tiempo transcurrido están en el log y no se extrapolan a un porcentaje de CPU del plugin completo.

## Regresión actual
PASS: bypass de módulos, independencia espectral del tamaño de bloque, blanca a 40 BPM (144000 samples a 48 kHz), detector de tonos 80–880 Hz, ganancia enlazada, saturación mono/estéreo, bloques grandes, perfil vocal y latencia de pitch a unidad.

PASS: procesador/editor compilados y enlazados en Linux con JUCE 9.0.1 y Rubber Band 4.0.0 reales; cadena completa a 44.1/48/96 kHz mono/estéreo, bloques 0/1/64/257/2048/17/4096, bypass seco, Mix=0, estado, parámetros, estimación de cola, vínculo del mando Tune, pestañas y renderizados.

Logs actuales en Tests/Results. El benchmark y su baseline forman parte del proyecto; CMake/CTest y el flujo Windows incorporan detector_benchmark. El propio detector no asigna buffers durante pushSample/analyse; no se ha hecho una auditoría general de asignaciones de todas las dependencias.

## Pendiente
No se ha compilado el wrapper VST3 0.7 ni ejecutado pluginval para esta versión; el resultado de pluginval 0.6.0 está archivado por separado. Windows/FL Studio/Ableton y GitHub Actions siguen sin ejecutar en esta entrega. No se incluyen binarios Windows.

Faltan comparaciones auditivas con voces reales, diferentes registros/técnicas, consonantes, vibrato, ruido real y evaluación de artefactos de resíntesis. Los resultados no prueban superioridad global frente a productos comerciales. Los tiempos de ejecución de una pasada no son un benchmark de producción.
