# Modelos neuronales

VOXERA ejecuta capturas en la etapa **Neural**, donde iría un previo. Se cargan desde:

```
Documentos\VOXERA\Models
```

La carpeta se crea sola. Deja ahí los archivos y aparecerán en el menú del botón **LOAD NEURAL MODEL** de la pestaña PRESETS.

## Formatos

| Extensión | Qué es |
|---|---|
| `.nam` | Captura de [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) |
| `.json` | Modelo de RTNeural (lo que exportan Keras y las herramientas de Aida-X) |

**Solo arquitectura LSTM.** Los modelos WaveNet se rechazan con un mensaje que lo explica: cuestan del orden de una décima de núcleo cada uno, y la cadena entera de VOXERA funciona en una décima de núcleo. No es una limitación pendiente de arreglar, es una decisión.

## Qué capturas sirven para una voz

Aunque NAM esté pensado para amplificadores de guitarra, el proceso de captura vale para cualquier cosa que un micrófono atraviese. Lo que interesa aquí:

- **Previos de micrófono** — es el uso natural de esta etapa
- **Canales de consola**
- **Compresores y ecualizadores de rack** con carácter
- Cadenas completas de grabación

Un modelo de amplificador de guitarra funciona técnicamente, pero está capturado con la señal y los niveles de una guitarra; sobre una voz suele ser demasiado.

La biblioteca grande está en [TONE3000](https://www.tone3000.com/).

## Frecuencia de muestreo

Una captura es una red recurrente: su estado avanza **una vez por muestra**, así que sus constantes de tiempo están definidas en muestras y no en segundos. Darle 44,1 kHz a un modelo entrenado a 48 estiraría todas ellas un nueve por ciento.

VOXERA remuestrea alrededor del modelo para ejecutarlo siempre a su frecuencia. **No tienes que hacer nada**, pero el plugin te dirá cuándo está ocurriendo.

## Sobre redistribuir capturas

**VOXERA no incluye ninguna, y es deliberado.**

Una captura descargada trae los términos que le pusiera quien la hizo, y "gratis para usar" no es lo mismo que "libre para redistribuir dentro de otro producto". Además, el equipo que modela lleva un nombre que es marca registrada de otro: poder cargar una captura de un previo famoso no da derecho a vender nada usando ese nombre.

Si vas a distribuir VOXERA con modelos incluidos, cada uno necesita permiso explícito de redistribución de su autor.

**La vía limpia es capturar tu propio hardware.** Cualquier previo, interfaz con carácter o pedal que tengas se puede capturar con las herramientas de NAM. El modelo resultante es tuyo: sin dudas de licencia, sin problema de marca, y eliges la arquitectura para que entre en tu presupuesto de CPU.
