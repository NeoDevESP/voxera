# Selección de librerías y herramientas — 7 septiembre 2026

No hay una librería universalmente mejor. Se priorizan adecuación al procesamiento en tiempo real, integración ya comprobada y pruebas reproducibles. Las alternativas se han revisado documentalmente; no se ha realizado una comparativa auditiva entre motores.

| Proyecto oficial | Función documentada | Decisión para VOXERA 0.8 |
|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) | Framework C++ multiplataforma, interfaz y formatos de plugin | Mantener 9.0.1 y sus módulos DSP; base ya integrada |
| [Rubber Band](https://github.com/breakfastquay/rubberband) | Cambio de tono y duración; API LiveShifter | Mantener 4.0.0; no sustituir un motor sin comparación de resultados |
| [chowdsp_utils](https://github.com/Chowdhury-DSP/chowdsp_utils) | Módulos DSP, filtros, compresión, buffers y utilidades para JUCE | Candidato para futuros filtros y dinámica; no añadido como dependencia sin una necesidad medida |
| [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) | Pitch/time, formantes y latencia configurable mediante tamaño de bloque | Alternativa para un futuro banco A/B; no se afirma superioridad sobre Rubber Band |
| [RNNoise](https://github.com/xiph/rnnoise) | Supresión de ruido con red recurrente orientada a voz hablada | No integrado: falta evaluar su comportamiento con canto y adaptar el flujo de audio |
| [RTNeural](https://github.com/jatinchowdhury18/RTNeural) | Inferencia de redes para audio en tiempo real | No aporta por sí solo un modelo entrenado de EQ; no integrado |
| [pluginval](https://github.com/Tracktion/pluginval) | Prueba y validación de plugins multiplataforma | Puerta de validación del VST3 antes de empaquetar |
| [Inno Setup](https://jrsoftware.org/isinfo.php) | Generación de instaladores Windows | Script incluido para VST3 y standalone; ejecución Windows pendiente |

## Dependencias de audio fijadas
- JUCE 9.0.1: `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8`.
- Rubber Band 4.0.0: `1d95888bec3ae0a17c0c4af791810d5a63f6bc35`.
- pluginval descargado por el script: release 1.0.4. Inno Setup en CI: 6.7.3 con comprobación de firma. Las herramientas de compilación y Actions aún no están fijadas íntegramente a hashes; no se promete reproducción binaria bit a bit.

La Smart EQ nueva es implementación propia y utiliza el Biquad existente. No se ha incorporado código de las alternativas revisadas. La disponibilidad de un proyecto en GitHub no demuestra por sí sola calidad, compatibilidad o ausencia de fallos.
