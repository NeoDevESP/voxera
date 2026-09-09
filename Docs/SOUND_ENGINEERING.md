# Dinámica y color — revisión de septiembre de 2026

Los modos de VOXERA son diseños de comportamiento propios. Las referencias ayudan a elegir tiempos, topología y recuperación; no se han medido unidades físicas ni se afirma equivalencia con un 1176, LA-2A, SSL o Manley.

## Referencias consultadas

| Familia | Referencia primaria | Decisión en VOXERA |
|---|---|---|
| FET | [Universal Audio: 1176, controles y uso](https://www.uaudio.com/blogs/ua/1176-collection-tips) | Ataque del elemento limitado a 20–800 µs; recuperación rápida limitada a 50–1100 ms, con cola dependiente del programa. Detección rápida, realimentación y color asociado al trabajo del compresor. |
| Opto | [Universal Audio: historia técnica del LA-2A](https://help.uaudio.com/hc/en-us/articles/215779663-A-History-of-the-Teletronix-LA-2A-Leveling-Amplifier) | Nuevo modo Opto: ataque del elemento de 10 ms y recuperación con componente rápida de 60 ms y lenta de 3 s. La etapa Optical independiente usa memoria de la reducción, 60 ms y 1,5 s; estos coeficientes no equivalen a tiempos medidos de recuperación del aparato. |
| VCA | [SSL: guía del Bus Compressor](https://www.solidstatelogic.com/assets/uploads/downloads/plug-ins/SSL%20Native%20V6%20User%20Guide%202.1.5.pdf) | Detección anticipada respecto a la ganancia, rodilla suave, recuperación dependiente del programa, filtro de detector y mezcla paralela. |
| Vari-Mu | [Manley: Nu Mu, funcionamiento de ataque y recuperación](https://www.manley.com/pro/mnumu) | Se conserva la voz lenta con realimentación y ratio progresivo; el color asimétrico ahora elimina la componente continua. Nu Mu es una referencia de controles, no un circuito reproducido. |

Los valores de Attack/Release del host siguen siendo los controles existentes: FET y Vari-Mu los escalan según su carácter; Opto usa tiempos propios. No deben leerse como una medición del ataque efectivo de toda la cadena, que incluye el detector. Se mantienen los índices anteriores; Opto se añade al final.

## Cambios en ingeniería

- Detector estéreo enlazado por el máximo de magnitudes filtradas de cada canal: una señal L=-R ya no desaparece del detector. Se aplica la misma reducción a los dos canales.
- Memoria de recuperación calculada desde la frecuencia de muestreo, actualizada durante ataque y recuperación. Evita que el comportamiento dependa del número de muestras por segundo.
- Comp Sauce controla la no linealidad del elemento sin cambiar el umbral. Clean sigue sin color. La rectificación asimétrica pasa por un eliminador de continua a 8 Hz antes de la realimentación y la mezcla.
- Comp Mix y Comp Sauce cambian suavemente durante el audio. La configuración inicial se aplica directamente para que Mix=0 arranque como identidad.
- Los once presets fijan umbral, ratio, ataque, recuperación y Sauce; reinician los demás parámetros sonoros salvo el contexto documentado. No dependen del último preset utilizado.
- Match Level compara energía RMS del audio procesado y la entrada retardada, con un promedio de dos segundos, transición de 250 ms y límite de -36 a +9 dB. La mayor capacidad de atenuación permite comparar tomas de micrófono que Auto Gain ha elevado mucho. No aumenta ganancia por silencios. Está antes del limitador y apagado por defecto.

## Uso

En **PRESETS**, elige Clean, FET, VCA, Vari-Mu u Opto y ajusta **COMP SAUCE**. El resto de controles de compresión está en **MORE**.

Como punto de partida de diseño: FET para contener picos, VCA para control regular, Vari-Mu para color suave y Opto para nivelado. Si acumulas Opto y Optical, comprueba los medidores de reducción para evitar comprimir dos veces más de lo necesario. Para más textura, aumenta Sauce y reduce Comp Mix; compara con Match Level activado.

En **VOCALS**, Auto Mix escucha ocho segundos. **MIX OPTIONS** permite elegir intensidad y conservar afinación, EQ, dinámica o color. **A/B** alterna solo los parámetros que cambió el último Auto Mix y **UNDO MIX** los devuelve al punto anterior. Cambiar de preset o cargar una sesión descarta ese historial temporal. Match Level es independiente de A/B: actívalo para comparar y deja unos segundos de audio para que se estabilice.

## Alcance de la verificación

Las pruebas cubren polaridad estéreo, estabilidad, silencio, partición de bloques, captura en distintas frecuencias, restauración sobre una instancia usada, presets repetibles, A/B, deshacer, bloqueos e igualación RMS. Los detalles de la ejecución se recogen en `VALIDACION.md`.

El limitador sigue siendo de pico de muestra: no se añade una garantía true-peak. El color del compresor no incorpora un nuevo sobremuestreador; la etapa Saturator conserva su sobremuestreo 4×. No se afirma ausencia de aliasing a Sauce máximo. La aprobación artística requiere escuchar las comparaciones, especialmente consonantes, respiraciones y colas, dentro de la mezcla.

## Rebalanceo medido, septiembre de 2026

Los once presets se compararon con una voz comercial acabada, renderizando la misma toma por ambos caminos y midiendo banda a banda.

La referencia sostiene 17,6 dB de cresta. Los presets iban de 13,1 a 3,9, que pasado cierto punto no es compresión con carácter sino una toma sin vida. Y todos dejaban entre el 44 y el 52 por ciento de su energía por debajo de 250 Hz donde la referencia guarda 28; en una voz esa banda es sobre todo la sala y la proximidad del micrófono.

De ahí salieron cuatro decisiones:

- La reducción de ganancia baja en los once, y la limpieza de graves sube en los once. Rage pasa de 3,9 a 8,0 dB de cresta, Radio de 6,1 a 10,4, Modern queda en 17,0.
- Ratio de compresión y estante de graves entran en la tabla de presets. Estaban tratados como ajuste fino bajo las macros y resultaron ser los dos mandos que mueven las dos diferencias medidas.
- Las etapas correctivas —Vocal Lock, Smart EQ, aire y graves— pasan a venir encendidas. Una grabación casera llega con proximidad abajo, resonancias de sala en medios y sin aire arriba, y eran justo las tres que los valores de fábrica dejaban apagadas.
- Level Match viene encendido. La limpieza se lleva unos 3 dB, y además más fuerte se oye como mejor: con el nivel igualado se juzga el procesado en vez del volumen.

Queda una diferencia sin cerrar: el grave se queda en el 38 por ciento frente al 28 de la referencia. Se puede bajar más, pero adelgaza la voz, y no se persigue la cifra hasta estropear el sonido.

Preset nuevo **Clarity**, construido con esos números y no de oído: a 0,61 dB del RMS de la referencia, a 1,63 dB de su cresta, y con la octava alta idéntica.
