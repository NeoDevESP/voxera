# VOXERA 0.8.0 — Smart EQ

Procesador vocal C++/JUCE para VST3 y aplicación autónoma. Esta entrega contiene **código fuente y la ruta para generar un instalador Windows**, no un instalador Windows ya compilado. No es todavía una versión final validada en un DAW Windows.

## Novedades
- Smart EQ adaptativa: cinco bandas (250, 500, 1000, 2000, 4000 Hz), análisis estéreo enlazado y correcciones solo de atenuación.
- Cantidad, límite total de corrección de 1–6 dB y respuesta de 100–1000 ms; lectura de las ganancias reales en la pestaña SMART EQ.
- Parámetros automatizables y persistentes. Sesiones anteriores cargan Smart EQ a 0%; se conservan los identificadores e índices de los parámetros existentes.
- Dependencias de audio fijadas a commits; compilación, regresiones, pluginval e instalador Inno Setup reunidos en un script Windows y un workflow GitHub Actions.

## Probar la EQ
1. Abre SMART EQ y reproduce una voz seca.
2. Como punto de partida orientativo: Amount 40%, Budget 3 dB, Response 250 ms.
3. Compara bajando Amount a 0%; la corrección se retira gradualmente. El BYPASS principal afecta a toda la cadena.
4. Las barras muestran la ganancia de cada filtro, no el espectro de entrada ni una curva de respuesta combinada.

La EQ detecta prominencias locales de energía, no sabe qué timbre deseas. Puede reducir formantes útiles; no es una red neuronal, un sistema de aprendizaje por referencia ni una garantía de mejor sonido. Convive con Clean, De-ess y Tone; el límite Smart EQ se aplica solo a sus cinco filtros, no a toda la cadena.

## Windows
Lee **WINDOWS.md**. Con las herramientas instaladas, `CREAR_INSTALADOR_WINDOWS.cmd` construye, prueba y empaqueta. El resultado esperado es `dist/VOXERA-0.8.0-Windows-x64-Setup.exe`. El instalador coloca el VST3 en la carpeta común de plugins y ofrece la aplicación autónoma. No se ha ejecutado esa ruta en Windows en esta entrega.

## Documentación
- `SMART_EQ.md`: diseño y límites de la nueva EQ.
- `LIBRERIAS.md`: selección contrastada, alternativas y enlaces oficiales.
- `VALIDACION.md`: resultados y limitaciones comprobadas.
- `ALGORITMO.md`: motor heredado de afinación y efectos.
- `Previews/`: capturas del editor JUCE real; Smart EQ muestra una señal sintética de prueba.
- `Docs/History/`: documentación y evidencia de versiones anteriores; no valida esta versión.
