# Smart EQ 0.8.0

## Objetivo y señal
Reducir prominencias locales de energía que persisten, con un límite conservador compartido. Insertada después del motor espectral existente y antes del compresor. No modifica la latencia declarada: no retarda muestras, aunque sus filtros cambian la fase y el detector tiene tiempo de reacción.

Siete detectores pasabanda de Q=1 con centros de 125 Hz a 8 kHz analizan la entrada de esta etapa. La potencia se promedia entre canales **después de elevar al cuadrado**, evitando cancelación por contrafase. Envolvente exponencial de 200 ms; control cada aproximadamente 5 ms, independiente de los límites de bloque del host. Cinco filtros peaking Q=0.9 aplican ganancias comunes a los canales.

## Regla de corrección
Para cada banda interior:

    exceso = dB(banda) - 0.5 * (dB(vecina inferior) + dB(vecina superior))
    petición = Amount * clamp(0.75 * (exceso - 4), 0, Budget)

Las peticiones se escalan si la suma supera Amount × Budget. Ataque = Response; recuperación = 3 × Response. Después del suavizado se limita de nuevo la suma de las atenuaciones a Budget, incluyendo cambios de automatización. Un cambio brusco de Budget hacia abajo puede acelerar la retirada de corrección; no se garantiza ausencia absoluta de artefactos durante automatización extrema.

La puerta de potencia (-55 dBFS) retira las peticiones en silencio. No identifica voz frente a música o ruido. Los filtros solo atenúan. Amount=0 en una instancia nueva deja esta etapa exactamente neutra; tras usarla, la corrección decae gradualmente hasta cero. El conjunto de los cinco filtros respeta el presupuesto estático; ese presupuesto no incluye Clean, De-ess, Body, Tone ni Air.

## Integración
IDs nuevos al final de APVTS: `smartEQAmount`, `smartEQRange`, `smartEQResponse`. Valores iniciales 0%, 3 dB y 250 ms. Guardado/restauración mediante el estado del host y presets de archivo. Los presets de fábrica mantienen la elección Smart EQ actual. `spectralOn` controla el motor previo; Smart EQ tiene su propia cantidad. No hay asignaciones dinámicas ni bloqueo explícito en `SmartEQ::process`; las cinco lecturas para la UI son atómicas.

## Alcance de validación
Pruebas sintéticas: identidad inicial, atenuación medible a 1 kHz, límite compartido, recuperación tras silencio, enlace estéreo, contrafase e igualdad exacta con distintos tamaños de bloque a 44.1/48/96 kHz. La reducción del tono a máxima cantidad fue aproximadamente 0.84–0.86 dB. No demuestra la calidad perceptual sobre voces ni el uso real de todo el margen de 6 dB: la detección conservadora puede pedir menos.

No incluye aprendizaje de una voz objetivo, eliminación neural de ruido, EQ por referencia o detección separada de vocales/consonantes. Se necesita comparación A/B con voces reales para decidir ajustes musicales.
