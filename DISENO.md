# Interfaz

Implementación nativa en `Source/PluginEditor.cpp`, sin navegador embebido. Fondo y mandos vectoriales; los textos, medidores y señal son componentes/datos reales. Resolución base 1200×840; rango 960×672 a 1600×1120, manteniendo proporción.

La referencia visual se conserva en Design/Reference.jpeg. Esta versión reinterpreta el diseño: mascota vectorial simplificada, sin interruptor Power decorativo, Tune 0–100 en lugar de una escala de transposición y presets en su propia pestaña.

- Vocals: tonalidad, escala, modo, velocidad, humanización y perfil.
- FX: ancho, doblado, delay, feedback, drive y mezcla de saturación.
- Presets: cinco puntos de partida; archivos propios .voxera con perfil incluido.
- More: editor genérico de parámetros avanzados dentro de la interfaz.

La forma de onda muestra el canal izquierdo de salida, diezmado para visualización; no es un analizador espectral. Medidores de pico de bloque con caída visual; no son LUFS ni true-peak. Cambiar un mando usa los attachments de APVTS; los cambios de preset notifican al host. Doble clic restablece el valor predeterminado del parámetro.

Las capturas Previews usan una señal sintética de prueba. No representan una voz del usuario ni garantizan el acabado gráfico de otra plataforma.
