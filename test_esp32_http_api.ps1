param(
    [string]$Esp32BaseUrl = $env:ESP32_BASE_URL,
    [switch]$ControlTest,
    [switch]$EnvironmentTest,
    [int]$TimeoutSec = 5
)

$ErrorActionPreference = "Stop"

function Normalize-BaseUrl {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        throw "ESP32 base URL is required. Example: -Esp32BaseUrl http://192.168.1.23:8080"
    }

    $normalized = $Value.Trim()
    if ($normalized -notmatch '^https?://') {
        $normalized = "http://$normalized"
    }
    return $normalized.TrimEnd("/")
}

function Invoke-Esp32Api {
    param(
        [string]$Method,
        [string]$Path,
        [object]$Body = $null
    )

    $uri = "$BaseUrl$Path"
    if ($null -eq $Body) {
        return Invoke-RestMethod -Method $Method -Uri $uri -TimeoutSec $TimeoutSec
    }

    $json = $Body | ConvertTo-Json -Compress
    return Invoke-RestMethod -Method $Method -Uri $uri -Body $json -ContentType "application/json" -TimeoutSec $TimeoutSec
}

function Show-StateSummary {
    param([object]$State)

    Write-Host "State summary:"
    Write-Host ("  purifier_level   : {0}" -f $State.purifier_level)
    Write-Host ("  fresh_air_level  : {0}" -f $State.fresh_air_level)
    Write-Host ("  humidifier_level : {0}" -f $State.humidifier_level)
    Write-Host ("  auto_mode        : {0}" -f $State.auto_mode)
    Write-Host ("  eco_mode         : {0}" -f $State.eco_mode)
    Write-Host ("  temperature_c    : {0}" -f $State.temperature_c)
    Write-Host ("  humidity_percent : {0}" -f $State.humidity_percent)
    Write-Host ("  air_score        : {0}" -f $State.air_score)
    Write-Host ("  comfort          : {0}" -f $State.comfort)
    Write-Host ("  advice           : {0}" -f $State.advice)
}

$BaseUrl = Normalize-BaseUrl $Esp32BaseUrl
$env:ESP32_BASE_URL = $BaseUrl

Write-Host "Testing ESP32 HTTP API: $BaseUrl"

$initialState = Invoke-Esp32Api -Method "GET" -Path "/api/state"
Write-Host "[OK] GET /api/state"
Show-StateSummary $initialState

$history = Invoke-Esp32Api -Method "GET" -Path "/api/history"
Write-Host ("[OK] GET /api/history count={0}" -f $history.count)

if ($ControlTest) {
    Write-Host "Running optional control test. This will briefly change actuator state."
    $restorePurifier = [int]$initialState.purifier_level
    $restoreFreshAir = [int]$initialState.fresh_air_level
    $restoreHumidifier = [int]$initialState.humidifier_level
    $restoreAuto = [bool]$initialState.auto_mode
    $restoreEco = [bool]$initialState.eco_mode

    Invoke-Esp32Api -Method "POST" -Path "/api/device" -Body @{ device = "purifier"; power = $true; level = 1 } | Out-Null
    Write-Host "[OK] POST /api/device purifier level=1"

    Invoke-Esp32Api -Method "POST" -Path "/api/device" -Body @{ device = "fresh_air"; power = $true; level = 1 } | Out-Null
    Write-Host "[OK] POST /api/device fresh_air level=1"

    Invoke-Esp32Api -Method "POST" -Path "/api/device" -Body @{ device = "humidifier"; power = $true; level = 1 } | Out-Null
    Write-Host "[OK] POST /api/device humidifier level=1"

    Invoke-Esp32Api -Method "POST" -Path "/api/device" -Body @{ device = "purifier"; power = ($restorePurifier -gt 0); level = $restorePurifier } | Out-Null
    Invoke-Esp32Api -Method "POST" -Path "/api/device" -Body @{ device = "fresh_air"; power = ($restoreFreshAir -gt 0); level = $restoreFreshAir } | Out-Null
    Invoke-Esp32Api -Method "POST" -Path "/api/device" -Body @{ device = "humidifier"; power = ($restoreHumidifier -gt 0); level = $restoreHumidifier } | Out-Null
    Invoke-Esp32Api -Method "POST" -Path "/api/mode" -Body @{ mode = "auto"; power = $restoreAuto } | Out-Null
    Invoke-Esp32Api -Method "POST" -Path "/api/mode" -Body @{ mode = "eco"; power = $restoreEco } | Out-Null
    Write-Host "[OK] Restored actuator and mode state from initial /api/state"
}

if ($EnvironmentTest) {
    Write-Host "Running optional environment test. This will enable manual environment mode briefly."
    $restoreManual = [bool]$initialState.manual_environment_mode

    Invoke-Esp32Api -Method "POST" -Path "/api/environment" -Body @{ enabled = $true; preset = "POLLUTED" } | Out-Null
    Write-Host "[OK] POST /api/environment preset=POLLUTED"

    if ($restoreManual) {
        Invoke-Esp32Api -Method "POST" -Path "/api/environment" -Body @{
            enabled = $true
            temperature_c = [double]$initialState.temperature_c
            humidity_percent = [double]$initialState.humidity_percent
            air_score = [int]$initialState.air_score
        } | Out-Null
        Write-Host "[OK] Restored manual environment values from initial /api/state"
    } else {
        Invoke-Esp32Api -Method "POST" -Path "/api/environment" -Body @{ enabled = $false } | Out-Null
        Write-Host "[OK] Disabled manual environment mode"
    }
}

$finalState = Invoke-Esp32Api -Method "GET" -Path "/api/state"
Write-Host "[OK] Final GET /api/state"
Show-StateSummary $finalState

Write-Host "ESP32 HTTP API check completed."
