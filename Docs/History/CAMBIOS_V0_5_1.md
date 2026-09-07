# VOXERA 0.5.1: estabilidad antes de nuevas funciones

## Cambios implementados
- Corregido el tipo inexistente `SpectralEngine`: la clase declarada es `AdaptiveSpectralEngine`.
- Ganancias de entrada/salida: una actualización de rampa por muestra, compartida entre canales. Antes cada canal consumía una parte diferente de la rampa.
- Saturación: oversampling preparado para el número real de canales; almacenamiento reservado en prepare; bloques grandes divididos sin redimensionar buffers en process.
- Saturación: señal seca retardada por la latencia entera del oversampling. Compensa el retardo nominal; no elimina la fase dependiente de frecuencia de los filtros IIR.
- Drive suavizado a frecuencia sobremuestreada y mezcla a frecuencia de audio, compartidos entre canales. Antes avanzaban una vez por bloque.
- Adaptador de pitch: retardo de acumulación B-1, ya que la primera salida se consume en la muestra B-1.
- Pitch intelligence: frecuencia de control igual a sampleRate / bloque Rubber Band.
- Perfil vocal: coeficientes de filtros precalculados; una captura con RMS <= -55 dBFS no se considera válida. Esto no es un detector de voz: un ruido fuerte aún puede pasar.
- Procesador: protección de bloques vacíos y reinicio del disparador de captura al preparar.
- Script Windows: directorio independiente del lanzamiento y comprobación de errores nativos.

## Verificación realizada
Tests C++20 de las utilidades utilizadas por el procesador: rampas estéreo, impulso con retardos 0/1/7/32/512, aislamiento de canales y reset. Compilados con -Wall -Wextra -Werror y AddressSanitizer/UndefinedBehaviorSanitizer: PASS. LeakSanitizer desactivado porque el entorno no permite su inspección de procesos; no se afirma haber validado fugas.
Etiqueta JUCE 9.0.1 verificada por git ls-remote (e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8). Se conserva Rubber Band v4.0.0.

## Pendiente antes de usar en una sesión importante
No se ha compilado el plugin completo: este entorno no dispone de CMake ni del toolchain de Windows. Los tests anteriores no validan JUCE, Rubber Band ni toda la cadena.
1. Compilar en Windows y resolver cualquier fallo adicional de integración.
2. Ejecutar pluginval y abrir VST3/Standalone en el equipo de destino.
3. Verificar impulso/latencia real de toda la cadena a 44.1/48/96 kHz, mono/estéreo y bloques variables (incluido 0).
4. Escuchar automatización de Drive/Mix/Input/Output, consonantes y notas sostenidas; medir CPU.
5. Comparar con una voz seca y volumen igualado.

## Límites que siguen abiertos
La interfaz sigue siendo la genérica de JUCE; el ZIP original no incluye la interfaz visual de referencia. No se ha añadido TRACK de baja latencia. Pitch Off desactiva la corrección, pero sigue pasando por la resíntesis y puede mantener el desplazamiento de formantes. No hay bypass global compensado. El perfil aprendido no se persiste con el estado: al reabrir, Auto Voice requiere nuevo análisis. El suavizado de retune y la robustez frente a cambios rápidos de parámetros necesitan pruebas del motor completo. El motor espectral y espacial necesitan revisión de sus transiciones de bypass. No se afirma equivalencia con plugins comerciales ni calidad profesional validada.

## Referencias técnicas
- JUCE: https://github.com/juce-framework/JUCE/tree/9.0.1
- Latencia LiveShifter: https://breakfastquay.com/rubberband/code-doc/classRubberBand_1_1RubberBandLiveShifter.html

Siguiente hito recomendado: cerrar compilación/validación Windows y estado del perfil; después interfaz propia con medición, estado de captura y controles agrupados.
