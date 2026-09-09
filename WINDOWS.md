# Obtener el instalador de Windows x64

**Estado:** consulta `VALIDACION.md` para la ejecución local más reciente. El código fuente requiere compilación; la carpeta `build/VOXERA_artefacts/Release` contiene los binarios locales cuando se han construido.

## Opción A: compilar en tu PC
Instala estas herramientas oficiales (solo necesarias para construir, no para usar el instalador resultante):

1. [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/vs/older-downloads/) o Visual Studio 2022 con «Desarrollo para el escritorio con C++», MSVC v143 y Windows SDK.
2. [CMake 3.24 o posterior](https://cmake.org/download/) disponible en PATH, y [Git for Windows](https://git-scm.com/downloads/win).
3. [Inno Setup 6](https://jrsoftware.org/isdl.php).

Extrae el ZIP completo, abre la carpeta VOXERA_V0_8 y ejecuta `CREAR_INSTALADOR_WINDOWS.cmd`. También puedes abrir PowerShell en esa carpeta y ejecutar:

```powershell
./BUILD_WINDOWS.ps1 -Package
```

El script necesita Internet para descargar las dependencias y pluginval. Se detiene si falla la compilación, alguna prueba o pluginval; solo entonces invoca Inno Setup. No cambia la política de ejecución de PowerShell. Si tu equipo impide ejecutar scripts, usa las normas de tu equipo o la opción GitHub Actions.

Resultado esperado: `dist/VOXERA-1.5-1.5.0-Windows-x64-Setup.exe` y `dist/SHA256SUMS.txt`. No se firma digitalmente el ejecutable. El hash verifica integridad; no sustituye una firma.

## Opción B: GitHub Actions
El paquete incluye `.github/workflows/windows.yml`. Coloca el contenido de VOXERA_V0_8 en la raíz de un repositorio tuyo, incluyendo el workflow. En Actions selecciona **Windows x64 build, validation and installer**, después **Run workflow**. Si todas las puertas pasan, descarga el artefacto **VOXERA-1.5.0-Windows-x64-Installer**. No se ha subido ni lanzado este proyecto en un repositorio externo en esta entrega.

## Instalar el resultado
Cierra el DAW, ejecuta el Setup y acepta la elevación para instalar el VST3 en `C:\Program Files\Common Files\VST3\VOXERA 1.5.vst3`. La aplicación autónoma va en `C:\Program Files\VOXERA 1.5`. Abre el DAW y vuelve a escanear VST3. En FL Studio utiliza Plugin Manager; en Ableton activa las carpetas de sistema VST3 y vuelve a escanear. El instalador añade una entrada de desinstalación.

El standalone no tiene ASIO incorporado en esta configuración. Para trabajar con tu interfaz y su controlador ASIO, carga el VST3 desde el DAW. No se ha probado esta versión con SSL2+, Apollo, FL Studio o Ableton en Windows.

## Antes de llamarla final
Antes de distribuir siguen siendo necesarias instalación/desinstalación real y pruebas de restauración/automatización dentro de los DAW elegidos. Low Latency retira el afinador de la ruta de señal para monitorización. Las pruebas automáticas y el procesado de grabaciones se documentan por separado de una escucha artística.
