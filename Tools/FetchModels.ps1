# Busca capturas en TONE3000 y se queda solo con las que este plugin puede usar.
#
# La clave se lee del entorno y no se escribe en ningun sitio: ni en este fichero,
# ni en el repositorio, ni en la pantalla. Ponla en la sesion antes de ejecutar:
#
#     $env:TONE3000_API_KEY = "..."
#     .\Tools\FetchModels.ps1 -Query "preamp" -Wanted 5
#
# Si prefieres que sobreviva a reiniciar el PC, usa las variables de entorno de
# usuario de Windows en lugar de escribirla en un fichero del proyecto.

[CmdletBinding()]
param(
    [string] $Query = "preamp",
    [int]    $Wanted = 5,
    [int]    $Examine = 25,
    [string] $Destination = (Join-Path $env:USERPROFILE "Documents\VOXERA\Models")
)

$ErrorActionPreference = "Stop"

$key = $env:TONE3000_API_KEY
if ([string]::IsNullOrWhiteSpace($key)) {
    Write-Host "Falta TONE3000_API_KEY en el entorno." -ForegroundColor Red
    Write-Host 'Ponla con:  $env:TONE3000_API_KEY = "tu-clave"'
    exit 1
}

$base = "https://www.tone3000.com/api/v1"
$headers = @{ Authorization = "Bearer $key" }

New-Item -ItemType Directory -Force $Destination | Out-Null
$scratch = Join-Path $env:TEMP "voxera-models"
New-Item -ItemType Directory -Force $scratch | Out-Null

Write-Host "Buscando '$Query' en TONE3000..." -ForegroundColor Cyan

# La busqueda no puede filtrar por LSTM: el parametro 'architecture' de la API
# distingue A1 de A2, no la arquitectura de red. Asi que hay que traerse
# candidatos y mirar dentro de cada fichero, que es lo que hace el bucle de abajo.
$page = 1
$candidates = @()
while ($candidates.Count -lt $Examine -and $page -le 5) {
    $url = "$base/tones/search?query=$([uri]::EscapeDataString($Query))&format=nam&page=$page&page_size=25"
    try {
        $result = Invoke-RestMethod -Uri $url -Headers $headers -Method Get
    } catch {
        Write-Host "La busqueda ha fallado: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "Si es un 401, la clave no es valida o ha caducado."
        exit 1
    }
    $batch = @($result.tones)
    if ($batch.Count -eq 0) { break }
    $candidates += $batch
    $page++
}

Write-Host "$($candidates.Count) capturas candidatas." -ForegroundColor Cyan

$kept = 0
$rejected = @{}

foreach ($tone in $candidates) {
    if ($kept -ge $Wanted) { break }

    $models = @()
    try {
        $detail = Invoke-RestMethod -Uri "$base/tones/$($tone.id)" -Headers $headers -Method Get
        $models = @($detail.models)
    } catch { continue }

    foreach ($model in $models) {
        if ($kept -ge $Wanted) { break }
        if ([string]::IsNullOrWhiteSpace($model.model_url)) { continue }
        if ($model.name -notmatch '\.nam$' -and $model.model_url -notmatch '\.nam') { continue }

        $safe = ($tone.name + "_" + $model.name) -replace '[^\w\-\. ]', '_'
        if ($safe -notmatch '\.nam$') { $safe += ".nam" }
        $temp = Join-Path $scratch $safe

        try {
            Invoke-WebRequest -Uri $model.model_url -OutFile $temp -UseBasicParsing
        } catch { continue }

        # Lo que decide si sirve esta dentro del fichero, no en los metadatos.
        try {
            $nam = Get-Content -LiteralPath $temp -Raw | ConvertFrom-Json
        } catch {
            $rejected["JSON ilegible"] = 1 + $rejected["JSON ilegible"]
            Remove-Item -LiteralPath $temp -Force
            continue
        }

        $arch = "$($nam.architecture)"
        if ($arch -ne "LSTM") {
            # WaveNet se rechaza a proposito: cuesta mas CPU que todo el resto
            # de la cadena junta, y el motor PSOLA acaba de bajarla al 0,55%.
            $rejected[$arch] = 1 + $rejected[$arch]
            Remove-Item -LiteralPath $temp -Force
            continue
        }

        $layers = [int]$nam.config.num_layers
        $hidden = [int]$nam.config.hidden_size
        if ($layers -lt 1 -or $layers -gt 4 -or $hidden -lt 1 -or $hidden -gt 64) {
            $rejected["LSTM fuera de rango ($layers x $hidden)"] = 1 + $rejected["LSTM fuera de rango ($layers x $hidden)"]
            Remove-Item -LiteralPath $temp -Force
            continue
        }

        $final = Join-Path $Destination $safe
        Move-Item -LiteralPath $temp -Destination $final -Force
        $sizeKb = [math]::Round((Get-Item -LiteralPath $final).Length / 1KB, 1)
        Write-Host ("  GUARDADO  {0}  (LSTM {1}x{2}, {3} kB)" -f $safe, $layers, $hidden, $sizeKb) -ForegroundColor Green
        $kept++
    }
}

Write-Host ""
Write-Host "$kept guardadas en $Destination" -ForegroundColor Cyan
if ($rejected.Count -gt 0) {
    Write-Host "Descartadas:" -ForegroundColor DarkGray
    foreach ($reason in $rejected.Keys) {
        Write-Host ("  {0}: {1}" -f $reason, $rejected[$reason]) -ForegroundColor DarkGray
    }
}
Write-Host ""
Write-Host "En el plugin: LOAD NEURAL MODEL." -ForegroundColor Cyan
