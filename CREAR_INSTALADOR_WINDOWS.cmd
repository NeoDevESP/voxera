@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -File "%~dp0BUILD_WINDOWS.ps1" -Package
if errorlevel 1 (
  echo ERROR: No se ha generado el instalador. Lee WINDOWS.md y el mensaje anterior.
  pause
  exit /b 1
)
echo Instalador generado en la carpeta dist.
pause
