# Enseña la forma real de las respuestas de TONE3000, para dejar de adivinar
# nombres de campo. No descarga nada y no imprime la clave.
#
#     $env:TONE3000_API_KEY = "..."
#     .\Tools\ProbeApi.ps1

$ErrorActionPreference = "Stop"

$key = $env:TONE3000_API_KEY
if ([string]::IsNullOrWhiteSpace($key)) { Write-Host "Falta TONE3000_API_KEY." -ForegroundColor Red; exit 1 }

$base = "https://www.tone3000.com/api/v1"
$headers = @{ Authorization = "Bearer $key" }

Write-Host "=== GET /tones/search ===" -ForegroundColor Cyan
$search = Invoke-RestMethod -Uri "$base/tones/search?query=preamp&page=1&page_size=3" -Headers $headers
Write-Host "campos del nivel superior:" -ForegroundColor Yellow
$search.PSObject.Properties.Name -join ", "

# El array de resultados puede llamarse de varias formas; lo buscamos en vez de suponerlo.
$listName = $null
foreach ($property in $search.PSObject.Properties) {
    if ($property.Value -is [System.Array] -and $property.Value.Count -gt 0) { $listName = $property.Name; break }
}
Write-Host "array de resultados: '$listName'" -ForegroundColor Yellow
if (-not $listName) { Write-Host ($search | ConvertTo-Json -Depth 4); exit 0 }

$first = $search.$listName[0]
Write-Host "campos de un resultado:" -ForegroundColor Yellow
$first.PSObject.Properties.Name -join ", "
Write-Host "id=$($first.id)  name=$($first.name)"

Write-Host ""
Write-Host "=== GET /tones/$($first.id) ===" -ForegroundColor Cyan
$detail = Invoke-RestMethod -Uri "$base/tones/$($first.id)" -Headers $headers
Write-Host "campos del detalle:" -ForegroundColor Yellow
$detail.PSObject.Properties.Name -join ", "
Write-Host ""
Write-Host "detalle completo (recortado):" -ForegroundColor Yellow
$detail | ConvertTo-Json -Depth 4 | Select-Object -First 60

Write-Host ""
Write-Host "=== GET /models?tone_id=$($first.id) ===" -ForegroundColor Cyan
try {
    $models = Invoke-RestMethod -Uri "$base/models?tone_id=$($first.id)" -Headers $headers
    $models | ConvertTo-Json -Depth 4 | Select-Object -First 40
} catch {
    Write-Host "no disponible: $($_.Exception.Message)" -ForegroundColor DarkGray
}
