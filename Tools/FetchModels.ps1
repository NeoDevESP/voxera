# Busca capturas en TONE3000 y se queda solo con las que este plugin puede usar.
#
# La clave se lee del entorno y no se escribe en ningun sitio: ni en este fichero,
# ni en el repositorio, ni en la pantalla. Ponla en la sesion antes de ejecutar:
#
#     $env:TONE3000_API_KEY = "..."
#     .\Tools\FetchModels.ps1 -Query "preamp" -Wanted 5
#
# Con -Report no guarda nada: solo cuenta que arquitecturas hay, que es la forma
# barata de saber cuanto de lo que se busca es siquiera cargable.

[CmdletBinding()]
param(
    [string] $Query = "preamp",
    [int]    $Wanted = 5,
    [int]    $Tones = 8,
    # La API si sabe filtrar por tamano, aunque no por arquitectura. Nano y
    # feather son los WaveNet estrechos, que es lo que este plugin acepta.
    [string[]] $Sizes = @("nano", "feather"),
    [switch] $Report,
    [string] $Destination = (Join-Path $env:USERPROFILE "Documents\VOXERA\Models")
)

$ErrorActionPreference = "Stop"

$key = $env:TONE3000_API_KEY
if ([string]::IsNullOrWhiteSpace($key)) {
    Write-Host "Falta TONE3000_API_KEY en el entorno." -ForegroundColor Red
    Write-Host 'Ponla con:  $env:TONE3000_API_KEY = "tu-clave"'
    Write-Host "Usa la clave t3k_cs_...; la t3k_pub_ es publicable y da 401 aqui."
    exit 1
}

$base = "https://www.tone3000.com/api/v1"
# La descarga tambien la necesita: model_url apunta a la propia API, no a un CDN
# abierto, asi que pedirla sin cabecera devuelve un 401 en silencio.
$headers = @{ Authorization = "Bearer $key" }

New-Item -ItemType Directory -Force $Destination | Out-Null
$scratch = Join-Path $env:TEMP "voxera-models"
New-Item -ItemType Directory -Force $scratch | Out-Null

Write-Host "Buscando '$Query' en TONE3000..." -ForegroundColor Cyan

try {
    $search = Invoke-RestMethod -Headers $headers -Method Get `
        -Uri ("$base/tones/search?query=$([uri]::EscapeDataString($Query))&format=nam&page=1&page_size=25"
              + (($Sizes | ForEach-Object { "&sizes=$_" }) -join ""))
} catch {
    Write-Host "La busqueda ha fallado: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Un 401 aqui significa que la clave no vale para esta API."
    exit 1
}

# Las listas de esta API vienen siempre en 'data', con page/total al lado.
#
# La lista NO puede llamarse $tones: PowerShell no distingue mayusculas en los
# nombres de variable, asi que $tones y el parametro [int] $Tones son la misma,
# y asignarle un array a algo declarado entero revienta con un error de
# conversion que no menciona ninguna de las dos.
$candidates = @($search.data) | Select-Object -First $Tones
Write-Host "$($candidates.Count) capturas candidatas de $($search.total) encontradas." -ForegroundColor Cyan
Write-Host ""

$kept = 0
$seen = 0
$rejected = [ordered]@{}
$note = {
    param($reason)
    if ($rejected.Contains($reason)) { $rejected[$reason] = $rejected[$reason] + 1 }
    else { $rejected[$reason] = 1 }
}

foreach ($tone in $candidates) {
    if ($kept -ge $Wanted -and -not $Report) { break }

    # El detalle del tono no trae los modelos: hay que pedirlos aparte.
    try {
        $list = Invoke-RestMethod -Headers $headers -Method Get `
            -Uri "$base/models?tone_id=$($tone.id)&page=1&page_size=25"
    } catch {
        & $note "no se pudo listar los modelos"
        continue
    }

    $models = @($list.data)
    if ($models.Count -eq 0) { & $note "el tono no tiene modelos"; continue }

    Write-Host ("{0}  ({1} modelos)" -f $tone.title, $models.Count) -ForegroundColor DarkGray

    foreach ($model in $models) {
        if ($kept -ge $Wanted -and -not $Report) { break }
        if ([string]::IsNullOrWhiteSpace($model.model_url)) { & $note "sin model_url"; continue }

        $safe = ($tone.title + " - " + $model.name) -replace '[^\w\-\. ]', '_'
        if ($safe -notmatch '\.nam$') { $safe += ".nam" }
        $temp = Join-Path $scratch $safe

        try {
            Invoke-WebRequest -Uri $model.model_url -Headers $headers -OutFile $temp -UseBasicParsing
        } catch {
            & $note "descarga fallida"
            continue
        }
        $seen++

        # Lo que decide si sirve esta dentro del fichero, no en los metadatos:
        # la API no distingue LSTM de WaveNet en ningun campo.
        try {
            $nam = Get-Content -LiteralPath $temp -Raw | ConvertFrom-Json
        } catch {
            & $note "JSON ilegible"
            Remove-Item -LiteralPath $temp -Force
            continue
        }

        # Lo que decide es el ancho, no la arquitectura: el plugin carga
        # cualquier cosa que el core sepa leer, pero rechaza por encima de doce
        # canales porque a partir de ahi cuesta mas que el resto de la cadena.
        $arch = "$($nam.architecture)"
        $widest = 0
        foreach ($layer in @($nam.config.layers)) {
            if ($null -ne $layer.channels -and [int]$layer.channels -gt $widest) { $widest = [int]$layer.channels }
        }
        $label = if ($widest -gt 0) { "$arch $widest canales" } else { $arch }

        if ($widest -gt 12) {
            & $note ("$label (demasiado ancho)")
            Remove-Item -LiteralPath $temp -Force
            continue
        }

        if ($Report) {
            & $note ("$label (utilizable)")
            Remove-Item -LiteralPath $temp -Force
            continue
        }

        $final = Join-Path $Destination $safe
        Move-Item -LiteralPath $temp -Destination $final -Force
        $sizeKb = [math]::Round((Get-Item -LiteralPath $final).Length / 1KB, 1)
        Write-Host ("  GUARDADO  {0}  ({1}, {2} kB)" -f $safe, $label, $sizeKb) -ForegroundColor Green
        $kept++
    }
}

Write-Host ""
Write-Host "$seen ficheros examinados, $kept guardados en $Destination" -ForegroundColor Cyan
if ($rejected.Count -gt 0) {
    Write-Host "Reparto:" -ForegroundColor DarkGray
    foreach ($reason in $rejected.Keys) {
        Write-Host ("  {0}: {1}" -f $reason, $rejected[$reason]) -ForegroundColor DarkGray
    }
}
Write-Host ""
Write-Host "En el plugin: LOAD NEURAL MODEL." -ForegroundColor Cyan
