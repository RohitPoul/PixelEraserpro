# PixelEraser Pro

Professional background removal tool for Windows. Built with C++, Qt 6, and OpenCV.

![License](https://img.shields.io/badge/license-MIT-blue.svg)

## Features

- **Auto Color Remove** - Click to remove similar colors using LAB color space matching
- **Manual Eraser** - Paint to erase pixels with adjustable brush size
- **Repair Tool** - Restore original pixels
- **Unlimited Undo/Redo** - Full history support
- **Large Image Support** - Optimized for 4K+ images
- **Edge Softening** - Real-time preview
- **Multiple Backgrounds** - Dark, Light, AMOLED, White preview modes
- **Export to PNG** - With transparency

## Requirements

- Windows 10/11
- Qt 6.5+ (with MSVC 2022)
- OpenCV 4.x
- CMake 3.16+

## Building

### Prerequisites

1. Install [Qt 6](https://www.qt.io/download) with MSVC 2022 64-bit
2. Install OpenCV via vcpkg:
   ```
   vcpkg install opencv4:x64-windows
   ```

### Build Steps

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

Or open in Qt Creator and build.

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Open | Ctrl+O |
| Export | Ctrl+E |
| Undo | Ctrl+Z |
| Redo | Ctrl+Y |
| Zoom In | Ctrl++ |
| Zoom Out | Ctrl+- |
| Fit to Screen | Ctrl+0 |
| Auto Color Tool | A |
| Eraser Tool | E |
| Repair Tool | R |
| Brush Size | [ / ] |
| Pan | Space+Drag |
| Compare Original | H |
| Toggle Sidebar | Tab |

## License

MIT License - see [LICENSE](LICENSE) file.
