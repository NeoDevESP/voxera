param([switch]$Package)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Set-Location $PSScriptRoot
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
if ($env:OS -ne "Windows_NT") { throw "Run this script on Windows x64." }
foreach ($program in @("cmake", "ctest", "git")) {
    if (-not (Get-Command $program -ErrorAction SilentlyContinue)) { throw "Missing $program. See WINDOWS.md." }
}
$iscc = $null
if ($Package) {
    $candidate = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($candidate) { $iscc = $candidate.Source }
    else { $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
    if (-not (Test-Path $iscc)) { throw "Install Inno Setup 6 before packaging. See WINDOWS.md." }
    # A failed build must not leave a stale installer looking like a new release.
    if (Test-Path "dist/VOXERA-0.8.0-Windows-x64-Setup.exe") {
        Remove-Item "dist/VOXERA-0.8.0-Windows-x64-Setup.exe"
    }
}
Write-Host "VOXERA v0.8.0 - Windows x64: build, tests, plugin validation"
Invoke-Checked cmake @("-S", ".", "-B", "build", "-G", "Visual Studio 17 2022", "-A", "x64", "-DVOXERA_BUILD_TESTS=ON")
Invoke-Checked cmake @("--build", "build", "--config", "Release", "--parallel", "2")
Invoke-Checked ctest @("--test-dir", "build", "-C", "Release", "--output-on-failure")
$validatorDir = Join-Path $PSScriptRoot "build/pluginval-1.0.4"
New-Item -ItemType Directory -Force -Path $validatorDir | Out-Null
Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip" -OutFile "build/pluginval.zip"
Expand-Archive "build/pluginval.zip" -DestinationPath $validatorDir -Force
$validator = Get-ChildItem $validatorDir -Filter pluginval.exe -Recurse | Select-Object -First 1
if (-not $validator) { throw "pluginval.exe missing" }
Invoke-Checked $validator.FullName @("--strictness-level", "5", "--skip-gui-tests", "--random-seed", "12345", "--validate", "$PSScriptRoot/build/VOXERA_artefacts/Release/VST3/VOXERA 1.0.vst3")
if ($Package) {
    Invoke-Checked $iscc @("Installer/VOXERA.iss")
    $installer = Get-Item "dist/VOXERA-0.8.0-Windows-x64-Setup.exe"
    (Get-FileHash $installer.FullName -Algorithm SHA256).Hash + "  " + $installer.Name |
        Set-Content "dist/SHA256SUMS.txt" -Encoding ASCII
    Write-Host "Installer created: $($installer.FullName)"
} else { Write-Host "Validated VST3: build/VOXERA_artefacts/Release/VST3/VOXERA 1.0.vst3" }
