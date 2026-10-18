# MoonLink Dual-Process E2E Test
# Starts Host in background, then runs Controller, verifies connection.

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

Write-Host "=== MoonLink Dual-Process E2E Test ===" -ForegroundColor Cyan
Write-Host ""

# Build both apps first.
Write-Host "Building host and controller..."
Push-Location $ProjectRoot
moon build cmd/host --target native 2>&1 | Out-Null
moon build cmd/controller --target native 2>&1 | Out-Null
Pop-Location

$HostExe = Join-Path $ProjectRoot "_build\native\debug\cmd\host\host.exe"
$ControllerExe = Join-Path $ProjectRoot "_build\native\debug\cmd\controller\controller.exe"

if (-not (Test-Path $HostExe)) {
    Write-Host "ERROR: Host executable not found at $HostExe" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $ControllerExe)) {
    Write-Host "ERROR: Controller executable not found at $ControllerExe" -ForegroundColor Red
    exit 1
}

# Start Host in background.
Write-Host "Starting Host..." -ForegroundColor Yellow
$HostProcess = Start-Process -FilePath $HostExe -PassThru -RedirectStandardOutput "$env:TEMP\moonlink_host_out.txt" -RedirectStandardError "$env:TEMP\moonlink_host_err.txt"
Start-Sleep -Seconds 2

if ($HostProcess.HasExited) {
    Write-Host "ERROR: Host exited prematurely" -ForegroundColor Red
    Get-Content "$env:TEMP\moonlink_host_out.txt"
    Get-Content "$env:TEMP\moonlink_host_err.txt"
    exit 1
}
Write-Host "Host running (PID $($HostProcess.Id))"

# Run Controller (will connect to host, exchange SDP, receive video).
Write-Host "Starting Controller..." -ForegroundColor Yellow
$ControllerOutput = & $ControllerExe 2>&1
$ControllerExitCode = $LASTEXITCODE

Write-Host ""
Write-Host "--- Controller Output ---" -ForegroundColor Cyan
$ControllerOutput | ForEach-Object { Write-Host $_ }
Write-Host "--- End Controller ---" -ForegroundColor Cyan

# Stop Host.
Write-Host ""
Write-Host "Stopping Host..." -ForegroundColor Yellow
if (-not $HostProcess.HasExited) {
    Stop-Process -Id $HostProcess.Id -Force
    Start-Sleep -Seconds 1
}
$HostOutput = Get-Content "$env:TEMP\moonlink_host_out.txt" -ErrorAction SilentlyContinue
Write-Host "--- Host Output ---" -ForegroundColor Cyan
$HostOutput | ForEach-Object { Write-Host $_ }
Write-Host "--- End Host ---" -ForegroundColor Cyan

# Evaluate results.
Write-Host ""
$Success = $true

if ($ControllerOutput -match "WebRTC connected") {
    Write-Host "[PASS] WebRTC connection established" -ForegroundColor Green
} else {
    Write-Host "[FAIL] WebRTC connection NOT established" -ForegroundColor Red
    $Success = $false
}

if ($ControllerOutput -match "Packets received: ([1-9]\d*)") {
    Write-Host "[PASS] Received $($Matches[1]) packets" -ForegroundColor Green
} else {
    Write-Host "[WARN] No packets received (may need more time)" -ForegroundColor Yellow
}

if ($ControllerOutput -match "Frames decoded: ([1-9]\d*)") {
    Write-Host "[PASS] Decoded $($Matches[1]) frames" -ForegroundColor Green
} else {
    Write-Host "[WARN] No frames decoded (decoder may need keyframe)" -ForegroundColor Yellow
}

if ($HostOutput -match "sent") {
    Write-Host "[PASS] Host sent data" -ForegroundColor Green
} else {
    Write-Host "[WARN] Host may not have sent data" -ForegroundColor Yellow
}

Write-Host ""
if ($Success) {
    Write-Host "=== DUAL-PROCESS TEST PASSED ===" -ForegroundColor Green
} else {
    Write-Host "=== DUAL-PROCESS TEST FAILED ===" -ForegroundColor Red
}

# Cleanup temp files.
Remove-Item "$env:TEMP\moonlink_host_out.txt" -ErrorAction SilentlyContinue
Remove-Item "$env:TEMP\moonlink_host_err.txt" -ErrorAction SilentlyContinue

exit $(if ($Success) { 0 } else { 1 })
