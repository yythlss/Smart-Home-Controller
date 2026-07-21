param(
    [string]$McpEndpoint = $env:MCP_ENDPOINT,
    [string]$Esp32BaseUrl = $env:ESP32_BASE_URL,
    [string]$McpPipePath = "",
    [string]$Python = "python",
    [switch]$InstallDeps,
    [switch]$SkipHttpCheck
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

function Redact-McpEndpoint {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ""
    }
    return ($Value -replace 'token=[^&\s]+', 'token=***')
}

$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BridgeScript = Join-Path $ProjectRoot "tools\xiaozhi_mcp_bridge\smart_home_bridge.py"
$Requirements = Join-Path $ProjectRoot "tools\xiaozhi_mcp_bridge\requirements.txt"
$DefaultPipePath = Join-Path $ProjectRoot ".cache\mcp-calculator\mcp_pipe.py"
$HttpTestScript = Join-Path $ProjectRoot "scripts\test_esp32_http_api.ps1"

if (-not (Test-Path -LiteralPath $BridgeScript)) {
    throw "Bridge script not found: $BridgeScript"
}

if ([string]::IsNullOrWhiteSpace($McpEndpoint)) {
    throw "MCP_ENDPOINT is required. Set it with: `$env:MCP_ENDPOINT='wss://api.xiaozhi.me/mcp/?token=...'"
}

$BaseUrl = Normalize-BaseUrl $Esp32BaseUrl
$env:MCP_ENDPOINT = $McpEndpoint
$env:ESP32_BASE_URL = $BaseUrl

if ([string]::IsNullOrWhiteSpace($McpPipePath)) {
    $McpPipePath = $DefaultPipePath
}

if (-not (Test-Path -LiteralPath $McpPipePath)) {
    Write-Host "mcp_pipe.py not found: $McpPipePath"
    Write-Host "Prepare it with:"
    Write-Host "  git clone https://github.com/78/mcp-calculator .cache\mcp-calculator"
    throw "Missing mcp_pipe.py"
}

if ($InstallDeps) {
    Write-Host "Installing MCP bridge dependencies..."
    & $Python -m pip install -r $Requirements
}

if (-not $SkipHttpCheck) {
    Write-Host "Running ESP32 HTTP API pre-check..."
    & powershell -ExecutionPolicy Bypass -File $HttpTestScript -Esp32BaseUrl $BaseUrl
}

Write-Host "Starting Xiaozhi MCP bridge..."
Write-Host ("  MCP_ENDPOINT   : {0}" -f (Redact-McpEndpoint $McpEndpoint))
Write-Host ("  ESP32_BASE_URL : {0}" -f $BaseUrl)
Write-Host ("  mcp_pipe.py    : {0}" -f $McpPipePath)
Write-Host "Press Ctrl+C to stop."

& $Python $McpPipePath $BridgeScript
