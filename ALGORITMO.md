# Algoritmo 0.8

La novedad de 0.8 está descrita en `SMART_EQ.md`; el motor de afinación 0.7 se conserva.

## Afinación
YIN/CMNDF continúa siendo el detector monofónico. En 0.7 se han cambiado la ventana y el frente de análisis descritos al final de este documento. Se borra ahora la frecuencia antigua cuando una trama se clasifica como silencio o no tiene candidato fiable.

`PitchIntelligence` ya no compara indefinidamente con una mediana de valores previamente corregidos: eso podía mantener un salto real de octava en el registro antiguo. Detecta candidatos próximos a ±1200 cents respecto a la última frecuencia aceptada; protege los aislados, acumula su duración y acepta los persistentes (60 ms con confianza >=0.9; 100 ms en otro caso). Tras 80 ms sin una frecuencia fiable olvida el contexto. Son tiempos del controlador, adicionales a la memoria del detector y a la resíntesis. No es una clasificación perfecta: un error persistente de octava también puede ser aceptado.

La transición y la actividad de movimiento moderado usan constantes temporales y movimiento normalizado a 10 ms. La actividad denominada vibrato sigue siendo una heurística, no una identificación musical validada de vibrato.

`PitchSmoother` aplica un filtro de primer orden en semitonos:

    a = exp(-dt / tau)
    actual = objetivo + a * (actual - objetivo)
    ratio = 2^(actual / 12)

Cambiar el objetivo o tau no reinicia el estado. Tau representa una constante de tiempo (~63.2% del paso), no el tiempo para completar el 100% de la corrección. La pantalla muestra el desplazamiento suavizado realmente solicitado al shifter.

El motor conserva Rubber Band LiveShifter, corrección monofónica, selección de escala y protección de interpretación. Pitch Off lleva el objetivo a unidad y desactiva el desplazamiento manual de formantes, pero el audio sigue atravesando el shifter. Para comparar con la señal original se usa el bypass global.

## Ganancia
Auto Gain comparte una envolvente RMS entre canales y actualiza por muestra. El detector usa 50 ms, reducción de ganancia 80 ms y aumento 500 ms. La compensación se limita a -12/+18 dB; bajo -55 dBFS se vuelve gradualmente a 0 dB de compensación. Desactivarlo también vuelve suavemente a unidad. Estos valores son decisiones iniciales, no un sustituto de pruebas auditivas con voces.

## Señal paralela
La rama seca se captura antes de Input, Auto Gain y efectos. Se retrasa por el retardo declarado de pitch + saturación. Mix usa mezcla lineal, no equal-power, para no sumar +3 dB cuando las ramas son idénticas. Bypass reduce el peso procesado a cero con una rampa de 20 ms. Output ajusta la rama procesada antes de la mezcla global; la señal de bypass permanece original.

La compensación alinea el retardo nominal: no garantiza coherencia de fase entre una señal original y otra afinada ni elimina la fase de filtros IIR. Combinar ambas puede crear doblado/chorus. Bypass no ahorra CPU porque los motores siguen activos. No hay limitador true-peak.

## Persistencia y tiempo real
El hilo de audio usa buffers reservados en prepare y divide bloques grandes. Los intercambios de perfil usan copias de tamaño fijo y adquisición atómica de un solo intento: el hilo de audio omite el intercambio si está ocupado, sin esperar. La serialización puede esperar en un hilo no destinado a audio. No se accede a ValueTree desde el procesamiento de muestras para guardar el perfil.

La captura es transitoria y se guarda desactivada. El estado incorpora VoiceProfile y una versión de esquema. Al importar estados antiguos, Tone=0, Mix=100 y Bypass=false si esos parámetros no estaban presentes. Se conservan los IDs existentes del plugin y de parámetros. Los perfiles cargados validan finitud y límites de sus estadísticas.

## Trabajo restante
Validar material vocal real, falsas octavas sostenidas, consonantes, vibrato, cambios rápidos de tono, consumo CPU, bypass de módulos individuales, latencia a ratios extremos y duración real de colas. RNNoise, Signalsmith, RTNeural y otras bibliotecas no se han integrado: ninguna mejora su rendimiento por el hecho de añadirla.


## Revisión 0.6.1: bloques, bypass y delay
El motor espectral calcula el análisis en orden cronológico, antes de procesar cada muestra. Los filtros se actualizan cada round(sampleRate * 0.001) muestras; el contador persiste entre callbacks. Se elimina así el análisis adelantado por bloque y el suavizado dependiente de su longitud. No se añade un buffer de latencia.

Ambos módulos usan una única rampa wet de 20 ms compartida por los canales. Desactivar el motor espectral mezcla hasta la señal de entrada del módulo; desactivar el espacial lleva su suma de efectos a cero. Los filtros y líneas continúan evolucionando, evitando congelar memorias y recuperarlas antiguas al reactivar. No es una función de ahorro de CPU ni deja oír la cola tras completar la rampa.

El tiempo de delay se calcula en doble precisión y se convierte a muestras al final. El buffer existente de 4 s permite los 3 s requeridos por la división más larga al tempo mínimo admitido. La lectura ocurre antes de push; se compensa una muestra porque FractionalDelay::read se refiere a la última muestra escrita.

El host recibe una estimación de cola de 4.8 + 3*(1 + ln(0.001)/ln(feedback)) segundos; con feedback cero el término de repeticiones es cero. La estimación usa el tempo más lento soportado, aunque el proyecto vaya más rápido, y un umbral relativo de -60 dB. No es una medición de silencio absoluto ni una garantía de que todos los hosts respeten la cola. Se conserva el desplazamiento de tono al variar continuamente el tiempo del delay: aún no se ha añadido un cambio de taps mediante crossfade.


## Revisión 0.7: frente de análisis YIN
Dos biquads low-pass (Q 0.541196100146197 y 1.306562964876377) forman un Butterworth de cuarto orden. El corte nominal es 1800 Hz, limitado al 35% de la frecuencia de análisis para frecuencias de host bajas. Su objetivo es atenuar contenido agudo antes de diezmar, reduciendo componentes que podrían aparecer como graves por aliasing. No garantiza supresión completa del aliasing.

El factor de reducción es max(1, round(sampleRate/12000)). A 44.1 kHz resulta 4, a 48 kHz 4 y a 96 kHz 8. Se conserva un promedio de las muestras filtradas de cada grupo. La ventana es ceil(analysisRate*0.048), con un mínimo de 128 muestras; el salto es round(analysisRate*0.008), con mínimo 1. Esos mínimos sólo afectan frecuencias inusualmente bajas.

Antes del bucle de diferencias YIN se copia la ventana a memoria cronológica reservada en prepare. La selección CMNDF, umbrales y cuantización musical siguen siendo los anteriores. La nueva ventana sacrifica parte de la memoria histórica: no se afirma que sea óptima para todos los cantantes o ruido.

El frente de análisis convierte NaN/Inf de entrada a cero sólo dentro del detector, evitando contaminar sus filtros. No sanea la señal audible de todo el plugin.

El benchmark conserva el detector previo en Tests/Baseline/YinPitchDetector061.h. Cada fixture dura 0.6 s; se observa tras 0.2 s, aproximadamente cada 8 ms. Se cuenta como acierto una detección voiced a menos de 20 cents del objetivo. Las ventanas se solapan: los puntos no son ensayos estadísticamente independientes. La prueba de cambio exige tres observaciones consecutivas correctas tras 220→330 Hz; no mide latencia de audio ni tiempo para que el cantante oiga la corrección.

Los tiempos de ejecución registrados son tiempo transcurrido local de una pasada, no consumo de CPU certificado ni benchmark del PC del usuario. En todos los fixtures de esta ejecución el nuevo detector tardó menos; la magnitud varía por carga y plataforma.
