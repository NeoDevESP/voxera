# VOXERA 0.7.0 — beta de desarrollo

Procesador vocal C++/JUCE con interfaz nativa metal/negro/rosa inspirada en la referencia proporcionada. Incluye código fuente, pruebas y capturas reales de la interfaz; no incluye un instalador Windows.

## Nuevo en 0.7
- Detector YIN con filtro de análisis Butterworth de cuarto orden antes de reducir la frecuencia de muestreo. No filtra directamente la señal audible.
- Frecuencia de análisis cercana a 12 kHz, ventana de 48 ms y salto de aproximadamente 8 ms. Se mantiene un coste de análisis acotado al aumentar la frecuencia del host.
- Buffer cronológico reservado previamente: se eliminan las lecturas circulares repetidas del bucle intensivo de YIN.
- Comparativa automática contra el detector 0.6.1 en 24 casos sintéticos a 44.1/48/96 kHz, más cambios de nota.

La prueba de cambio 220→330 Hz mejora a 44.1/48 kHz, pero empeora 8 ms a 96 kHz. No se reduce la latencia del shifter ni se convierte VOXERA en un motor TRACK. Consulta VALIDACION.md para las cifras y los límites.

## Conservado de 0.6.1
- Bypass de los motores espectral y espacial con transición enlazada de 20 ms; sus estados siguen avanzando mientras están desactivados.
- Análisis espectral por muestra y actualización de filtros a intervalos fijos de aproximadamente 1 ms. La salida deja de depender de cómo el host divide una misma señal en bloques, con controles constantes.
- Delay sincronizado: la blanca a 40 BPM dura 3 segundos; se corrigen el límite anterior de 1.9 s, el desfase de lectura de una muestra y el redondeo prematuro del tiempo.
- Estimación conservadora de cola según feedback, usando el delay máximo de 3 s y reverb de hasta 4.8 s. Puede alargar el tiempo de render/exportación que algunos hosts añaden después del audio.

## Funciones conservadas de 0.6
- Editor propio redimensionable: cinco mandos, medidores de pico, osciloscopio real, mascota vectorial y pestañas Vocals / FX / Presets / More.
- Afinación: suavizado en semitonos continuo, independiente del tamaño de bloque de control; los cambios de Retune no reinician la rampa.
- Protector de octavas: retiene errores aislados y acepta cambios sostenidos tras 60–100 ms de observación del controlador; reinicia el contexto tras una pausa sin voz.
- Auto Gain: seguimiento RMS por muestra, enlazado entre canales, con ataque/liberación y umbral para no perseguir señales débiles.
- Mix y Bypass globales con retardo nominal compensado y transición de 20 ms. La cadena sigue calculándose durante bypass para mantener la continuidad.
- El perfil vocal se guarda en la sesión y en archivos .voxera. Las capturas no se reinician al cargar un estado.
- Cinco presets de fábrica y guardado/carga de presets personales. Los presets de fábrica conservan Key, Scale, Input, Output y el perfil vocal.
- Flujo de GitHub Actions para compilación y pruebas Windows, preparado pero no ejecutado desde esta entrega.

## Compilar en Windows
1. Instalar Visual Studio 2022 con la carga de trabajo Desarrollo para el escritorio con C++, CMake 3.22 o posterior y Git.
2. Abrir PowerShell y ejecutar `./BUILD_WINDOWS.ps1` desde esta carpeta.
3. El script descarga las dependencias, compila y ejecuta las pruebas. Si falla, conserva el error completo; no anuncia éxito.
4. El plugin estará en `build/VOXERA_artefacts/Release/VST3/VOXERA.vst3`. Copiar la carpeta .vst3 completa al directorio VST3 del equipo y volver a escanear desde el DAW.
5. El ejecutable independiente queda en `build/VOXERA_artefacts/Release/Standalone/`.

Alternativa: colocar el contenido de esta carpeta en la raíz de un repositorio GitHub. `.github/workflows/windows.yml` genera el artefacto Windows después de compilar y pasar CTest. No se ha creado ni publicado ningún repositorio en esta entrega.

## Uso
1. En Vocals, elegir Key y Scale de la canción. Tune controla cantidad de corrección (no transposición).
2. Retune regula velocidad; Humanize reduce corrección en pequeñas desviaciones; Mode selecciona Natural / Modern / Hard.
3. Pulsar Analyze Voice y cantar 8 segundos con audio en marcha. Al aparecer Profile Ready, subir Auto Voice progresivamente.
4. Tone aplica un balance tonal; Air añade/quita agudos; Space controla reverb; Mix combina señal seca y procesada.
5. FX contiene anchura, dobles, delay, feedback y saturación. More da acceso al resto de parámetros, incluidos entrada/salida, compresor, formantes y activación de módulos.
6. Presets permite guardar y recuperar ajustes completos. Los cinco botones de fábrica son puntos de partida, no modos DSP diferentes ni presets validados con grabaciones reales.

## Estado de validación
Ver `VALIDACION.md`. El procesador y el editor se han compilado y ejecutado en un programa de integración Linux con JUCE 9.0.1 y Rubber Band 4.0.0 reales. Las imágenes de `Previews/` proceden del componente JUCE renderizado, no de un mockup web. El acabado es una recreación vectorial; no reproduce exactamente el metal fotográfico o la mascota 3D de la referencia.

La versión anterior 0.6.0 pasó pluginval en Linux. En 0.7 se han compilado y ejecutado el procesador/editor y las pruebas DSP/de integración; no se ha repetido la compilación del wrapper VST3 ni pluginval. Pendiente: ejecutar el VST3 en Windows/FL Studio/Ableton y probar sesiones con voces reales. El motor sigue siendo MIX de latencia alta: alrededor de 54.8 ms a 48 kHz en las pruebas de esta versión. No es un modo TRACK de monitorización directa.

Conserva la versión anterior y los renders de sesiones importantes: los cambios de Auto Gain y afinación modifican el sonido aun manteniendo los mismos valores de parámetros.
