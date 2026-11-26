<div align="center">

# PixelEraser Pro

**GPU-accelerated background removal, infrared alpha inspection, and batch cutout studio for Windows.**

[![Release](https://img.shields.io/github/v/release/RohitPoul/PixelEraserpro?style=flat-square&color=white&label=Release)](https://github.com/RohitPoul/PixelEraserpro/releases/latest)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Qt 6](https://img.shields.io/badge/Qt-6-41CD52?style=flat-square&logo=qt&logoColor=white)](https://www.qt.io/)
[![OpenCV 4](https://img.shields.io/badge/OpenCV-4-5C3EE8?style=flat-square&logo=opencv&logoColor=white)](https://opencv.org/)
[![ONNX Runtime](https://img.shields.io/badge/ONNX_Runtime-CUDA_%2F_CPU-005CED?style=flat-square&logo=onnx&logoColor=white)](https://onnxruntime.ai/)
[![Platform](https://img.shields.io/badge/Windows-10%2F11_x64-0078D6?style=flat-square&logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-2ea44f?style=flat-square)](LICENSE)

[Download Latest Release](https://github.com/RohitPoul/PixelEraserpro/releases/latest) · [Report Bug](https://github.com/RohitPoul/PixelEraserpro/issues) · [Feature Request](https://github.com/RohitPoul/PixelEraserpro/issues)

</div>

---

## Core Capabilities

| Area | Details |
| :--- | :--- |
| **G4 AI Background Remover** | ToonOut G4 ONNX (`1536px` native) with optional `1024` / `1536` / `2048px` SmartEdge tiling, overlap control (`10–75%`), H-flip TTA, and edge deep-clean. Runs on NVIDIA CUDA with automatic CPU fallback, real-time progress telemetry, Pause/Resume, and instant Cancel (`SetTerminate`). |
| **AI Upscaler** | Real-ESRGAN ONNX (`2x`, `4x`, `4x Anime`) with tiled inference, CUDA/CPU backend auto-selection, Pause/Resume, and instant Cancel. Models download on demand to `%LOCALAPPDATA%\PixelEraser Pro\models`. |
| **Infrared Dust Inspector** | Press `I` to expose alpha defects: neon pink detached specks (`1–120px`, including isolated `1×1` solid pixels) with crimson radar halos, red/amber translucent fringe (`alpha 1–230`), and cool dimmed gray for solid mask regions. |
| **1-Click Clean Infrared Dust** | Press `Shift+I` or click **Clean Infrared Dust** (below the Infrared swatch) to remove only the exact infrared-highlighted dust pixels without touching the solid mask. Undoable with `Ctrl+Z`. |
| **Precision Brush Engine** | Auto Color Remove (LAB flood fill, tolerance `0–255`), 1:1 point-to-point Eraser & Repair (`1–200px` floating-point radius), outside-frame brush entry along image edges, and edge softening (`0–5`). |
| **Self-Remembering PNGs** | Exported PNGs embed the original RGB pixels beneath the alpha channel. Reopening a cutout restores full **Repair (`R`)**, **Compare Original (`H`)**, and **Restore Full Original (`Ctrl+Alt+O`)** support. |
| **Mass Image Queue & Sync** | Multi-select open (`Ctrl+O`), folder import, and multi-file/folder drag-and-drop. Stores paths only — **only the active image stays in RAM**; modified images offload to a session temp folder on switch. Click **Sync** (`Ctrl+Shift+S`) to batch-save all edited images as **Export Copies** or **Overwrite Originals**. |
| **Save vs. Export & Settings** | **`Ctrl+S`** overwrites the original file in-place after a confirmation warning. **`Ctrl+E`** exports a copy using your **Settings (`Ctrl+,`)**: filename mode (`Original` / `Prefix` / `Suffix` / `Both`) and output format (`Keep Original` / `PNG` / `WebP` / `JPG`). |
| **Live Delta Patch Updater** | Checks GitHub Releases automatically and downloads **only the updated binary (~380 KB)** with live progress, MB/s speed, ETA, Markdown release notes, and automatic in-place restart. |
| **Liquid Glass Studio UI** | Frameless Windows 11 studio chrome, 6 dark glass themes (`Obsidian Smoke`, `Matte Onyx`, `Carbon Ash`, `Graphite Steel`, etc.), centered dialogs, and off-thread image decode/encode so the UI stays responsive on 4K–8K files. |

---

## Download & Install

Grab the latest build from [GitHub Releases](https://github.com/RohitPoul/PixelEraserpro/releases/latest):

| Package | Use Case |
| :--- | :--- |
| **`PixelEraserPro-Setup-<ver>.exe`** | Standard Windows 64-bit installer (Start Menu + Desktop shortcut). |
| **`PixelEraserPro-Portable-<ver>-win64.zip`** | Extract anywhere and run `PixelEraserPro.exe`. |
| **`PixelEraserPro-Patch-<ver>.zip`** | ~380 KB delta patch used automatically by the in-app updater (`v2.0.1+`). |

**Supported Formats:** Read & write `.png`, `.webp`, `.jpg`/`.jpeg`, `.tiff`/`.tif`, `.bmp`.

---

## Keyboard Shortcuts

| File & Queue | Shortcut | Tools & Canvas | Shortcut |
| :--- | :--- | :--- | :--- |
| Open / Add Images (Multi-Select) | `Ctrl+O` | Auto Color Remove / Eraser / Repair | `A` / `E` / `R` |
| Save Over Original (In-Place) | `Ctrl+S` | Smaller / Larger Brush | `[` / `]` |
| Sync / Save All Edited | `Ctrl+Shift+S` | Pan Canvas | `Space + Drag` / Middle Drag |
| Export Copy | `Ctrl+E` | Zoom In / Out | `=` / `-` or `Ctrl+` / `Ctrl-` |
| Remove Current / Remove All | `Ctrl+D` / `Ctrl+N` | Fit to Screen / Actual Size (100%) | `Ctrl+0` / `Ctrl+1` |
| Previous / Next Queued Image | `PageUp` / `PageDown` | Infrared Dust Scan / Clean IR Dust | `I` / `Shift+I` |
| Undo / Redo (up to 10 steps) | `Ctrl+Z` / `Ctrl+Y` | Compare Original / Toggle Sidebar | `H` / `Tab` |
| Settings | `Ctrl+,` | Shortcuts Reference | `F1` |

| AI Studio & Image | Shortcut |
| :--- | :--- |
| G4 AI Background Remover | `Ctrl+Alt+B` |
| Apply External Mask / Cutout | `Ctrl+Alt+M` |
| Restore Full Original (Self-Memory) | `Ctrl+Alt+O` |
| Real-ESRGAN AI Upscaler | `Ctrl+Alt+U` |
| Resize Canvas (Lanczos4) | `Ctrl+Alt+R` |

---

## Build from Source

**Requirements:** Windows 10/11 x64 · MSVC 2022+ · CMake 3.20+ · Qt 6 (`Core`, `Gui`, `Widgets`, `Network`, `Concurrent`) · vcpkg (`opencv4`, `onnxruntime`).

```powershell
git clone https://github.com/RohitPoul/PixelEraserpro.git
cd PixelEraserpro

# Install C++ dependencies via vcpkg
vcpkg install opencv4:x64-windows onnxruntime:x64-windows

# Configure & build Release (from x64 Native Tools Command Prompt)
cmake -G "NMake Makefiles JOM" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/msvc2022_64" `
  -S . -B build/Release

cmake --build build/Release -j 12
```

For CUDA acceleration, place the ONNX Runtime GPU DLLs (`onnxruntime.dll`, `onnxruntime_providers_shared.dll`, `onnxruntime_providers_cuda.dll`) alongside `PixelEraserPro.exe` with CUDA/cuDNN available on the system; the app auto-discovers CUDA paths at startup and falls back to CPU if unavailable.

To package the Windows installer, run Inno Setup 6 on [`installer/PixelEraserPro.iss`](installer/PixelEraserPro.iss).

---

## Project Structure

```text
PixelEraserpro/
├── CMakeLists.txt
├── include/
│   ├── MainWindow.h          # Studio window, mass image queue, sync/save/export
│   ├── CanvasWidget.h        # Viewport, mipmaps, 1:1 brush cursor, Infrared scanner
│   ├── ImageProcessor.h      # Off-thread decode/encode, LAB flood fill, brush ops
│   ├── BackgroundRemover.h   # ToonOut G4 ONNX tiled inference (CUDA/CPU)
│   ├── Upscaler.h            # Real-ESRGAN ONNX upscaler (CUDA/CPU)
│   ├── OnnxRuntimeHelper.h   # CUDA EP auto-discovery, arena shrinkage, SetTerminate
│   ├── HistoryManager.h      # Undo/redo stack (10 steps, 2 GB cap)
│   ├── ToolManager.h         # Active tool, tolerance, brush size & hardness
│   ├── GlassWidgets.h        # Liquid Glass components, 6 themes, centered modal base
│   ├── SettingsDialog.h      # Export naming (prefix/suffix) & format preferences
│   ├── UpdateChecker.h       # GitHub Releases API checker (patch/installer/portable)
│   ├── UpdateDialog.h        # In-app live delta patcher with speed, ETA & auto-restart
│   ├── RemoverDialog.h       # G4 remover configuration dialog
│   ├── UpscaleDialog.h       # Real-ESRGAN model selector & downloader
│   ├── ExportDialog.h        # Export preview dialog
│   ├── ResizeDialog.h        # Lanczos4 canvas resize dialog
│   └── Version.h.in          # CMake-generated version header
├── src/                      # Corresponding C++ implementations + main.cpp
├── installer/
│   └── PixelEraserPro.iss    # Inno Setup 6 Windows installer script
└── resources/                # App icons, Windows .rc, Qt .qrc
```

---

## License

MIT — see [LICENSE](LICENSE).
