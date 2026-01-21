<div align="center">

# PixelEraser Pro

### Professional background removal and image editing tool with AI-powered upscaling

[![Qt](https://img.shields.io/badge/Qt-6.10.1-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![OpenCV](https://img.shields.io/badge/OpenCV-4.x-5C3EE8?style=for-the-badge&logo=opencv&logoColor=white)](https://opencv.org/)
[![ONNX Runtime](https://img.shields.io/badge/ONNX_Runtime-1.x-005CED?style=for-the-badge&logo=onnx&logoColor=white)](https://onnxruntime.ai/)
[![Windows](https://img.shields.io/badge/Windows-10/11-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)](LICENSE)

<br/>

**Intelligent background removal • Precise editing tools • AI upscaling**

*Professional-grade image editing with a DaVinci Resolve-inspired dark interface*

<br/>

[Download Latest Release](https://github.com/RohitPoul/PixelEraserPro/releases/latest) | [Report Bug](https://github.com/RohitPoul/PixelEraserPro/issues) | [Request Feature](https://github.com/RohitPoul/PixelEraserPro/issues)

</div>

---

## Table of Contents

- [For Users](#for-users)
- [For Developers](#for-developers)
- [Additional Info](#additional-info)

---

# For Users

## Table of Contents

1. [Features Overview](#1-features-overview)
2. [Getting Started](#2-getting-started)
3. [Tools & Features](#3-tools--features)
   - [Auto Color Remove](#auto-color-remove)
   - [Manual Eraser](#manual-eraser)
   - [Repair Tool](#repair-tool)
   - [AI Upscaling](#ai-upscaling)
4. [Navigation & Controls](#4-navigation--controls)
5. [Export Options](#5-export-options)
6. [Keyboard Shortcuts](#6-keyboard-shortcuts)
7. [Tips & Best Practices](#7-tips--best-practices)

---

## 1. Features Overview

PixelEraser Pro is a desktop application designed for professional background removal and image editing. Built with Qt and OpenCV, it offers powerful tools with an intuitive interface.

**Key Features:**

| Feature | Description |
|---------|-------------|
| **Auto Color Remove** | Intelligent background removal using LAB color space analysis |
| **Manual Eraser** | Precise control with adjustable brush size and edge softening |
| **Repair Tool** | Fix over-erased areas with smart restoration |
| **AI Upscaling** | 12 upscaling models including Real-ESRGAN and Real-CUGAN |
| **Undo/Redo** | Up to 10 levels of history for safe editing |
| **Zoom & Pan** | Smooth navigation with Space bar panning |
| **Compare Mode** | Press H to compare with original image |
| **Export Options** | PNG, JPEG, WebP with quality control |

---

## 2. Getting Started

**System Requirements:**

| Requirement | Specification |
|-------------|---------------|
| Operating System | Windows 10/11 64-bit |
| RAM | 4GB minimum, 8GB recommended |
| Graphics | OpenGL support |
| Disk Space | 500MB for installation |

**Installation:**

1. Download `PixelEraserPro_Setup_v1.0.0.exe` from [Releases](https://github.com/RohitPoul/PixelEraserPro/releases/latest)
2. Run the installer
3. Follow the installation wizard
4. Launch PixelEraser Pro from Start Menu or Desktop

**First Launch:**

1. Click **File → Open** (Ctrl+O) or drag and drop an image
2. Supported formats: PNG, JPEG, JPG, BMP, WebP
3. The image will load in the canvas
4. Start editing with the tools in the sidebar

---

## 3. Tools & Features

### Auto Color Remove

Intelligent background removal that analyzes colors in the LAB color space for accurate selection.

**How to use:**

1. Click the **Auto Color Remove** button or press **A**
2. Click on the background color you want to remove
3. Adjust tolerance (0-100) to control selection sensitivity
4. The tool removes similar colors in the visible viewport

**Settings:**

| Setting | Range | Description |
|---------|-------|-------------|
| Tolerance | 0-100 | Higher values remove more similar colors |
| Edge Softening | 0-10 | Smooths edges for natural results |

**Tips:**
- Start with tolerance around 50
- For complex backgrounds, use multiple clicks
- Zoom in for precise selection
- Only processes visible area when zoomed

---

### Manual Eraser

Precise eraser tool for manual background removal with adjustable brush size.

**How to use:**

1. Click the **Eraser** button or press **E**
2. Adjust brush size (1-200 pixels)
3. Click and drag to erase
4. Use edge softening for smooth transitions

**Settings:**

| Setting | Range | Description |
|---------|-------|-------------|
| Brush Size | 1-200 | Diameter of the eraser brush |
| Edge Softening | 0-10 | Feathers the brush edges |

**Tips:**
- Use smaller brushes for detailed work
- Increase edge softening for natural results
- Hold Space to pan while erasing
- Use Ctrl+Z to undo mistakes

---

### Repair Tool

Restore over-erased areas by bringing back the original image.

**How to use:**

1. Click the **Repair** button or press **R**
2. Adjust brush size
3. Paint over areas you want to restore
4. The original image will be revealed

**Use cases:**
- Fix accidentally erased areas
- Restore details after aggressive auto removal
- Blend edges between erased and kept areas

---

### AI Upscaling

Enhance image resolution using state-of-the-art AI models. Choose from 12 different models optimized for various image types.

**Available Models:**

| Category | Models | Best For |
|----------|--------|----------|
| **General Purpose** | Real-ESRGAN x4/x2 Plus | Photos and realistic images |
| **Anime & Illustrations** | Real-ESRGAN x4 Anime | Anime, manga, and drawn art |
| **Denoise** | Real-ESRGAN x4/x2 Denoise | Removing JPEG artifacts and noise |
| **Conservative** | Real-CUGAN 2x/3x/4x Conservative | Minimal changes, fewer artifacts |
| **No Denoise** | Real-CUGAN 2x/3x/4x No Denoise | Preserving film grain and texture |

**How to use:**

1. Open an unmodified image (upscaling only works on original images)
2. Go to **Image → Upscale** (Ctrl+Alt+U)
3. Select a model from the dropdown
4. Click **Download Model** if not already downloaded (~17MB)
5. Click **Upscale** and wait for processing
6. The upscaled image will replace the current image

**Model Selection Guide:**

| Image Type | Recommended Model |
|------------|-------------------|
| Photos | Real-ESRGAN x4 Plus |
| Compressed photos | Real-ESRGAN x4 Denoise |
| Anime/Manga | Real-ESRGAN x4 Anime |
| Film scans | Real-CUGAN 4x No Denoise |
| General upscaling | Real-CUGAN 4x Conservative |

**Notes:**
- Models are downloaded once and cached locally
- Processing time depends on image size
- Larger images are processed in tiles
- Upscaling only works on images without transparency

---

## 4. Navigation & Controls

**Zoom:**

| Action | Method |
|--------|--------|
| Zoom In | Mouse wheel up / Ctrl + Plus |
| Zoom Out | Mouse wheel down / Ctrl + Minus |
| Fit to Screen | Double-click canvas / Ctrl+0 |
| Actual Size | Ctrl+1 |

**Pan:**

| Action | Method |
|--------|--------|
| Pan | Hold Space + Drag |
| Pan (alternative) | Middle mouse button + Drag |

**View:**

| Action | Shortcut |
|--------|----------|
| Toggle Sidebar | Tab |
| Compare with Original | Hold H |
| Checkerboard Background | Automatic for transparency |

---

## 5. Export Options

Export your edited images in various formats with quality control.

**How to export:**

1. Click **File → Export** (Ctrl+E)
2. Choose format: PNG, JPEG, or WebP
3. Adjust quality settings
4. Select save location
5. Click **Export**

**Format Comparison:**

| Format | Transparency | Quality | File Size | Best For |
|--------|--------------|---------|-----------|----------|
| PNG | Yes | Lossless | Large | Final output, transparency needed |
| JPEG | No | Lossy | Small | Photos, no transparency |
| WebP | Yes | Lossy/Lossless | Medium | Web use, modern browsers |

**Quality Settings:**

| Format | Range | Recommendation |
|--------|-------|----------------|
| PNG | N/A | Always lossless |
| JPEG | 1-100 | 85-95 for high quality |
| WebP | 1-100 | 80-90 for balanced quality/size |

---

## 6. Keyboard Shortcuts

**File Operations:**

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open Image |
| Ctrl+E | Export |
| Ctrl+Q | Quit |

**Editing:**

| Shortcut | Action |
|----------|--------|
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| A | Auto Color Remove |
| E | Eraser Tool |
| R | Repair Tool |

**View:**

| Shortcut | Action |
|----------|--------|
| Space | Pan (hold and drag) |
| H | Compare with Original (hold) |
| Tab | Toggle Sidebar |
| Ctrl+0 | Fit to Screen |
| Ctrl+1 | Actual Size |
| Ctrl + Plus | Zoom In |
| Ctrl + Minus | Zoom Out |

**Image:**

| Shortcut | Action |
|----------|--------|
| Ctrl+Alt+R | Resize Image |
| Ctrl+Alt+U | Upscale Image |

**Help:**

| Shortcut | Action |
|----------|--------|
| F1 | Keyboard Shortcuts |

---

## 7. Tips & Best Practices

**For Best Results:**

| Tip | Why |
|-----|-----|
| Start with high-resolution images | Better quality after editing |
| Use Auto Color Remove first | Faster than manual erasing |
| Adjust tolerance gradually | Prevents over-removal |
| Enable edge softening | Creates natural-looking edges |
| Zoom in for details | More precise editing |
| Use Compare mode (H) | Check your progress |
| Save frequently | Use Export to save your work |

**Performance Tips:**

| Tip | Benefit |
|-----|---------|
| Close other applications | More RAM for processing |
| Work on smaller images first | Faster processing |
| Use Fit to Screen | Better overview |
| Limit undo history | Saves memory |

**Common Mistakes to Avoid:**

| Mistake | Solution |
|---------|----------|
| Too high tolerance | Start low, increase gradually |
| Forgetting to zoom | Zoom in for precise work |
| Not using edge softening | Enable for natural edges |
| Upscaling edited images | Upscale original images only |

---

# For Developers

This section is for developers who want to build from source or contribute to the project.

## Development Setup

**Prerequisites:**

| Requirement | Version |
|-------------|---------|
| Qt | 6.10.1 |
| CMake | 3.20+ |
| C++ Compiler | MSVC 2022 (Windows) |
| vcpkg | Latest |
| OpenCV | 4.x |
| ONNX Runtime | 1.x |

**Installation:**

1. Clone the repository:
```bash
git clone https://github.com/RohitPoul/PixelEraserPro.git
cd PixelEraserPro
```

2. Install dependencies via vcpkg:
```bash
vcpkg install opencv4:x64-windows
vcpkg install onnxruntime:x64-windows
```

3. Open in Qt Creator:
   - File → Open File or Project
   - Select `CMakeLists.txt`
   - Configure with MSVC 2022 64-bit kit
   - Build (Ctrl+B)

---

## Project Structure

```
PixelEraserPro/
├── include/                    # Header files
│   ├── MainWindow.h
│   ├── CanvasWidget.h
│   ├── ImageProcessor.h
│   ├── ToolManager.h
│   ├── HistoryManager.h
│   ├── Upscaler.h
│   ├── UpscaleDialog.h
│   ├── ExportDialog.h
│   ├── ResizeDialog.h
│   └── UpdateChecker.h
├── src/                        # Source files
│   ├── main.cpp
│   ├── MainWindow.cpp
│   ├── CanvasWidget.cpp
│   ├── ImageProcessor.cpp
│   ├── ToolManager.cpp
│   ├── HistoryManager.cpp
│   ├── Upscaler.cpp
│   ├── UpscaleDialog.cpp
│   ├── ExportDialog.cpp
│   ├── ResizeDialog.cpp
│   └── UpdateChecker.cpp
├── resources/                  # Resources
│   ├── icons/
│   │   ├── app-icon.ico
│   │   └── app-icon.png
│   ├── app.rc
│   └── resources.qrc
├── libs/                       # Third-party libraries
│   ├── stb_image.h
│   └── stb_image_write.h
├── build/                      # Build output
├── CMakeLists.txt             # CMake configuration
├── installer.iss              # Inno Setup script
├── deploy.ps1                 # Deployment script
└── README.md
```

---

## Building the Application

**Debug Build:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

**Release Build:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Using Qt Creator:**
1. Open project in Qt Creator
2. Select Release mode (bottom-left)
3. Build → Rebuild All (Ctrl+B)

---

## Creating a Release

**Automated Deployment:**

1. Build in Release mode
2. Run the deployment script:
```powershell
cd PixelEraserPro
.\deploy.ps1
```

This will:
- Create portable version in `../PixelEraserPro_Portable`
- Copy all required DLLs (Qt, OpenCV, ONNX Runtime)
- Create installer using Inno Setup
- Output: `../PixelEraserPro_Setup_v1.0.0.exe`

**Manual Deployment:**

1. Build in Release mode
2. Run `windeployqt` on the executable:
```bash
windeployqt.exe PixelEraserPro.exe
```
3. Copy OpenCV DLLs from vcpkg
4. Copy ONNX Runtime DLL from vcpkg
5. Create installer with Inno Setup

---

## Architecture

**Design Pattern:** Model-View-Controller (MVC)

| Component | Responsibility |
|-----------|----------------|
| **MainWindow** | Main UI, menu bar, toolbar, status bar |
| **CanvasWidget** | Image display, zoom, pan, tool interaction |
| **ImageProcessor** | Image manipulation, color removal, erasing |
| **ToolManager** | Tool state, brush settings, tool switching |
| **HistoryManager** | Undo/redo stack, state management |
| **Upscaler** | AI upscaling, ONNX Runtime integration |
| **UpdateChecker** | GitHub release checking, auto-updates |

**Key Technologies:**

| Technology | Purpose |
|------------|---------|
| Qt 6 | GUI framework, cross-platform support |
| OpenCV | Image processing, color space conversion |
| ONNX Runtime | AI model inference for upscaling |
| CMake | Build system |
| vcpkg | Dependency management |

---

## Contributing

Contributions are welcome! Here's how you can help:

**Bug Reports:**
1. Check if the issue already exists
2. Create a new issue with:
   - Clear description
   - Steps to reproduce
   - Expected vs actual behavior
   - Screenshots if applicable
   - System information

**Feature Requests:**
1. Describe the feature
2. Explain the use case
3. Provide examples if possible

**Pull Requests:**
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request with clear description

**Code Style:**
- Follow Qt coding conventions
- Use meaningful variable names
- Comment complex logic
- Keep functions focused and small

---

# Additional Info

## Technical Specifications

**Image Processing:**

| Feature | Implementation |
|---------|----------------|
| Color Space | LAB for color removal, RGB for display |
| Flood Fill | Custom implementation with tolerance |
| Edge Softening | Gaussian blur on alpha channel |
| Large Image Optimization | Lazy rendering for images >8.3MP |

**AI Upscaling:**

| Feature | Details |
|---------|---------|
| Framework | ONNX Runtime |
| Models | Real-ESRGAN, Real-CUGAN |
| Processing | Tile-based (512x512) with padding |
| Formats | ONNX format (.onnx files) |
| Cache Location | `%LOCALAPPDATA%\PixelEraserPro\models` |

**Performance:**

| Metric | Value |
|--------|-------|
| Max Undo Levels | 10 |
| Large Image Threshold | 8.3 megapixels (4K) |
| Tile Size (Upscaling) | 512x512 pixels |
| Tile Padding | 10 pixels |

---

## Supported Formats

**Input Formats:**

| Format | Extension | Notes |
|--------|-----------|-------|
| PNG | .png | Full support, transparency |
| JPEG | .jpg, .jpeg | No transparency |
| BMP | .bmp | Windows bitmap |
| WebP | .webp | Modern format |

**Export Formats:**

| Format | Extension | Transparency | Compression |
|--------|-----------|--------------|-------------|
| PNG | .png | Yes | Lossless |
| JPEG | .jpg | No | Lossy |
| WebP | .webp | Yes | Lossy/Lossless |

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

**Third-Party Libraries:**

| Library | Purpose | License |
|---------|---------|---------|
| Qt | GUI framework | LGPL v3 |
| OpenCV | Image processing | Apache 2.0 |
| ONNX Runtime | AI inference | MIT |
| stb_image | Image loading | Public Domain |
| Real-ESRGAN | Upscaling models | BSD-3-Clause |

**Special Thanks:**
- xinntao for Real-ESRGAN models
- bilibili for Real-CUGAN models
- Qt Company for the excellent framework
- OpenCV community for image processing tools

---

## Contact & Support

**Developer:** Rohit Poul

**Connect:**

| Platform | Link |
|----------|------|
| GitHub | [@RohitPoul](https://github.com/RohitPoul) |
| Email | [poulrohit258@gmail.com](mailto:poulrohit258@gmail.com) |

**Support:**

| Platform | Link |
|----------|------|
| GitHub Issues | [Report Bug](https://github.com/RohitPoul/PixelEraserPro/issues) |
| GitHub Discussions | [Ask Questions](https://github.com/RohitPoul/PixelEraserPro/discussions) |

**Other Projects:**
- [Telegram Sticker Maker](https://github.com/RohitPoul/Telegram-Sticker-Maker-And-Auto-Uploader)

---

<div align="center">

Made with ❤️ by Rohit Poul

⭐ Star this repo if you find it useful!

</div>
