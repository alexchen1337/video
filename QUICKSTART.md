# Quick Start Guide

Get up and running with Discord File Compressor in under 5 minutes.

## Windows Quick Start

### 1. Install Dependencies

```powershell
# Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install zlib
.\vcpkg install zlib:x64-windows

# Set environment variable (optional, for build script)
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
```

### 2. Build

```powershell
# Automated build
.\build.ps1 -Release

# Manual build
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake ..
cmake --build . --config Release
cd ..
```

### 3. Test

```powershell
# Run test suite
.\test_tool.ps1 -FileSizeMB 100

# Or manual test
.\build\Release\discord_compressor.exe -c test.mp4 chunks\
.\build\Release\discord_compressor.exe -d chunks\ restored.mp4
```

## Linux Quick Start

### 1. Install Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake zlib1g-dev

# Fedora/RHEL
sudo dnf install gcc-c++ cmake zlib-devel

# Arch Linux
sudo pacman -S base-devel cmake zlib
```

### 2. Build

```bash
# Using Make
make

# Or using CMake
mkdir build && cd build
cmake ..
make
cd ..
```

### 3. Test

```bash
# Create test file
dd if=/dev/urandom of=test.bin bs=1M count=100

# Compress
./discord_compressor -c test.bin chunks/

# Decompress
./discord_compressor -d chunks/ restored.bin

# Verify
sha256sum test.bin restored.bin
```

## macOS Quick Start

### 1. Install Dependencies

```bash
# Install Homebrew (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake zlib
```

### 2. Build

```bash
mkdir build && cd build
cmake ..
make
cd ..
```

### 3. Test

```bash
# Create test file
dd if=/dev/urandom of=test.bin bs=1m count=100

# Compress
./discord_compressor -c test.bin chunks/

# Decompress
./discord_compressor -d chunks/ restored.bin

# Verify
shasum -a 256 test.bin restored.bin
```

## Common Use Cases

### Scenario 1: Upload Large Video to Discord

```powershell
# Compress the video
discord_compressor -c my_video.mp4 video_chunks\

# Upload all .dchunk files from video_chunks\ to Discord
# (Manually drag and drop or use Discord bot)

# Download chunks from Discord to a folder

# Restore the video
discord_compressor -d downloaded_chunks\ restored_video.mp4
```

### Scenario 2: Backup Multiple Files

```powershell
# Compress each file
discord_compressor -c document1.pdf chunks_doc1\
discord_compressor -c document2.pdf chunks_doc2\
discord_compressor -c presentation.pptx chunks_pres\

# Upload all chunk directories to Discord

# Later, restore individual files
discord_compressor -d chunks_doc1\
discord_compressor -d chunks_doc2\
discord_compressor -d chunks_pres\
```

### Scenario 3: Batch Processing

**Windows:**
```powershell
# Compress all MP4 files in current directory
Get-ChildItem *.mp4 | ForEach-Object {
    $outDir = "chunks_$($_.BaseName)"
    .\discord_compressor.exe -c $_.Name $outDir
    Write-Host "Compressed: $($_.Name) -> $outDir"
}
```

**Linux/macOS:**
```bash
# Compress all MP4 files
for file in *.mp4; do
    outdir="chunks_${file%.*}"
    ./discord_compressor -c "$file" "$outdir"
    echo "Compressed: $file -> $outdir"
done
```

## Troubleshooting

### Build Issues

**Error: "zlib not found"**
```powershell
# Windows: Install via vcpkg
.\vcpkg install zlib:x64-windows

# Linux: Install dev package
sudo apt-get install zlib1g-dev

# macOS: Install via Homebrew
brew install zlib
```

**Error: "CMake version too old"**
```bash
# Download latest CMake
# Windows: https://cmake.org/download/
# Linux: sudo snap install cmake --classic
# macOS: brew upgrade cmake
```

### Runtime Issues

**Error: "Cannot open input file"**
- Check file path is correct
- Ensure file exists and is readable
- Use absolute paths if relative paths fail

**Error: "Invalid chunk file format"**
- Verify .dchunk files are not corrupted
- Re-download chunks from Discord
- Check all chunks are from the same file

**Error: "CRC32 checksum mismatch"**
- Chunk corrupted during transfer
- Re-download the specific chunk
- Verify file integrity after download

## Performance Tips

### 1. Adjust Chunk Size

Edit `discord_compressor.cpp`:
```cpp
constexpr size_t CHUNK_SIZE = 20 * 1024 * 1024;  // Change to desired size
```

Smaller chunks (10MB):
- More chunks to manage
- Better for unreliable connections
- More metadata overhead

Larger chunks (24MB):
- Fewer chunks
- Closer to Discord's limit
- Better compression ratio

### 2. Adjust Compression Level

Edit `discord_compressor.cpp`:
```cpp
constexpr int COMPRESSION_LEVEL = 6;  // 0-9
```

- Level 1-3: Faster, less compression
- Level 6: Balanced (default)
- Level 7-9: Slower, better compression

### 3. Adjust Buffer Size

Edit `discord_compressor.cpp`:
```cpp
constexpr size_t BUFFER_SIZE = 1024 * 1024;  // 1MB default
```

- HDD: 512KB - 1MB
- SSD: 1MB - 4MB
- NVMe: 4MB - 8MB

## Discord Upload Tips

### Free Tier (25MB Limit)

- Default 20MB chunks work perfectly
- Each chunk under limit with metadata
- Can upload ~100 chunks per channel

### Nitro (500MB Limit)

Increase chunk size for fewer uploads:
```cpp
constexpr size_t CHUNK_SIZE = 480 * 1024 * 1024;  // 480MB
```

### Best Practices

1. **Create dedicated channel** for file chunks
2. **Name chunks clearly** (already done by tool)
3. **Upload in order** (not required, but helpful)
4. **Verify all chunks uploaded** before deleting original
5. **Download to empty folder** to avoid mixing chunks

## File Size Estimates

### Compression Ratios by Type

| File Type | Original | Compressed | Chunks (25MB limit) |
|-----------|----------|------------|---------------------|
| 1GB Text | 1000 MB | 100 MB | 5 chunks |
| 1GB Video (raw) | 1000 MB | 250 MB | 13 chunks |
| 1GB Video (h264) | 1000 MB | 950 MB | 48 chunks |
| 1GB Images (PNG) | 1000 MB | 750 MB | 38 chunks |
| 1GB Archive (ZIP) | 1000 MB | 1000 MB | 50 chunks |

### Upload Time Estimates

At 10 Mbps upload speed:

| File Size | Compressed | Upload Time |
|-----------|------------|-------------|
| 100 MB | 30 MB | ~25 seconds |
| 500 MB | 150 MB | ~2 minutes |
| 1 GB | 300 MB | ~4 minutes |
| 5 GB | 1.5 GB | ~20 minutes |

## Next Steps

- Read full [README.md](README.md) for detailed documentation
- Check [TECHNICAL.md](TECHNICAL.md) for architecture details
- Modify constants for your specific use case
- Create automation scripts for frequent operations
- Consider adding encryption for sensitive files

## Support

Common issues and solutions:
1. **Build fails**: Check CMake/compiler versions
2. **Slow compression**: Adjust compression level down
3. **High memory usage**: Reduce number of threads or chunk size
4. **Corrupted output**: Verify original file not corrupted
5. **Missing chunks**: Ensure all .dchunk files downloaded

For detailed troubleshooting, see README.md.

