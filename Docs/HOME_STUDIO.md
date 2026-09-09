# VOXERA para home studio

## Flujo

1. Escoge un sonido inicial en el selector superior. Selecciona tonalidad y escala si quieres afinar.
2. Reproduce una frase representativa y pulsa AUTO MIX. Escucha ocho segundos; el informe queda visible y se puede desplazar.
3. Compara con A/B y MATCH LEVEL. Undo recupera los cambios del último Auto Mix.
4. Ajusta Tune, Tone, Air, Space y Mix. Abre las páginas específicas cuando necesites más control.
5. Activa LOW LATENCY al grabar si necesitas escuchar con menos retardo. Este modo desactiva la corrección de afinación; la cifra de latencia incluye el plugin insertado.

## Sonido

La reducción de cuerpo de Auto Mix ahora depende de la turbidez detectada: una toma sin turbidez no recibe un recorte fijo de graves. Optical, Density, Punch y Clip parten de cantidades menores para evitar acumular demasiada compresión sobre voces ya controladas. Clarity sigue siendo un sonido estético, no una referencia universal.

El de-esser mide energía después de añadir color, combina canales sin cancelación por polaridad y suaviza su respuesta. HOLD: HEAR ESSES permite escuchar lo retirado mientras se mantiene pulsado. No guarda ese modo de escucha en las sesiones.

## Plugins externos

El nuevo efecto y su memoria de compensación se preparan antes de intercambiarlos bajo el bloqueo de audio. Las ventanas retienen la instancia a la que pertenecen hasta cerrarse; al restaurar otra sesión se cierra la ventana anterior en la siguiente actualización del editor. Una sesión sin insert elimina el insert anterior.

Se propagan transporte y modo de render; se revisan cambios de latencia y se incluye la cola del efecto. Un cambio dinámico de latencia reconstruye la compensación con audio protegido; puede provocar un breve reajuste del host.

Auto Mix no interpreta nombres como Threshold o Input para adivinar su función. Desde INSERT PLUGIN elige explícitamente el parámetro y activa «More compression = lower value» cuando corresponda. Auto Mix mueve como máximo el 20 % del recorrido desde el valor actual, escalado por diagnóstico e intensidad. No es una calibración física en dB para cada fabricante. Intensidad cero y bloqueo de dinámica conservan el valor; A/B y Undo restauran el parámetro externo modificado.

## Referencia de producto

La referencia de flujo es [VocalDose](https://www.vocalessential.com/vst): llegar rápido a una cadena vocal editable. VOXERA mantiene su diseño y motores propios. Esta revisión no incorpora reconocimiento de voz ni reproduce sus algoritmos.

## Comparación de grabaciones

El comparador exige la misma frecuencia de muestreo y analiza la duración común, con ventanas de actividad compartidas de 20 ms por encima de -55 dBFS. Informa del ajuste RMS para comparar B con A. No alinea interpretaciones distintas ni mide LUFS. Tres fragmentos locales de FL Studio sirven para comprobación técnica; una valoración musical requiere escucharlos en contexto con la instrumental.
