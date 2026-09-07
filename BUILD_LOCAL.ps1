param([switch]$Package)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Set-Location $PSScriptRoot

# ── Locate cmake ─────────────────────────────────────────────────────────────
# 1. Check if cmake is already in PATH
$_cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
$cmakeExe = if ($_cmakeCmd) { $_cmakeCmd.Source } else { $null }
# 2. Try the ZIP-extracted location (no-admin install)
if (-not $cmakeExe) {
    $candidate = "$env:USERPROFILE\cmake-3.31.7-windows-x86_64\bin\cmake.exe"
    if (Test-Path $candidate) { $cmakeExe = $candidate }
}
# 3. Try the standard install path
if (-not $cmakeExe) {
    $candidate = "C:\Program Files\CMake\bin\cmake.exe"
    if (Test-Path $candidate) { $cmakeExe = $candidate }
}
if (-not $cmakeExe) {
    throw "cmake not found. Run: winget install Kitware.CMake  -or-  download the ZIP from cmake.org"
}
Write-Host "Using cmake: $cmakeExe"
& $cmakeExe --version

# ── Locate ctest (same bin dir as cmake) ─────────────────────────────────────
$ctestExe = Join-Path (Split-Path $cmakeExe) "ctest.exe"
if (-not (Test-Path $ctestExe)) { throw "ctest.exe not found next to cmake" }

# ── Helpers ──────────────────────────────────────────────────────────────────
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

if ($env:OS -ne "Windows_NT") { throw "Run this script on Windows x64." }
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "Missing git. Install from git-scm.com." }

$iscc = $null
if ($Package) {
    $candidate = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($candidate) { $iscc = $candidate.Source }
    else { $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
    if (-not (Test-Path $iscc)) { throw "Install Inno Setup 6 before packaging." }
    if (Test-Path "dist/VOXERA-0.8.0-Windows-x64-Setup.exe") {
        Remove-Item "dist/VOXERA-0.8.0-Windows-x64-Setup.exe"
    }
}

Write-Host ""
Write-Host "=== VOXERA v0.8.0 — Windows x64: configure ==="
Invoke-Checked $cmakeExe @("-S", ".", "-B", "build", "-G", "Visual Studio 17 2022", "-A", "x64", "-DVOXERA_BUILD_TESTS=ON")

Write-Host ""
Write-Host "=== Build (Release) ==="
Invoke-Checked $cmakeExe @("--build", "build", "--config", "Release", "--parallel", "4")

Write-Host ""
Write-Host "=== Tests ==="
Invoke-Checked $ctestExe @("--test-dir", "build", "-C", "Release", "--output-on-failure")

Write-Host ""
Write-Host "=== pluginval ==="
$validatorDir = Join-Path $PSScriptRoot "build/pluginval-1.0.4"
New-Item -ItemType Directory -Force -Path $validatorDir | Out-Null
Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip" -OutFile "build/pluginval.zip" -UseBasicParsing
Expand-Archive "build/pluginval.zip" -DestinationPath $validatorDir -Force
$validator = Get-ChildItem $validatorDir -Filter pluginval.exe -Recurse | Select-Object -First 1
if (-not $validator) { throw "pluginval.exe missing" }
Invoke-Checked $validator.FullName @("--strictness-level", "5", "--skip-gui-tests", "--random-seed", "12345", "--validate", "$PSScriptRoot/build/VOXERA_artefacts/Release/VST3/VOXERA+CHOP.vst3")

if ($Package) {
    Invoke-Checked $iscc @("Installer/VOXERA.iss")
    $installer = Get-Item "dist/VOXERA-0.8.0-Windows-x64-Setup.exe"
    (Get-FileHash $installer.FullName -Algorithm SHA256).Hash + "  " + $installer.Name |
        Set-Content "dist/SHA256SUMS.txt" -Encoding ASCII
    Write-Host "Installer: $($installer.FullName)"
} else {
    Write-Host ""
    Write-Host "✅  VST3 listo: build/VOXERA_artefacts/Release/VST3/VOXERA+CHOP.vst3"
    Write-Host 'Copia esa carpeta .vst3 a: C:\Program Files\Common Files\VST3\'
}
