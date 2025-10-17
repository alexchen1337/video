# Build script for Windows
# Automatically detects vcpkg and builds the project

param(
    [switch]$Clean,
    [switch]$Release,
    [string]$VcpkgPath = $env:VCPKG_ROOT
)

$ErrorActionPreference = "Stop"

Write-Host "Discord File Compressor - Build Script" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

if ($Clean -and (Test-Path "build")) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location "build"

$cmakeArgs = @("..")
$buildConfig = "Debug"

if ($Release) {
    $buildConfig = "Release"
    Write-Host "Build configuration: Release" -ForegroundColor Green
} else {
    Write-Host "Build configuration: Debug" -ForegroundColor Yellow
}

if ($VcpkgPath) {
    $toolchainFile = Join-Path $VcpkgPath "scripts\buildsystems\vcpkg.cmake"
    if (Test-Path $toolchainFile) {
        Write-Host "Using vcpkg toolchain: $toolchainFile" -ForegroundColor Green
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
    } else {
        Write-Host "Warning: vcpkg toolchain not found at $toolchainFile" -ForegroundColor Yellow
        Write-Host "Continuing without vcpkg. Ensure zlib is installed system-wide." -ForegroundColor Yellow
    }
} else {
    Write-Host "Warning: VCPKG_ROOT not set. Install zlib manually or set vcpkg path." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Running CMake configuration..." -ForegroundColor Cyan
& cmake $cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Cyan
& cmake --build . --config $buildConfig

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Set-Location ..

$exePath = "build\$buildConfig\discord_compressor.exe"
if (-not (Test-Path $exePath)) {
    $exePath = "build\discord_compressor.exe"
}

if (Test-Path $exePath) {
    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "Executable: $exePath" -ForegroundColor Green
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Cyan
    Write-Host "  Compress:   $exePath -c <input_file> <output_dir>" -ForegroundColor White
    Write-Host "  Decompress: $exePath -d <chunks_dir> [output_file]" -ForegroundColor White
} else {
    Write-Host "Build completed but executable not found!" -ForegroundColor Red
    exit 1
}

