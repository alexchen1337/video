# Test script for Discord Compressor
# Creates test files, compresses, and decompresses them

param(
    [int]$FileSizeMB = 50,
    [string]$ExePath = "build\Release\discord_compressor.exe"
)

$ErrorActionPreference = "Stop"

Write-Host "Discord Compressor Test Suite" -ForegroundColor Cyan
Write-Host "=============================" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $ExePath)) {
    $ExePath = "build\discord_compressor.exe"
    if (-not (Test-Path $ExePath)) {
        Write-Host "Error: Executable not found. Build the project first." -ForegroundColor Red
        Write-Host "Run: .\build.ps1 -Release" -ForegroundColor Yellow
        exit 1
    }
}

Write-Host "Using executable: $ExePath" -ForegroundColor Green
Write-Host ""

$testDir = "test_temp"
$chunksDir = "$testDir\chunks"
$outputFile = "$testDir\restored_test.dat"

if (Test-Path $testDir) {
    Remove-Item -Recurse -Force $testDir
}
New-Item -ItemType Directory -Path $testDir | Out-Null
New-Item -ItemType Directory -Path $chunksDir | Out-Null

Write-Host "Step 1: Creating test file ($FileSizeMB MB)..." -ForegroundColor Cyan
$testFile = "$testDir\test_file.dat"
$bytes = New-Object byte[] (1024 * 1024)
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$stream = [System.IO.File]::OpenWrite($testFile)

for ($i = 0; $i -lt $FileSizeMB; $i++) {
    $rng.GetBytes($bytes)
    $stream.Write($bytes, 0, $bytes.Length)
    Write-Progress -Activity "Generating test file" -Status "$i / $FileSizeMB MB" -PercentComplete (($i / $FileSizeMB) * 100)
}

$stream.Close()
Write-Progress -Activity "Generating test file" -Completed
$originalHash = (Get-FileHash $testFile -Algorithm SHA256).Hash
Write-Host "  Original file: $testFile" -ForegroundColor Green
Write-Host "  Size: $FileSizeMB MB" -ForegroundColor Green
Write-Host "  SHA256: $originalHash" -ForegroundColor Green
Write-Host ""

Write-Host "Step 2: Compressing file..." -ForegroundColor Cyan
$compressStart = Get-Date
& $ExePath -c $testFile $chunksDir
if ($LASTEXITCODE -ne 0) {
    Write-Host "Compression failed!" -ForegroundColor Red
    exit 1
}
$compressTime = (Get-Date) - $compressStart
Write-Host "  Compression time: $($compressTime.TotalSeconds.ToString('F2')) seconds" -ForegroundColor Green
Write-Host ""

$chunkFiles = Get-ChildItem "$chunksDir\*.dchunk"
Write-Host "  Created $($chunkFiles.Count) chunks" -ForegroundColor Green
$totalChunkSize = ($chunkFiles | Measure-Object -Property Length -Sum).Sum
$reductionPercent = ((($FileSizeMB * 1024 * 1024) - $totalChunkSize) / ($FileSizeMB * 1024 * 1024)) * 100
Write-Host "  Total chunk size: $([math]::Round($totalChunkSize / 1024 / 1024, 2)) MB" -ForegroundColor Green
Write-Host "  Size reduction: $($reductionPercent.ToString('F2'))%" -ForegroundColor Green
Write-Host ""

Write-Host "Step 3: Decompressing chunks..." -ForegroundColor Cyan
$decompressStart = Get-Date
& $ExePath -d $chunksDir $outputFile
if ($LASTEXITCODE -ne 0) {
    Write-Host "Decompression failed!" -ForegroundColor Red
    exit 1
}
$decompressTime = (Get-Date) - $decompressStart
Write-Host "  Decompression time: $($decompressTime.TotalSeconds.ToString('F2')) seconds" -ForegroundColor Green
Write-Host ""

Write-Host "Step 4: Verifying integrity..." -ForegroundColor Cyan
if (-not (Test-Path $outputFile)) {
    Write-Host "Error: Output file not created!" -ForegroundColor Red
    exit 1
}

$restoredHash = (Get-FileHash $outputFile -Algorithm SHA256).Hash
Write-Host "  Original SHA256:  $originalHash" -ForegroundColor White
Write-Host "  Restored SHA256:  $restoredHash" -ForegroundColor White

if ($originalHash -eq $restoredHash) {
    Write-Host ""
    Write-Host "SUCCESS! File integrity verified." -ForegroundColor Green
    Write-Host ""
    Write-Host "Performance Summary:" -ForegroundColor Cyan
    Write-Host "  Compression:   $($compressTime.TotalSeconds.ToString('F2'))s ($([math]::Round($FileSizeMB / $compressTime.TotalSeconds, 2)) MB/s)" -ForegroundColor White
    Write-Host "  Decompression: $($decompressTime.TotalSeconds.ToString('F2'))s ($([math]::Round($FileSizeMB / $decompressTime.TotalSeconds, 2)) MB/s)" -ForegroundColor White
    Write-Host "  Size reduction: $($reductionPercent.ToString('F2'))%" -ForegroundColor White
} else {
    Write-Host ""
    Write-Host "FAILURE! File integrity check failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Cleaning up test files..." -ForegroundColor Yellow
Remove-Item -Recurse -Force $testDir

Write-Host "Test completed successfully!" -ForegroundColor Green

